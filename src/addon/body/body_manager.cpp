

#include "body_manager.h"
#include <iostream>
#include "../world.h"
#include "../napi/napi_registry.h"
#include "../layers/layers_manager.h"

namespace JOLT {


  BodyManager::BodyManager(napi_env env) : _nenv(env) {}
  BodyManager::~BodyManager() {
    reset();
  }

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

  Body* BodyManager::getBodyI(uint32_t id) {
    auto it = _bodies.find(id);
    return it != _bodies.end() ? it->second : nullptr;
  }

  void BodyManager::update(int frames) {
    for (const auto pair: _bodies) {
      pair.second->update(frames);
    }
  }

  void BodyManager::reset() {
    std::vector<Body*> bodies;
    bodies.reserve(_bodies.size());
    for (auto& pair : _bodies) bodies.push_back(pair.second);
    destroyBodies(bodies);
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

  std::vector<uint8_t> BodyManager::snapshotBodies(std::vector<uint32_t> bids) {
    std::vector<uint8_t> buf;
    buf.reserve(bids.size() * 56);
    auto append = [&](const void *src, size_t n) {
      const uint8_t *p = reinterpret_cast<const uint8_t *>(src);
      buf.insert(buf.end(), p, p + n);
    };
    for (uint32_t rawId : bids) {
      JPH::BodyID bid(rawId);
      if (bid.IsInvalid() || !_bodyInterface->IsAdded(bid)) continue;
      JPH::RVec3 pos; JPH::Quat rot;
      _bodyInterface->GetPositionAndRotation(bid, pos, rot);
      JPH::Vec3 lv = _bodyInterface->GetLinearVelocity(bid);
      JPH::Vec3 av = _bodyInterface->GetAngularVelocity(bid);
      float px = (float)pos.GetX(), py = (float)pos.GetY(), pz = (float)pos.GetZ();
      float rx = rot.GetX(), ry = rot.GetY(), rz = rot.GetZ(), rw = rot.GetW();
      float lvx = lv.GetX(), lvy = lv.GetY(), lvz = lv.GetZ();
      float avx = av.GetX(), avy = av.GetY(), avz = av.GetZ();
      append(&rawId, 4); append(&px, 4); append(&py, 4); append(&pz, 4);
      append(&rx, 4);    append(&ry, 4); append(&rz, 4); append(&rw, 4);
      append(&lvx, 4);   append(&lvy, 4); append(&lvz, 4);
      append(&avx, 4);   append(&avy, 4); append(&avz, 4);
    }
    return buf;
  }

  void BodyManager::trackBody(uint32_t id, Body* body) {
    _bodies[id] = body;
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
    body->update(0);
    return body;
  }

  Box* BodyManager::createBox(const BoxShape &shape, const BodyCreationSettings &settings) {
    Box * body = _addBody(new Box(_nenv), new JPH::BoxShape(shape.halfExtent, shape.convexRadius), settings);
    _world->emit<Box* >("body-created", body);
    return body;
  }

  Sphere* BodyManager::createSphere(const SphereShape &shape, const BodyCreationSettings &settings) {
    Sphere * body = _addBody(new Sphere(_nenv), new JPH::SphereShape(shape.radius), settings);
    _world->emit<Sphere* >("body-created", body);
    return body;
  }

  Triangle* BodyManager::createTriangle(const TriangleShape &shape, const BodyCreationSettings &settings) {
    Triangle * body = _addBody(new Triangle(_nenv), new JPH::TriangleShape(shape.p1, shape.p2, shape.p3, shape.convexRadius), settings);
    _world->emit<Triangle* >("body-created", body);
    return body;
  }

  Capsule* BodyManager::createCapsule(const CapsuleShape &shape, const BodyCreationSettings &settings) {
    Capsule * body = _addBody(new Capsule(_nenv), new JPH::CapsuleShape(shape.inHalfHeight, shape.inRadius), settings);
    _world->emit<Capsule* >("body-created", body);
    return body;
  }

  // Box* BodyManager::createBox(const JPH::Vec3 &halfSize, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }
  // Sphere* BodyManager::createSphere(const float radius, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }
  // Triangle* BodyManager::createTriangle(const JPH::Vec3 &p1, const JPH::Vec3 &p2, const JPH::Vec3 p3, const float radius, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }
  // Capsule* BodyManager::createCapsule(const float halfHeight, const float radius, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }
  // ConvexHull* BodyManager::createConvexHull(const JPH::Vec3* points, int pointsNum, const float radius, const JPH::Vec3 &pos, const JPH::Quat &rot, bool activate, const BodyMotionType &motionType, const std::string &layer) { ... }

  void BodyManager::addBody(Body * body, bool active) {
    if (!body || !body->getJoltBody()) return;
    JPH::EActivation act = active ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
    _bodyInterface->AddBody(body->getJoltBody()->GetID(), act);
  }

  void BodyManager::addBodyI(uint32_t bid, bool active) {
    Body* body = getBody(JPH::BodyID(bid));
    if (!body) return;
    addBody(body, active);
  }

  void BodyManager::addBodies(std::vector<Body*> bodies, bool active) {
    if (bodies.empty()) return;
    JPH::EActivation act = active ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
    std::vector<JPH::BodyID> ids;
    ids.reserve(bodies.size());
    for (Body* body : bodies)
      if (body && body->getJoltBody())
        ids.push_back(body->getJoltBody()->GetID());
    if (ids.empty()) return;
    JPH::BodyInterface::AddState state = _bodyInterface->AddBodiesPrepare(ids.data(), (int)ids.size());
    _bodyInterface->AddBodiesFinalize(ids.data(), (int)ids.size(), state, act);
  }

  void BodyManager::addBodiesI(std::vector<uint32_t> bids, bool active) {
    if (bids.empty()) return;
    JPH::EActivation act = active ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
    std::vector<JPH::BodyID> ids;
    ids.reserve(bids.size());
    for (uint32_t bid : bids) {
      Body* body = getBody(JPH::BodyID(bid));
      if (body && body->getJoltBody())
        ids.push_back(body->getJoltBody()->GetID());
    }
    if (ids.empty()) return;
    JPH::BodyInterface::AddState state = _bodyInterface->AddBodiesPrepare(ids.data(), (int)ids.size());
    _bodyInterface->AddBodiesFinalize(ids.data(), (int)ids.size(), state, act);
  }

  void BodyManager::removeBody(Body * body) {
    if (!body || !body->getJoltBody()) return;
    JPH::BodyID jid = body->getJoltBody()->GetID();
    if (_bodyInterface->IsAdded(jid))
      _bodyInterface->RemoveBody(jid);
  }

  void BodyManager::removeBodyI(uint32_t bid) {
    Body* body = getBody(JPH::BodyID(bid));
    if (!body) return;
    removeBody(body);
  }

  void BodyManager::removeBodies(std::vector<Body*> bodies) {
    if (bodies.empty()) return;
    std::vector<JPH::BodyID> ids;
    ids.reserve(bodies.size());
    for (Body* body : bodies)
      if (body && body->getJoltBody() && _bodyInterface->IsAdded(body->getJoltBody()->GetID()))
        ids.push_back(body->getJoltBody()->GetID());
    if (!ids.empty())
      _bodyInterface->RemoveBodies(ids.data(), (int)ids.size());
  }

  void BodyManager::removeBodiesI(std::vector<uint32_t> bids) {
    if (bids.empty()) return;
    std::vector<JPH::BodyID> ids;
    ids.reserve(bids.size());
    for (uint32_t bid : bids) {
      Body* body = getBody(JPH::BodyID(bid));
      if (body && body->getJoltBody() && _bodyInterface->IsAdded(body->getJoltBody()->GetID()))
        ids.push_back(body->getJoltBody()->GetID());
    }
    if (!ids.empty())
      _bodyInterface->RemoveBodies(ids.data(), (int)ids.size());
  }

  void BodyManager::destroyBody(Body * body) {
    if (!body || !body->getJoltBody()) return;
    JPH::BodyID jid = body->getJoltBody()->GetID();
    if (_bodyInterface->IsAdded(jid))
      _bodyInterface->RemoveBody(jid);
    _bodyInterface->DestroyBody(jid);
    _bodies.erase(jid.GetIndexAndSequenceNumber());
    delete body;
  }

  void BodyManager::destroyBodyI(uint32_t bid) {
    Body* body = getBody(JPH::BodyID(bid));
    if (!body) return;
    destroyBody(body);
  }

  void BodyManager::destroyBodies(std::vector<Body*> bodies) {
    if (bodies.empty()) return;
    std::vector<JPH::BodyID> toRemove;
    std::vector<JPH::BodyID> toDestroy;
    std::vector<uint32_t>    mapKeys;
    toRemove.reserve(bodies.size());
    toDestroy.reserve(bodies.size());
    mapKeys.reserve(bodies.size());
    for (Body* body : bodies) {
      if (!body || !body->getJoltBody()) continue;
      JPH::BodyID jid = body->getJoltBody()->GetID();
      mapKeys.push_back(jid.GetIndexAndSequenceNumber());
      toDestroy.push_back(jid);
      if (_bodyInterface->IsAdded(jid))
        toRemove.push_back(jid);
    }
    if (!toRemove.empty())
      _bodyInterface->RemoveBodies(toRemove.data(), (int)toRemove.size());
    if (!toDestroy.empty())
      _bodyInterface->DestroyBodies(toDestroy.data(), (int)toDestroy.size());
    for (uint32_t key : mapKeys) _bodies.erase(key);
    for (Body* body : bodies) delete body;
  }

  void BodyManager::destroyBodiesI(std::vector<uint32_t> bids) {
    if (bids.empty()) return;
    std::vector<Body*> bodies;
    bodies.reserve(bids.size());
    for (uint32_t bid : bids) {
      Body* body = getBody(JPH::BodyID(bid));
      if (body) bodies.push_back(body);
    }
    destroyBodies(bodies);
  }

  void BodyManager::setBodyPositionI(uint32_t bid, JPH::Vec3 position) {
    Body * b = getBodyI(bid);
    if (b) b->setPosition(position);
  }


  void BodyManager::reshapeBodyToSphereI(uint32_t bid, const SphereShape &shape) {
    Sphere * body = getTBodyI<Sphere>(bid);
    if (!body) return;
    if (body->getType() != BodyShapeType::Sphere) return;
    JPH::RefConst<JPH::Shape> nextShape = new JPH::SphereShape(shape.radius);
    _bodyInterface->SetShape(body->GetID(), nextShape, true, JPH::EActivation::Activate);
    // body->forceUpdate();
    _world->emit<Sphere* >("body-reshape", body);
  }

}


static JOLT::AutoRegister _auto_reg_body_manager(JOLT::BodyManager::Init);
