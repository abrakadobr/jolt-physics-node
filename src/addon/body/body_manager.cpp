

#include "body_manager.h"
#include <iostream>
#include "../world.h"
#include "../napi/napi_registry.h"
#include "../layers/layers_manager.h"

namespace JOLT {


  BodyManager::BodyManager(napi_env env) : _nenv(env) {}
  BodyManager::~BodyManager() {}

  BodyMotionType BodyManager::motionTypeFromJolt(JPH::EMotionType mt) {
    if (mt == JPH::EMotionType::Kinematic) return BodyMotionType::Kinematic;
    if (mt == JPH::EMotionType::Dynamic) return BodyMotionType::Dynamic;
    return BodyMotionType::Static;
  }
  JPH::EMotionType BodyManager::motionTypeToJolt(BodyMotionType tp) {
    if (tp == BodyMotionType::Kinematic) return JPH::EMotionType::Kinematic;
    if (tp == BodyMotionType::Dynamic) return JPH::EMotionType::Dynamic;
    return JPH::EMotionType::Static;
  }

  void BodyManager::initialize(World * world) {
    _world = world;
    _physicsSystem = _world->joltPhysicsSystem();
    _bodyInterface = &_physicsSystem->GetBodyInterface();
  }

  Body* BodyManager::getBody(JPH::BodyID id) {
    auto it = _bodies.find(id.GetIndexAndSequenceNumber());
    return it != _bodies.end() ? it->second : nullptr;
  }

  void BodyManager::update(int frames) {
    for (const auto pair: _bodies) {
      pair.second->update(frames);
    }
  }
  // --- Serialization ---

  // Compact state snapshot (56 bytes/body).
  // Per body: bodyId(u32) posX posY posZ(f32×3) rotX rotY rotZ rotW(f32×4) lvX lvY lvZ(f32×3) avX avY avZ(f32×3)
  std::vector<uint8_t> BodyManager::snapshotState() {
    JPH::BodyIDVector bodyIds;
    _physicsSystem->GetBodies(bodyIds);
    // const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    std::vector<uint8_t> buf;
    buf.reserve(bodyIds.size() * 56);
    auto append = [&](const void *src, size_t n) {
      const uint8_t *p = reinterpret_cast<const uint8_t *>(src);
      buf.insert(buf.end(), p, p + n);
    };
    for (const JPH::BodyID &bid : bodyIds) {
      if (bid.IsInvalid()) continue;
      JPH::RVec3 pos; JPH::Quat rot;
      _bodyInterface->GetPositionAndRotation(bid, pos, rot);
      JPH::Vec3 lv = _bodyInterface->GetLinearVelocity(bid);
      JPH::Vec3 av = _bodyInterface->GetAngularVelocity(bid);
      uint32_t id = bid.GetIndexAndSequenceNumber();
      float px = (float)pos.GetX(), py = (float)pos.GetY(), pz = (float)pos.GetZ();
      float rx = rot.GetX(), ry = rot.GetY(), rz = rot.GetZ(), rw = rot.GetW();
      float lvx = lv.GetX(), lvy = lv.GetY(), lvz = lv.GetZ();
      float avx = av.GetX(), avy = av.GetY(), avz = av.GetZ();
      append(&id, 4); append(&px, 4); append(&py, 4); append(&pz, 4);
      append(&rx, 4); append(&ry, 4); append(&rz, 4); append(&rw, 4);
      append(&lvx, 4); append(&lvy, 4); append(&lvz, 4);
      append(&avx, 4); append(&avy, 4); append(&avz, 4);
    }
    return buf;
  }

  bool BodyManager::applySnapshot(std::vector<uint8_t> buf) {
    const uint8_t* data = buf.data();
    size_t len = buf.size();
    constexpr size_t stride = 56;
    if (len % stride != 0) return false;
    // BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    for (size_t off = 0; off < len; off += stride) {
      uint32_t id;
      float px, py, pz, rx, ry, rz, rw, lvx, lvy, lvz, avx, avy, avz;
      memcpy(&id,  data + off,      4);
      memcpy(&px,  data + off + 4,  4); memcpy(&py,  data + off + 8,  4); memcpy(&pz,  data + off + 12, 4);
      memcpy(&rx,  data + off + 16, 4); memcpy(&ry,  data + off + 20, 4); memcpy(&rz,  data + off + 24, 4); memcpy(&rw,  data + off + 28, 4);
      memcpy(&lvx, data + off + 32, 4); memcpy(&lvy, data + off + 36, 4); memcpy(&lvz, data + off + 40, 4);
      memcpy(&avx, data + off + 44, 4); memcpy(&avy, data + off + 48, 4); memcpy(&avz, data + off + 52, 4);
      JPH::BodyID bid(id);
      if (!_bodyInterface->IsAdded(bid)) continue;
      _bodyInterface->SetPositionAndRotation(bid, JPH::RVec3(px, py, pz), JPH::Quat(rx, ry, rz, rw), JPH::EActivation::DontActivate);
      _bodyInterface->SetLinearAndAngularVelocity(bid, JPH::Vec3(lvx, lvy, lvz), JPH::Vec3(avx, avy, avz));
    }
    return true;
  }


  template<class T>
  T* BodyManager::_addBody(T* body, JPH::Shape* shape, const BodyCreationSettings &settings) {
    Layer l = _world->layersManager()->layerByName(settings.layer);
    if (l.id == LayersManager::InvalidLayerID)
      l = _world->layersManager()->layerByName("static");
    JPH::EMotionType emt = motionTypeToJolt(settings.motionType);
    JPH::EActivation act = settings.active ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
    JPH::BodyCreationSettings bcs(shape, settings.position, settings.rotation, emt, l.objectLayer);
    JPH::Body* joltBody = _bodyInterface->CreateBody(bcs);
    body->setJoltBody(joltBody);
    body->setJoltBodyInterface(_bodyInterface);
    _bodies[body->id()] = body;
    _bodyInterface->AddBody(joltBody->GetID(), act);
    return body;
  }

  Box* BodyManager::createBox(const BoxShape &shape, const BodyCreationSettings &settings) {
    return _addBody(new Box(_nenv), new JPH::BoxShape(shape.halfExtent, shape.convexRadius), settings);
  }

  Sphere* BodyManager::createSphere(const SphereShape &shape, const BodyCreationSettings &settings) {
    return _addBody(new Sphere(_nenv), new JPH::SphereShape(shape.radius), settings);
  }

  Triangle* BodyManager::createTriangle(const TriangleShape &shape, const BodyCreationSettings &settings) {
    return _addBody(new Triangle(_nenv), new JPH::TriangleShape(shape.p1, shape.p2, shape.p3, shape.convexRadius), settings);
  }

  Capsule* BodyManager::createCapsule(const CapsuleShape &shape, const BodyCreationSettings &settings) {
    return _addBody(new Capsule(_nenv), new JPH::CapsuleShape(shape.inHalfHeight, shape.inRadius), settings);
  }

  // Box* BodyManager::createBox(const JPH::Vec3 &halfSize, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }
  // Sphere* BodyManager::createSphere(const float radius, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }
  // Triangle* BodyManager::createTriangle(const JPH::Vec3 &p1, const JPH::Vec3 &p2, const JPH::Vec3 p3, const float radius, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }
  // Capsule* BodyManager::createCapsule(const float halfHeight, const float radius, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }
  // ConvexHull* BodyManager::createConvexHull(const JPH::Vec3* points, int pointsNum, const float radius, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }

}


static JOLT::AutoRegister _auto_reg_world(JOLT::BodyManager::Init);
