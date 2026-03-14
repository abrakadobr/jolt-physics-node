#include "body_manager.h"
#include "world.h"
#include "napi/napi_registry.h"

namespace JOLT {


  BodyManager::BodyManager(napi_env env) : _nenv(env) {}
  BodyManager::~BodyManager() {}


  void BodyManager::initialize(World * world) {
    _world = world;
    _physicsSystem = _world->joltPhysicsSystem();
  }
  // --- Serialization ---

  // Compact state snapshot (56 bytes/body).
  // Per body: bodyId(u32) posX posY posZ(f32×3) rotX rotY rotZ rotW(f32×4) lvX lvY lvZ(f32×3) avX avY avZ(f32×3)
  std::vector<uint8_t> BodyManager::snapshotState() {
    /*
    BodyIDVector bodyIds;
    mPhysicsSystem.GetBodies(bodyIds);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    std::vector<uint8_t> buf;
    buf.reserve(bodyIds.size() * 56);
    auto append = [&](const void *src, size_t n) {
      const uint8_t *p = reinterpret_cast<const uint8_t *>(src);
      buf.insert(buf.end(), p, p + n);
    };
    for (const BodyID &bid : bodyIds) {
      if (bid.IsInvalid()) continue;
      RVec3 pos; Quat rot;
      bi.GetPositionAndRotation(bid, pos, rot);
      Vec3 lv = bi.GetLinearVelocity(bid);
      Vec3 av = bi.GetAngularVelocity(bid);
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
    */
    std::vector<uint8_t> ret;
    return ret;
  }

  bool BodyManager::applySnapshot(std::vector<uint8_t> buf) {
    /*
    const uint8_t* data = buf.data();
    size_t len = buf.size();
    constexpr size_t stride = 56;
    if (len % stride != 0) return false;
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    for (size_t off = 0; off < len; off += stride) {
      uint32_t id;
      float px, py, pz, rx, ry, rz, rw, lvx, lvy, lvz, avx, avy, avz;
      memcpy(&id,  data + off,      4);
      memcpy(&px,  data + off + 4,  4); memcpy(&py,  data + off + 8,  4); memcpy(&pz,  data + off + 12, 4);
      memcpy(&rx,  data + off + 16, 4); memcpy(&ry,  data + off + 20, 4); memcpy(&rz,  data + off + 24, 4); memcpy(&rw,  data + off + 28, 4);
      memcpy(&lvx, data + off + 32, 4); memcpy(&lvy, data + off + 36, 4); memcpy(&lvz, data + off + 40, 4);
      memcpy(&avx, data + off + 44, 4); memcpy(&avy, data + off + 48, 4); memcpy(&avz, data + off + 52, 4);
      BodyID bid(id);
      if (!bi.IsAdded(bid)) continue;
      bi.SetPositionAndRotation(bid, RVec3(px, py, pz), Quat(rx, ry, rz, rw), EActivation::DontActivate);
      bi.SetLinearAndAngularVelocity(bid, Vec3(lvx, lvy, lvz), Vec3(avx, avy, avz));
    }
    return true;
    */
    return false;
  }


}


static JOLT::AutoRegister _auto_reg_world(JOLT::BodyManager::Init);
