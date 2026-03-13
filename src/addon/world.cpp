#include "world.h"
#include "napi/napi_registry.h"

namespace JOLT {

  // napi_ref World::constructor;

  /*
  napi_value World::Init(napi_env env, napi_value exports)
  {
    napi_property_descriptor props[] = {
      // { "on", 0, METHOD(World,on), 0,0,0, napi_default, 0 },
        METHOD(World, on),
        // METHOD(World, emit),
        // METHOD(World,snapshotState),
        // METHOD(World,applySnapshot),
        // METHOD(World,saveScene),
        // METHOD(World,loadScene),
        // METHOD(World,setGravity)
    };

    napi_value cons;

    napi_define_class(
        env,
        "World",
        NAPI_AUTO_LENGTH,
        New,
        nullptr,
        sizeof(props)/sizeof(props[0]),
        props,
        &cons
    );

    napi_create_reference(env, cons, 1, &constructor);
    napi_set_named_property(env, exports, "World", cons);

    return exports;
  }

  napi_value World::New(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_value thisArg;

    napi_get_cb_info(env, info, &argc, args, &thisArg, nullptr);

    WorldSettings initData = JsConvert<WorldSettings>::from(env, args[0]);

    World * obj = new World(env, initData);

    napi_wrap(
        env,
        thisArg,
        obj,
        Destructor,
        nullptr,
        nullptr
    );

    return thisArg;
  }

  void World::Destructor(napi_env env, void* nativeObject, void*) {
    delete static_cast<World*>(nativeObject);
  }
  */

  World::World(napi_env env) : _nenv(env) {}

  void World::on(const std::string& event, JsCallback cb) {
    _callbacksMap[event].push_back(std::move(cb));
  }

  void World::initialize(WorldSettings s) {
    _settings = s;
    /*
    printf("settings mem: %d, bodies: %d, mutex: %d, pairs: %d, contacts: %d, gravity: %f",
      _settings.memoryPreallocatedMb,
      _settings.maxBodies,
      _settings.numBodyMutexes,
      _settings.maxBodiesPairs,
      _settings.maxContacts,
      _settings.gravity
    );
    */
    if (_settings.memoryPreallocatedMb < 1) _settings.memoryPreallocatedMb = 10;
    if (_settings.maxBodies < 1) _settings.maxBodies = 1024;
    if (_settings.maxBodiesPairs < 1) _settings.maxBodiesPairs = 1024;
    if (_settings.maxContacts < 1) _settings.maxContacts = 1024;
    /*
    printf("\n\nsettings mem: %d, bodies: %d, mutex: %d, pairs: %d, contacts: %d, gravity: %f",
      _settings.memoryPreallocatedMb,
      _settings.maxBodies,
      _settings.numBodyMutexes,
      _settings.maxBodiesPairs,
      _settings.maxContacts,
      _settings.gravity
    );
    */

    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    _tempAllocator = new JPH::TempAllocatorImpl(_settings.memoryPreallocatedMb * 1024 * 1024);
    // printf("allocator done");
    _jobSystem = new JPH::JobSystemThreadPool(
      JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, // comes from Jolt/Physics/PhysicsSettings.h
      std::max(1u, std::thread::hardware_concurrency() - 1)
    );
    // printf("jobsystem done");
    _layersManager = new LayersManager(_nenv);
    _layersManager->init();

    // printf("layers done");
    _physicsSystem = new JPH::PhysicsSystem();
    // printf("physics created");
    _physicsSystem->Init(
      _settings.maxBodies,
      _settings.numBodyMutexes,
      _settings.maxBodiesPairs,
      _settings.maxContacts,
      _layersManager->iBPLayerInterface,
      _layersManager->iObjectVsBroadPhaseLayerFilter,
      _layersManager->iObjectLayerPairFilter
    );
    // printf("physics init");
    _bodyActivationListner = new EngineBodyActivationListener();
    _bodyActivationListner->setWorld(this);
    _physicsSystem->SetBodyActivationListener(_bodyActivationListner);

    _contactListner = new EngineContactListener();
    _contactListner->setWorld(this);
    _physicsSystem->SetContactListener(_contactListner);
    /*
    mPhysicsSystem.SetBodyActivationListener(&mActivationListener);
    mPhysicsSystem.SetContactListener(&mContactListener);
    */

    setGravity(_settings.gravity);
    // CreateGround();
  }

  LayersManager * World::layersManager() {
    return _layersManager;
  }

  std::vector<std::string> World::layers() {
    std::vector<std::string> ret;
    std::vector<uint32_t> ids = _layersManager->layersIDs();
    for (auto lid: ids) {
      ret.push_back(_layersManager->layerName(lid));
    }
    return ret;
  }

  World::~World() {
    // _physicsSystem.SetBodyActivationListener(nullptr);
    // _physicsSystem.SetContactListener(nullptr);

    // Erase ragdoll constraint refs first so the standalone constraint loop below
    // won't try to RemoveConstraint on constraints already owned by ragdolls.
    /*
    for (auto &entry : mRagdollConstraintIds) {
      for (uint32_t cid : entry.second) {
        if (cid != 0) mConstraints.erase(cid);
      }
    }
    mRagdollConstraintIds.clear();

    for (auto &entry : mRagdolls) {
      entry.second->RemoveFromPhysicsSystem(true);
    }
    mRagdolls.clear();

    for (auto &entry : mConstraints) {
      mPhysicsSystem.RemoveConstraint(entry.second.GetPtr());
    }
    mConstraints.clear();

    // Release mutable compound shape refs explicitly before PhysicsSystem tears down.
    mMutableCompounds.clear();

    // CharacterVirtual instances hold a pointer to mPhysicsSystem; destroy before teardown.
    mCharacters.clear();
    if (mEnv != nullptr) {
      if (mBodyActivationCallbackRef != nullptr) napi_delete_reference(mEnv, mBodyActivationCallbackRef);
      if (mContactCallbackRef != nullptr) napi_delete_reference(mEnv, mContactCallbackRef);
      mEnv = nullptr;
    }
    */
    if (_physicsSystem) delete _physicsSystem;
    if (_contactListner)  delete _contactListner;
    if (_bodyActivationListner) delete _bodyActivationListner;
    if (_jobSystem) delete _jobSystem;
    if (_tempAllocator) delete _tempAllocator;
    if (_layersManager) delete _layersManager;
  }

  // REGISTER_CLASS(World)

  /*
  bool SetCallbackRef(napi_value cb_or_null, napi_ref &slot) {
    napi_valuetype type = napi_undefined;
    if (napi_typeof(mEnv, cb_or_null, &type) != napi_ok) return false;

    if (type == napi_null || type == napi_undefined) {
      if (slot != nullptr) {
        napi_delete_reference(mEnv, slot);
        slot = nullptr;
      }
      return true;
    }

    if (type != napi_function) return false;

    if (slot != nullptr) {
      napi_delete_reference(mEnv, slot);
      slot = nullptr;
    }

    return napi_create_reference(mEnv, cb_or_null, 1, &slot) == napi_ok;
  }

  void SetEventType(napi_value payload, PendingEventType type) const {
    const char *name = "unknown";
    switch (type) {
      case PendingEventType::BodyActivated:
        name = "activated";
        break;
      case PendingEventType::BodyDeactivated:
        name = "deactivated";
        break;
      case PendingEventType::ContactAdded:
        name = "added";
        break;
      case PendingEventType::ContactPersisted:
        name = "persisted";
        break;
      case PendingEventType::ContactRemoved:
        name = "removed";
        break;
    }
    napi_value v;
    napi_create_string_utf8(mEnv, name, NAPI_AUTO_LENGTH, &v);
    napi_set_named_property(mEnv, payload, "type", v);
  }

  void QueueBodyActivation(bool activated, const BodyID &body_id, uint64_t user_data) {
    std::lock_guard<std::mutex> lock(mPendingEventsMutex);
    PendingEvent ev;
    ev.type = activated ? PendingEventType::BodyActivated : PendingEventType::BodyDeactivated;
    ev.body_a = body_id.GetIndexAndSequenceNumber();
    ev.user_data = user_data;
    mPendingEvents.push_back(ev);
  }

  void QueueContactEvent(PendingEventType type, const BodyID &a, const BodyID &b, const ContactManifold &manifold) {
    PendingEvent ev;
    ev.type = type;
    ev.body_a = a.GetIndexAndSequenceNumber();
    ev.body_b = b.GetIndexAndSequenceNumber();
    ev.normal = manifold.mWorldSpaceNormal;
    ev.penetration_depth = manifold.mPenetrationDepth;
    if (!manifold.mRelativeContactPointsOn1.empty()) {
      ev.point = manifold.GetWorldSpaceContactPointOn1(0);
    }
    std::lock_guard<std::mutex> lock(mPendingEventsMutex);
    mPendingEvents.push_back(ev);
  }

  void QueueContactRemoved(const BodyID &a, const BodyID &b) {
    std::lock_guard<std::mutex> lock(mPendingEventsMutex);
    PendingEvent ev;
    ev.type = PendingEventType::ContactRemoved;
    ev.body_a = a.GetIndexAndSequenceNumber();
    ev.body_b = b.GetIndexAndSequenceNumber();
    mPendingEvents.push_back(ev);
  }
*/

  void World::setGravity(float gravity) {
    // _physicsSystem.setGravity(Vec3(0.0f, -gravity, 0.0f));
  }

  // void Step(float dt) { mPhysicsSystem.Update(dt, 1, &mTempAllocator, &mJobSystem); }

/*
  bool SetBodyActivationCallback(napi_value cb_or_null) {
    return SetCallbackRef(cb_or_null, mBodyActivationCallbackRef);
  }

  bool SetContactCallback(napi_value cb_or_null) {
    return SetCallbackRef(cb_or_null, mContactCallbackRef);
  }

  void DispatchCallbacks() {
    if (mEnv == nullptr) return;
    std::vector<PendingEvent> events;
    {
      std::lock_guard<std::mutex> lock(mPendingEventsMutex);
      if (mPendingEvents.empty()) return;
      events.swap(mPendingEvents);
    }

    napi_handle_scope scope;
    if (napi_open_handle_scope(mEnv, &scope) != napi_ok) return;

    napi_value global;
    napi_get_global(mEnv, &global);
    napi_value activation_cb = nullptr;
    napi_value contact_cb = nullptr;
    if (mBodyActivationCallbackRef != nullptr) napi_get_reference_value(mEnv, mBodyActivationCallbackRef, &activation_cb);
    if (mContactCallbackRef != nullptr) napi_get_reference_value(mEnv, mContactCallbackRef, &contact_cb);

    for (const PendingEvent &event : events) {
      napi_value cb = nullptr;
      switch (event.type) {
        case PendingEventType::BodyActivated:
        case PendingEventType::BodyDeactivated:
          cb = activation_cb;
          break;
        case PendingEventType::ContactAdded:
        case PendingEventType::ContactPersisted:
        case PendingEventType::ContactRemoved:
          cb = contact_cb;
          break;
      }
      if (cb == nullptr) continue;
      // Guard against stale refs (callback replaced or world partially torn down)
      napi_valuetype cb_type = napi_undefined;
      if (napi_typeof(mEnv, cb, &cb_type) != napi_ok || cb_type != napi_function) continue;

      napi_value payload;
      if (napi_create_object(mEnv, &payload) != napi_ok) continue;
      SetEventType(payload, event.type);
      SetUInt32(payload, "bodyA", event.body_a);
      SetUInt32(payload, "bodyB", event.body_b);
      if (event.type == PendingEventType::BodyActivated || event.type == PendingEventType::BodyDeactivated) {
        napi_value user_data;
        napi_create_double(mEnv, static_cast<double>(event.user_data), &user_data);
        napi_set_named_property(mEnv, payload, "userData", user_data);
      } else if (event.type != PendingEventType::ContactRemoved) {
        napi_set_named_property(mEnv, payload, "point", MakeVec3Object(mEnv, event.point));
        napi_set_named_property(mEnv, payload, "normal", MakeVec3Object(mEnv, RVec3(event.normal.GetX(), event.normal.GetY(), event.normal.GetZ())));
        napi_value penetration;
        napi_create_double(mEnv, event.penetration_depth, &penetration);
        napi_set_named_property(mEnv, payload, "penetrationDepth", penetration);
      }

      napi_call_function(mEnv, global, cb, 1, &payload, nullptr);
    }

    napi_close_handle_scope(mEnv, scope);
  }
  */



  // --- Serialization ---

  // Compact state snapshot (56 bytes/body).
  // Per body: bodyId(u32) posX posY posZ(f32×3) rotX rotY rotZ rotW(f32×4) lvX lvY lvZ(f32×3) avX avY avZ(f32×3)
  std::vector<uint8_t> World::snapshotState() {
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

  bool World::applySnapshot(std::vector<uint8_t> buf) {
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

  // Full scene save using Jolt PhysicsScene (current pos/rot/vel + shapes).
  // Note: custom constraints from mConstraints are NOT included.
  std::vector<uint8_t> World::saveScene() {
    /*
    PhysicsScene scene;
    scene.FromPhysicsSystem(&mPhysicsSystem);
    std::ostringstream oss(std::ios::binary);
    StreamOutWrapper stream(oss);
    scene.SaveBinaryState(stream, /inSaveShapes=/true, /inSaveGroupFilter=/true);
    scene.SaveBinaryState(stream, true, true);
    const std::string &str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
    */
    std::vector<uint8_t> ret;
    return ret;
  }

  // Restore bodies from saved binary scene data. Returns body count created, -1 on error.
  int32_t World::loadScene(std::vector<uint8_t> buf) {
    /*
    std::string str(reinterpret_cast<const char *>(buf.data()), buf.size());
    std::istringstream iss(str, std::ios::binary);
    StreamInWrapper stream(iss);
    auto result = PhysicsScene::sRestoreFromBinaryState(stream);
    if (result.HasError()) return -1;
    Ref<PhysicsScene> scene = result.Get();
    if (!scene->CreateBodies(&mPhysicsSystem)) return -1;
    return (int32_t)scene->GetNumBodies();
    */
    return 0;
  }

  // DebugGeoResult World::GetDebugGeometry(bool draw_bodies, bool draw_constraints,
  //                                 bool draw_constraint_limits, bool wireframe) {
    /*
    CollectingDebugRenderer r;
    if (draw_bodies) {
      BodyManager::DrawSettings s;
      s.mDrawShape = true;
      s.mDrawShapeWireframe = wireframe;
      mPhysicsSystem.DrawBodies(s, &r);
    }
    if (draw_constraints) mPhysicsSystem.DrawConstraints(&r);
    if (draw_constraint_limits) mPhysicsSystem.DrawConstraintLimits(&r);
    DebugGeoResult out;
    for (const auto &l : r.lines) {
      out.linePos.insert(out.linePos.end(), {l.x1,l.y1,l.z1,l.x2,l.y2,l.z2});
      out.lineCol.push_back(l.color);
    }
    for (const auto &t : r.tris) {
      out.triPos.insert(out.triPos.end(), {t.x1,t.y1,t.z1,t.x2,t.y2,t.z2,t.x3,t.y3,t.z3});
      out.triCol.push_back(t.color);
    }
    return out;
    */
  //  DebugGeoResult out;
  //  return out;
  //}

  // REGISTER_CLASS(World)

  //REGISTER_CLASS(World)
}

 // namespace JOLT {
// REGISTER_CLASS(Jolt::World)
// } 
static JOLT::AutoRegister _auto_reg_world(JOLT::World::Init);
