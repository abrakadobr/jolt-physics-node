
struct WorldHandle {
  PhysicsWorld *world = nullptr;
};



class PhysicsWorld {
 public:
   /*
  enum class PendingEventType { BodyActivated, BodyDeactivated, ContactAdded, ContactPersisted, ContactRemoved };

  struct PendingEvent {
    PendingEventType type;
    uint32_t body_a = 0;
    uint32_t body_b = 0;
    uint64_t user_data = 0;
    RVec3 point = RVec3::sZero();
    Vec3 normal = Vec3::sZero();
    float penetration_depth = 0.0f;
  };

  explicit PhysicsWorld(napi_env env, float gravity)
      : mTempAllocator(10 * 1024 * 1024),
        mJobSystem(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::max(1u, std::thread::hardware_concurrency() - 1)),
        mActivationListener(this),
        mContactListener(this),
        mEnv(env) {
    constexpr uint cMaxBodies = 65536;
    constexpr uint cNumBodyMutexes = 0;
    constexpr uint cMaxBodyPairs = 65536;
    constexpr uint cMaxContactConstraints = 10240;

    mPhysicsSystem.Init(
        cMaxBodies,
        cNumBodyMutexes,
        cMaxBodyPairs,
        cMaxContactConstraints,
        mBroadPhaseLayerInterface,
        mObjectVsBroadPhaseLayerFilter,
        mObjectLayerPairFilter);
    mPhysicsSystem.SetBodyActivationListener(&mActivationListener);
    mPhysicsSystem.SetContactListener(&mContactListener);

    SetGravity(gravity);
    CreateGround();
  }

  ~PhysicsWorld() {
    mPhysicsSystem.SetBodyActivationListener(nullptr);
    mPhysicsSystem.SetContactListener(nullptr);

    // Erase ragdoll constraint refs first so the standalone constraint loop below
    // won't try to RemoveConstraint on constraints already owned by ragdolls.
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
  }

  void SetGravity(float gravity) { mPhysicsSystem.SetGravity(Vec3(0.0f, -gravity, 0.0f)); }

  void Step(float dt) { mPhysicsSystem.Update(dt, 1, &mTempAllocator, &mJobSystem); }

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

  /*
  uint32_t CreateSphere(float radius, double x, double y, double z, bool dynamic, float restitution, float friction) {
    SphereShapeSettings settings(radius);
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, dynamic, restitution, friction);
  }

  uint32_t CreateBox(float hx, float hy, float hz, double x, double y, double z, bool dynamic, float restitution, float friction) {
    BoxShapeSettings settings(Vec3(hx, hy, hz));
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, dynamic, restitution, friction);
  }

  uint32_t CreateCapsule(float half_height, float radius, double x, double y, double z, bool dynamic, float restitution, float friction) {
    CapsuleShapeSettings settings(half_height, radius);
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, dynamic, restitution, friction);
  }

  uint32_t CreateCylinder(float half_height, float radius, double x, double y, double z, bool dynamic, float restitution, float friction) {
    CylinderShapeSettings settings(half_height, radius);
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, dynamic, restitution, friction);
  }

  uint32_t CreateTaperedCapsule(
      float half_height,
      float top_radius,
      float bottom_radius,
      double x,
      double y,
      double z,
      bool dynamic,
      float restitution,
      float friction) {
    TaperedCapsuleShapeSettings settings(half_height, top_radius, bottom_radius);
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, dynamic, restitution, friction);
  }

  uint32_t CreateTaperedCylinder(
      float half_height,
      float top_radius,
      float bottom_radius,
      double x,
      double y,
      double z,
      bool dynamic,
      float restitution,
      float friction) {
    TaperedCylinderShapeSettings settings(half_height, top_radius, bottom_radius);
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, dynamic, restitution, friction);
  }

  uint32_t CreateConvexHull(const std::vector<Vec3> &points, double x, double y, double z, bool dynamic, float restitution, float friction) {
    ConvexHullShapeSettings settings(points.data(), static_cast<int>(points.size()));
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, dynamic, restitution, friction);
  }

  uint32_t CreateMesh(const std::vector<Float3> &vertices, const std::vector<IndexedTriangle> &tris,
                       const PhysicsMaterialList &materials,
                       double x, double y, double z, float friction, float restitution) {
    VertexList jverts;
    jverts.reserve(vertices.size());
    for (const Float3 &v : vertices) jverts.push_back(v);

    IndexedTriangleList jtris;
    jtris.reserve(tris.size());
    for (const IndexedTriangle &t : tris) jtris.push_back(t);

    MeshShapeSettings settings(std::move(jverts), std::move(jtris));
    if (!materials.empty()) settings.mMaterials = materials;
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, false, restitution, friction);
  }

  uint32_t CreateHeightField(const std::vector<float> &samples, uint32_t sample_count,
                               Vec3Arg offset, Vec3Arg scale,
                               const std::vector<uint8_t> &mat_indices,
                               const PhysicsMaterialList &materials,
                               double x, double y, double z,
                               float friction, float restitution) {
    if (samples.size() < static_cast<size_t>(sample_count) * sample_count) return BodyID::cInvalidBodyID;
    HeightFieldShapeSettings settings(samples.data(), offset, scale, sample_count);
    if (!mat_indices.empty())
      settings.mMaterialIndices = Array<uint8_t>(mat_indices.data(), mat_indices.data() + mat_indices.size());
    if (!materials.empty()) settings.mMaterials = materials;
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, false, restitution, friction);
  }

  uint32_t CreateStaticCompound(const std::vector<SubShapeSpec> &subs,
                                 double x, double y, double z,
                                 bool dynamic, float friction, float restitution) {
    StaticCompoundShapeSettings settings;
    for (const auto &s : subs) settings.AddShape(s.pos, s.rot, s.shape);
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    return CreateBodyFromShape(result.Get(), x, y, z, dynamic, restitution, friction);
  }

  uint32_t CreateMutableCompound(const std::vector<SubShapeSpec> &subs,
                                  double x, double y, double z,
                                  bool dynamic, float friction, float restitution) {
    MutableCompoundShapeSettings settings;
    for (const auto &s : subs) settings.AddShape(s.pos, s.rot, s.shape);
    const ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return BodyID::cInvalidBodyID;
    Ref<Shape> shape = result.Get();
    const uint32_t body_id = CreateBodyFromShape(shape, x, y, z, dynamic, restitution, friction);
    if (body_id != static_cast<uint32_t>(BodyID::cInvalidBodyID)) {
      mMutableCompounds[body_id] = Ref<MutableCompoundShape>(static_cast<MutableCompoundShape *>(shape.GetPtr()));
    }
    return body_id;
  }

  int32_t AddMutableSubShape(uint32_t body_id, const SubShapeSpec &sub) {
    auto it = mMutableCompounds.find(body_id);
    if (it == mMutableCompounds.end()) return -1;
    MutableCompoundShape *mcs = it->second.GetPtr();
    const uint idx = mcs->AddShape(sub.pos, sub.rot, sub.shape);
    mcs->AdjustCenterOfMass();
    mPhysicsSystem.GetBodyInterface().SetShape(BodyID(body_id), mcs, true, EActivation::Activate);
    return static_cast<int32_t>(idx);
  }

  bool RemoveMutableSubShape(uint32_t body_id, uint32_t index) {
    auto it = mMutableCompounds.find(body_id);
    if (it == mMutableCompounds.end()) return false;
    MutableCompoundShape *mcs = it->second.GetPtr();
    mcs->RemoveShape(index);
    mcs->AdjustCenterOfMass();
    mPhysicsSystem.GetBodyInterface().SetShape(BodyID(body_id), mcs, true, EActivation::Activate);
    return true;
  }

  bool ModifyMutableSubShape(uint32_t body_id, uint32_t index, Vec3Arg pos, QuatArg rot) {
    auto it = mMutableCompounds.find(body_id);
    if (it == mMutableCompounds.end()) return false;
    MutableCompoundShape *mcs = it->second.GetPtr();
    mcs->ModifyShape(index, pos, rot);
    mPhysicsSystem.GetBodyInterface().SetShape(BodyID(body_id), mcs, false, EActivation::Activate);
    return true;
  }

  bool AdjustMutableCenterOfMass(uint32_t body_id) {
    auto it = mMutableCompounds.find(body_id);
    if (it == mMutableCompounds.end()) return false;
    MutableCompoundShape *mcs = it->second.GetPtr();
    mcs->AdjustCenterOfMass();
    mPhysicsSystem.GetBodyInterface().SetShape(BodyID(body_id), mcs, true, EActivation::Activate);
    return true;
  }

  bool GetBodyPosition(uint32_t body_id, RVec3 &out_position) const {
    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), BodyID(body_id));
    if (!lock.Succeeded()) return false;
    out_position = lock.GetBody().GetPosition();
    return true;
  }

  bool GetBodyRotation(uint32_t body_id, Quat &out_rotation) const {
    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), BodyID(body_id));
    if (!lock.Succeeded()) return false;
    out_rotation = lock.GetBody().GetRotation();
    return true;
  }

  bool SetBodyPosition(uint32_t body_id, double x, double y, double z, bool activate) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetPosition(id, RVec3(x, y, z), activate ? EActivation::Activate : EActivation::DontActivate);
    return true;
  }

  bool SetBodyRotation(uint32_t body_id, float x, float y, float z, float w, bool activate) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetRotation(id, Quat(x, y, z, w), activate ? EActivation::Activate : EActivation::DontActivate);
    return true;
  }
  */

  /*
  bool GetLinearVelocity(uint32_t body_id, Vec3 &out_v) const {
    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), BodyID(body_id));
    if (!lock.Succeeded()) return false;
    out_v = lock.GetBody().GetLinearVelocity();
    return true;
  }

  bool SetLinearVelocity(uint32_t body_id, Vec3Arg v) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetLinearVelocity(id, v);
    return true;
  }

  bool GetAngularVelocity(uint32_t body_id, Vec3 &out_v) const {
    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), BodyID(body_id));
    if (!lock.Succeeded()) return false;
    out_v = lock.GetBody().GetAngularVelocity();
    return true;
  }

  bool SetAngularVelocity(uint32_t body_id, Vec3Arg v) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetAngularVelocity(id, v);
    return true;
  }

  bool ApplyImpulse(uint32_t body_id, Vec3Arg impulse) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.AddImpulse(id, impulse);
    return true;
  }

  bool AddForce(uint32_t body_id, Vec3Arg force) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.AddForce(id, force);
    return true;
  }

  bool AddTorque(uint32_t body_id, Vec3Arg torque) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.AddTorque(id, torque);
    return true;
  }

  bool AddAngularImpulse(uint32_t body_id, Vec3Arg impulse) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.AddAngularImpulse(id, impulse);
    return true;
  }

  bool SetFrictionValue(uint32_t body_id, float friction) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetFriction(id, friction);
    return true;
  }

  bool GetFrictionValue(uint32_t body_id, float &out_value) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_value = bi.GetFriction(id);
    return true;
  }

  bool SetRestitutionValue(uint32_t body_id, float restitution) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetRestitution(id, restitution);
    return true;
  }

  bool GetRestitutionValue(uint32_t body_id, float &out_value) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_value = bi.GetRestitution(id);
    return true;
  }

  bool SetGravityFactorValue(uint32_t body_id, float gravity_factor) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetGravityFactor(id, gravity_factor);
    return true;
  }

  bool GetGravityFactorValue(uint32_t body_id, float &out_value) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_value = bi.GetGravityFactor(id);
    return true;
  }

  bool SetMotionTypeValue(uint32_t body_id, int32_t motion_type, bool activate) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    if (motion_type < 0 || motion_type > 2) return false;
    bi.SetMotionType(id, static_cast<EMotionType>(motion_type), activate ? EActivation::Activate : EActivation::DontActivate);
    return true;
  }

  bool GetMotionTypeValue(uint32_t body_id, int32_t &out_type) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_type = static_cast<int32_t>(bi.GetMotionType(id));
    return true;
  }

  bool SetMotionQualityValue(uint32_t body_id, int32_t quality) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    if (quality < 0 || quality > 1) return false;
    bi.SetMotionQuality(id, static_cast<EMotionQuality>(quality));
    return true;
  }

  bool GetMotionQualityValue(uint32_t body_id, int32_t &out_quality) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_quality = static_cast<int32_t>(bi.GetMotionQuality(id));
    return true;
  }

  bool SetObjectLayerValue(uint32_t body_id, uint32_t layer) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetObjectLayer(id, static_cast<ObjectLayer>(layer));
    return true;
  }

  bool GetObjectLayerValue(uint32_t body_id, uint32_t &out_layer) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_layer = static_cast<uint32_t>(bi.GetObjectLayer(id));
    return true;
  }

  bool SetDamping(uint32_t body_id, float linear_damping, float angular_damping) {
    const BodyLockWrite lock(mPhysicsSystem.GetBodyLockInterface(), BodyID(body_id));
    if (!lock.Succeeded()) return false;
    MotionProperties *mp = lock.GetBody().GetMotionPropertiesUnchecked();
    if (mp == nullptr) return false;
    mp->SetLinearDamping(linear_damping);
    mp->SetAngularDamping(angular_damping);
    return true;
  }

  bool GetDamping(uint32_t body_id, float &out_linear, float &out_angular) const {
    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), BodyID(body_id));
    if (!lock.Succeeded()) return false;
    const MotionProperties *mp = lock.GetBody().GetMotionPropertiesUnchecked();
    if (mp == nullptr) return false;
    out_linear = mp->GetLinearDamping();
    out_angular = mp->GetAngularDamping();
    return true;
  }

  bool ActivateBody(uint32_t body_id) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.ActivateBody(id);
    return true;
  }

  bool DeactivateBody(uint32_t body_id) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.DeactivateBody(id);
    return true;
  }
  */
  /*
  bool RemoveBody(uint32_t body_id) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.RemoveBody(id);
    bi.DestroyBody(id);
    mMutableCompounds.erase(body_id);
    return true;
  }

  bool HasBody(uint32_t body_id) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    return bi.IsAdded(id);
  }

  bool IsBodyActive(uint32_t body_id, bool &out_active) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_active = bi.IsActive(id);
    return true;
  }
  */

  /*
  bool SetBodySensor(uint32_t body_id, bool is_sensor) {
    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id(body_id);
    if (!bi.IsAdded(id)) return false;
    bi.SetIsSensor(id, is_sensor);
    return true;
  }

  bool IsBodySensor(uint32_t body_id, bool &out_is_sensor) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_is_sensor = bi.IsSensor(id);
    return true;
  }

  bool GetCenterOfMassPosition(uint32_t body_id, RVec3 &out_position) const {
    const BodyID id(body_id);
    const BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    if (!bi.IsAdded(id)) return false;
    out_position = bi.GetCenterOfMassPosition(id);
    return true;
  }
  */

  /*
  bool QueryAABB(RVec3Arg min, RVec3Arg max, const QueryFilters &filters, std::vector<uint32_t> &out_bodies) const {
    const AABox box(min, max);
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    AllHitCollisionCollector<CollideShapeBodyCollector> collector;
    mPhysicsSystem.GetBroadPhaseQuery().CollideAABox(box, collector, {}, layer_filter);
    if (!collector.HadHit()) return false;

    out_bodies.clear();
    out_bodies.reserve(collector.mHits.size());
    for (const BodyID &hit : collector.mHits) {
      if (!filters.exclude_ids.empty()) {
        const uint32_t raw = hit.GetIndexAndSequenceNumber();
        bool excluded = false;
        for (uint32_t excl : filters.exclude_ids) {
          if (excl == raw) { excluded = true; break; }
        }
        if (excluded) continue;
      }
      out_bodies.push_back(hit.GetIndexAndSequenceNumber());
    }
    return !out_bodies.empty();
  }

  struct ShapeQueryHit {
    uint32_t body_id;
    float fraction;
    float penetration_depth;
    Vec3 point;
    Vec3 normal;
    uint32_t material_index = 0;
  };

  bool CollideSphereAll(RVec3Arg center, float radius, float max_separation, const QueryFilters &filters, std::vector<ShapeQueryHit> &out_hits) const {
    SphereShape sphere(radius);
    CollideShapeSettings settings;
    settings.mMaxSeparationDistance = max_separation;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));
    AllHitCollisionCollector<CollideShapeCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CollideShape(
        &sphere,
        Vec3::sReplicate(1.0f),
        RMat44::sTranslation(center),
        settings,
        RVec3::sZero(),
        collector,
        {},
        layer_filter,
        body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());
    for (const CollideShapeResult &hit : collector.mHits) {
      const Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
      uint32_t mat_index = 0;
      {
        const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded())
          mat_index = GetHitMaterialIndex(lock.GetBody(), hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID2.GetIndexAndSequenceNumber(), 0.0f, hit.mPenetrationDepth, hit.mContactPointOn2, normal, mat_index});
    }
    return true;
  }

  bool CastSphereAll(RVec3Arg origin, Vec3Arg direction, float max_dist, float radius, const QueryFilters &filters, std::vector<ShapeQueryHit> &out_hits) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * max_dist;
    SphereShape sphere(radius);
    RShapeCast cast(&sphere, Vec3::sReplicate(1.0f), RMat44::sTranslation(origin), dir);
    ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));

    AllHitCollisionCollector<CastShapeCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CastShape(cast, settings, RVec3::sZero(), collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());
    for (const ShapeCastResult &hit : collector.mHits) {
      const Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
      uint32_t mat_index = 0;
      {
        const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded())
          mat_index = GetHitMaterialIndex(lock.GetBody(), hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID2.GetIndexAndSequenceNumber(), hit.mFraction, hit.mPenetrationDepth, hit.mContactPointOn2, normal, mat_index});
    }
    return true;
  }

  bool CastBoxAll(RVec3Arg origin, Vec3Arg direction, float max_dist,
                  float hx, float hy, float hz,
                  const QueryFilters &filters, std::vector<ShapeQueryHit> &out_hits) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * max_dist;
    BoxShape box(Vec3(hx, hy, hz));
    RShapeCast cast(&box, Vec3::sReplicate(1.0f), RMat44::sTranslation(origin), dir);
    ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));

    AllHitCollisionCollector<CastShapeCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CastShape(cast, settings, RVec3::sZero(), collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());
    for (const ShapeCastResult &hit : collector.mHits) {
      const Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
      uint32_t mat_index = 0;
      {
        const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded())
          mat_index = GetHitMaterialIndex(lock.GetBody(), hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID2.GetIndexAndSequenceNumber(), hit.mFraction, hit.mPenetrationDepth, hit.mContactPointOn2, normal, mat_index});
    }
    return true;
  }

  bool CastCapsuleAll(RVec3Arg origin, Vec3Arg direction, float max_dist,
                      float half_height, float radius,
                      const QueryFilters &filters, std::vector<ShapeQueryHit> &out_hits) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * max_dist;
    CapsuleShape capsule(half_height, radius);
    RShapeCast cast(&capsule, Vec3::sReplicate(1.0f), RMat44::sTranslation(origin), dir);
    ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));

    AllHitCollisionCollector<CastShapeCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CastShape(cast, settings, RVec3::sZero(), collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());
    for (const ShapeCastResult &hit : collector.mHits) {
      const Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
      uint32_t mat_index = 0;
      {
        const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded())
          mat_index = GetHitMaterialIndex(lock.GetBody(), hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID2.GetIndexAndSequenceNumber(), hit.mFraction, hit.mPenetrationDepth, hit.mContactPointOn2, normal, mat_index});
    }
    return true;
  }

  bool RayCastClosest(RVec3Arg origin, Vec3Arg direction, double max_dist, const QueryFilters &filters, uint32_t &out_body, double &out_fraction, Vec3 &out_normal, uint32_t &out_material) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * static_cast<float>(max_dist);
    RRayCast ray(origin, dir);
    RayCastResult hit;
    hit.Reset();
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));

    if (!mPhysicsSystem.GetNarrowPhaseQuery().CastRay(ray, hit, {}, layer_filter, body_filter)) {
      return false;
    }

    out_body = hit.mBodyID.GetIndexAndSequenceNumber();
    out_fraction = hit.mFraction;

    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID);
    if (lock.Succeeded()) {
      const Body &body = lock.GetBody();
      out_normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction));
      out_material = GetHitMaterialIndex(body, hit.mSubShapeID2);
    } else {
      out_normal = Vec3::sZero();
      out_material = 0;
    }

    return true;
  }

  struct RayHitInfo {
    uint32_t body_id;
    float fraction;
    Vec3 normal;
    uint32_t material_index = 0;
  };

  bool RayCastAll(RVec3Arg origin, Vec3Arg direction, double max_dist, const QueryFilters &filters, std::vector<RayHitInfo> &out_hits) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * static_cast<float>(max_dist);
    RRayCast ray(origin, dir);
    RayCastSettings settings;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));
    AllHitCollisionCollector<CastRayCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CastRay(ray, settings, collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());

    for (const RayCastResult &hit : collector.mHits) {
      Vec3 normal = Vec3::sZero();
      uint32_t mat_index = 0;
      const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID);
      if (lock.Succeeded()) {
        const Body &b = lock.GetBody();
        normal = b.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction));
        mat_index = GetHitMaterialIndex(b, hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID.GetIndexAndSequenceNumber(), hit.mFraction, normal, mat_index});
    }

    return true;
  }

  bool AreBodiesInContact(uint32_t a, uint32_t b) const {
    return mPhysicsSystem.WereBodiesInContact(BodyID(a), BodyID(b));
  }
  */

  /*
  uint32_t CreateFixedConstraint(uint32_t a, uint32_t b) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    FixedConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mAutoDetectPoint = true;

    Ref<Constraint> constraint = new FixedConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  uint32_t CreateDistanceConstraint(uint32_t a, uint32_t b, RVec3Arg point_a, RVec3Arg point_b, float min_dist, float max_dist) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    DistanceConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = point_a;
    settings.mPoint2 = point_b;
    settings.mMinDistance = min_dist;
    settings.mMaxDistance = max_dist;

    Ref<Constraint> constraint = new DistanceConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  bool SetDistanceLimits(uint32_t id, float min_dist, float max_dist) {
    DistanceConstraint *c = GetConstraintAs<DistanceConstraint>(id, EConstraintSubType::Distance);
    if (!c) return false;
    c->SetDistance(min_dist, max_dist);
    return true;
  }

  bool GetDistanceLimits(uint32_t id, float &out_min, float &out_max) {
    DistanceConstraint *c = GetConstraintAs<DistanceConstraint>(id, EConstraintSubType::Distance);
    if (!c) return false;
    out_min = c->GetMinDistance();
    out_max = c->GetMaxDistance();
    return true;
  }

  bool SetDistanceLimitsSpring(uint32_t id, const SpringSettings &spring) {
    DistanceConstraint *c = GetConstraintAs<DistanceConstraint>(id, EConstraintSubType::Distance);
    if (!c) return false;
    c->SetLimitsSpringSettings(spring);
    return true;
  }

  bool GetDistanceLimitsSpring(uint32_t id, SpringSettings &out) {
    DistanceConstraint *c = GetConstraintAs<DistanceConstraint>(id, EConstraintSubType::Distance);
    if (!c) return false;
    out = c->GetLimitsSpringSettings();
    return true;
  }

  uint32_t CreateHingeConstraint(uint32_t a, uint32_t b, RVec3Arg point, Vec3Arg axis, Vec3Arg normal) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    HingeConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = point;
    settings.mPoint2 = point;
    settings.mHingeAxis1 = axis;
    settings.mHingeAxis2 = axis;
    settings.mNormalAxis1 = normal;
    settings.mNormalAxis2 = normal;

    Ref<Constraint> constraint = new HingeConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  uint32_t CreateSliderConstraint(
      uint32_t a,
      uint32_t b,
      RVec3Arg point,
      Vec3Arg axis,
      Vec3Arg normal,
      float min_limit,
      float max_limit) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    SliderConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = point;
    settings.mPoint2 = point;
    settings.mSliderAxis1 = axis;
    settings.mSliderAxis2 = axis;
    settings.mNormalAxis1 = normal;
    settings.mNormalAxis2 = normal;
    settings.mLimitsMin = min_limit;
    settings.mLimitsMax = max_limit;

    Ref<Constraint> constraint = new SliderConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  uint32_t CreatePointConstraint(uint32_t a, uint32_t b, RVec3Arg point) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    PointConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = point;
    settings.mPoint2 = point;

    Ref<Constraint> constraint = new PointConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  uint32_t CreateConeConstraint(uint32_t a, uint32_t b, RVec3Arg point, Vec3Arg twist_axis, float half_angle) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    ConeConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = point;
    settings.mPoint2 = point;
    settings.mTwistAxis1 = twist_axis;
    settings.mTwistAxis2 = twist_axis;
    settings.mHalfConeAngle = half_angle;

    Ref<Constraint> constraint = new ConeConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  uint32_t CreateSwingTwistConstraint(
      uint32_t a,
      uint32_t b,
      RVec3Arg point,
      Vec3Arg twist_axis,
      Vec3Arg plane_axis,
      float normal_half_cone,
      float plane_half_cone,
      float twist_min,
      float twist_max) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    SwingTwistConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPosition1 = point;
    settings.mPosition2 = point;
    settings.mTwistAxis1 = twist_axis;
    settings.mTwistAxis2 = twist_axis;
    settings.mPlaneAxis1 = plane_axis;
    settings.mPlaneAxis2 = plane_axis;
    settings.mNormalHalfConeAngle = normal_half_cone;
    settings.mPlaneHalfConeAngle = plane_half_cone;
    settings.mTwistMinAngle = twist_min;
    settings.mTwistMaxAngle = twist_max;

    Ref<Constraint> constraint = new SwingTwistConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  uint32_t CreateSixDOFConstraint(uint32_t a, uint32_t b, RVec3Arg point, Vec3Arg axis_x, Vec3Arg axis_y) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    SixDOFConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPosition1 = point;
    settings.mPosition2 = point;
    settings.mAxisX1 = axis_x;
    settings.mAxisX2 = axis_x;
    settings.mAxisY1 = axis_y;
    settings.mAxisY2 = axis_y;

    Ref<Constraint> constraint = new SixDOFConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  uint32_t CreateGearConstraint(uint32_t a, uint32_t b, Vec3Arg hinge_axis1, Vec3Arg hinge_axis2, float ratio,
                                uint32_t gear1_id, uint32_t gear2_id) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    GearConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mHingeAxis1 = hinge_axis1;
    settings.mHingeAxis2 = hinge_axis2;
    settings.mRatio = ratio;

    Ref<Constraint> constraint = new GearConstraint(*body_a, *body_b, settings);
    const uint32_t id = StoreConstraint(constraint);

    if (gear1_id != 0 || gear2_id != 0) {
      GearConstraint *gear = static_cast<GearConstraint *>(constraint.GetPtr());
      auto it1 = mConstraints.find(gear1_id);
      auto it2 = mConstraints.find(gear2_id);
      const Constraint *c1 = (it1 != mConstraints.end()) ? it1->second.GetPtr() : nullptr;
      const Constraint *c2 = (it2 != mConstraints.end()) ? it2->second.GetPtr() : nullptr;
      gear->SetConstraints(c1, c2);
    }
    return id;
  }

  uint32_t CreateRackAndPinionConstraint(uint32_t a, uint32_t b,
                                          Vec3Arg hinge_axis, Vec3Arg slider_axis, float ratio,
                                          uint32_t pinion_id, uint32_t rack_id) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (!body_a || !body_b) return 0;

    RackAndPinionConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mHingeAxis = hinge_axis;
    settings.mSliderAxis = slider_axis;
    settings.mRatio = ratio;

    Ref<Constraint> constraint = new RackAndPinionConstraint(*body_a, *body_b, settings);
    const uint32_t id = StoreConstraint(constraint);

    if (pinion_id != 0 || rack_id != 0) {
      RackAndPinionConstraint *rap = static_cast<RackAndPinionConstraint *>(constraint.GetPtr());
      auto pit = mConstraints.find(pinion_id);
      auto rit = mConstraints.find(rack_id);
      const Constraint *pinion = (pit != mConstraints.end()) ? pit->second.GetPtr() : nullptr;
      const Constraint *rack   = (rit != mConstraints.end()) ? rit->second.GetPtr() : nullptr;
      rap->SetConstraints(pinion, rack);
    }
    return id;
  }

  bool GetRackAndPinionLambda(uint32_t id, float &out) {
    RackAndPinionConstraint *c = GetConstraintAs<RackAndPinionConstraint>(id, EConstraintSubType::RackAndPinion);
    if (!c) return false;
    out = c->GetTotalLambda();
    return true;
  }

  uint32_t CreatePulleyConstraint(uint32_t a, uint32_t b,
                                   RVec3Arg body_point1, RVec3Arg fixed_point1,
                                   RVec3Arg body_point2, RVec3Arg fixed_point2,
                                   float ratio, float min_length, float max_length) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    PulleyConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mBodyPoint1 = body_point1;
    settings.mFixedPoint1 = fixed_point1;
    settings.mBodyPoint2 = body_point2;
    settings.mFixedPoint2 = fixed_point2;
    settings.mRatio = ratio;
    settings.mMinLength = min_length;
    settings.mMaxLength = max_length;

    Ref<Constraint> constraint = new PulleyConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  bool RemoveConstraintById(uint32_t constraint_id) {
    auto it = mConstraints.find(constraint_id);
    if (it == mConstraints.end()) return false;
    mPhysicsSystem.RemoveConstraint(it->second.GetPtr());
    mConstraints.erase(it);
    return true;
  }

  bool SetHingeLimits(uint32_t constraint_id, float min_angle, float max_angle) {
    HingeConstraint *c = GetConstraintAs<HingeConstraint>(constraint_id, EConstraintSubType::Hinge);
    if (c == nullptr) return false;
    c->SetLimits(min_angle, max_angle);
    return true;
  }

  bool SetSliderLimits(uint32_t constraint_id, float min_limit, float max_limit) {
    SliderConstraint *c = GetConstraintAs<SliderConstraint>(constraint_id, EConstraintSubType::Slider);
    if (c == nullptr) return false;
    c->SetLimits(min_limit, max_limit);
    return true;
  }

  bool SetHingeMotor(uint32_t constraint_id, int32_t state, float target_velocity, float target_angle, float max_torque) {
    HingeConstraint *c = GetConstraintAs<HingeConstraint>(constraint_id, EConstraintSubType::Hinge);
    if (c == nullptr || state < 0 || state > 2) return false;
    c->GetMotorSettings().SetTorqueLimit(max_torque);
    c->SetMotorState(static_cast<EMotorState>(state));
    c->SetTargetAngularVelocity(target_velocity);
    c->SetTargetAngle(target_angle);
    return true;
  }

  bool SetSliderMotor(uint32_t constraint_id, int32_t state, float target_velocity, float target_position, float max_force) {
    SliderConstraint *c = GetConstraintAs<SliderConstraint>(constraint_id, EConstraintSubType::Slider);
    if (c == nullptr || state < 0 || state > 2) return false;
    c->GetMotorSettings().SetForceLimit(max_force);
    c->SetMotorState(static_cast<EMotorState>(state));
    c->SetTargetVelocity(target_velocity);
    c->SetTargetPosition(target_position);
    return true;
  }

  bool SetConeHalfAngle(uint32_t constraint_id, float half_angle) {
    ConeConstraint *c = GetConstraintAs<ConeConstraint>(constraint_id, EConstraintSubType::Cone);
    if (c == nullptr) return false;
    c->SetHalfConeAngle(half_angle);
    return true;
  }

  bool SetSwingTwistLimits(uint32_t constraint_id, float normal_half, float plane_half, float twist_min, float twist_max) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(constraint_id, EConstraintSubType::SwingTwist);
    if (c == nullptr) return false;
    c->SetNormalHalfConeAngle(normal_half);
    c->SetPlaneHalfConeAngle(plane_half);
    c->SetTwistMinAngle(twist_min);
    c->SetTwistMaxAngle(twist_max);
    return true;
  }

  bool GetHingeLimits(uint32_t constraint_id, float &out_min, float &out_max) {
    HingeConstraint *c = GetConstraintAs<HingeConstraint>(constraint_id, EConstraintSubType::Hinge);
    if (c == nullptr) return false;
    out_min = c->GetLimitsMin();
    out_max = c->GetLimitsMax();
    return true;
  }

  bool GetSliderLimits(uint32_t constraint_id, float &out_min, float &out_max) {
    SliderConstraint *c = GetConstraintAs<SliderConstraint>(constraint_id, EConstraintSubType::Slider);
    if (c == nullptr) return false;
    out_min = c->GetLimitsMin();
    out_max = c->GetLimitsMax();
    return true;
  }

  bool GetSwingTwistLimits(uint32_t constraint_id, float &out_n_half, float &out_p_half, float &out_t_min, float &out_t_max) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(constraint_id, EConstraintSubType::SwingTwist);
    if (c == nullptr) return false;
    out_n_half = c->GetNormalHalfConeAngle();
    out_p_half = c->GetPlaneHalfConeAngle();
    out_t_min = c->GetTwistMinAngle();
    out_t_max = c->GetTwistMaxAngle();
    return true;
  }

  bool SetSwingTwistMotor(
      uint32_t constraint_id,
      int32_t swing_state,
      int32_t twist_state,
      Vec3Arg target_ang_vel,
      QuatArg target_orientation,
      float max_torque) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(constraint_id, EConstraintSubType::SwingTwist);
    if (c == nullptr || swing_state < 0 || swing_state > 2 || twist_state < 0 || twist_state > 2) return false;
    c->GetSwingMotorSettings().SetTorqueLimit(max_torque);
    c->GetTwistMotorSettings().SetTorqueLimit(max_torque);
    c->SetSwingMotorState(static_cast<EMotorState>(swing_state));
    c->SetTwistMotorState(static_cast<EMotorState>(twist_state));
    c->SetTargetAngularVelocityCS(target_ang_vel);
    c->SetTargetOrientationCS(target_orientation.Normalized());
    return true;
  }

  bool SetSixDOFLimits(uint32_t constraint_id, Vec3Arg tmin, Vec3Arg tmax, Vec3Arg rmin, Vec3Arg rmax) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(constraint_id, EConstraintSubType::SixDOF);
    if (c == nullptr) return false;
    c->SetTranslationLimits(tmin, tmax);
    c->SetRotationLimits(rmin, rmax);
    return true;
  }

  bool SetSixDOFMotorState(uint32_t constraint_id, int32_t axis, int32_t state) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(constraint_id, EConstraintSubType::SixDOF);
    if (c == nullptr || axis < 0 || axis >= SixDOFConstraintSettings::EAxis::Num || state < 0 || state > 2) return false;
    c->SetMotorState(static_cast<SixDOFConstraint::EAxis>(axis), static_cast<EMotorState>(state));
    return true;
  }

  bool SetSixDOFTargetVelocity(uint32_t constraint_id, Vec3Arg linear, Vec3Arg angular) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(constraint_id, EConstraintSubType::SixDOF);
    if (c == nullptr) return false;
    c->SetTargetVelocityCS(linear);
    c->SetTargetAngularVelocityCS(angular);
    return true;
  }

  bool SetSixDOFTargetPose(uint32_t constraint_id, Vec3Arg position, QuatArg orientation) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(constraint_id, EConstraintSubType::SixDOF);
    if (c == nullptr) return false;
    c->SetTargetPositionCS(position);
    c->SetTargetOrientationCS(orientation.Normalized());
    return true;
  }

  // Motor spring settings setters
  bool SetHingeMotorSpring(uint32_t id, const MotorSpringParams &p) {
    HingeConstraint *c = GetConstraintAs<HingeConstraint>(id, EConstraintSubType::Hinge);
    if (c == nullptr) return false;
    ApplyMotorSpring(c->GetMotorSettings(), p);
    return true;
  }

  bool SetSliderMotorSpring(uint32_t id, const MotorSpringParams &p) {
    SliderConstraint *c = GetConstraintAs<SliderConstraint>(id, EConstraintSubType::Slider);
    if (c == nullptr) return false;
    ApplyMotorSpring(c->GetMotorSettings(), p);
    return true;
  }

  bool SetSwingMotorSpring(uint32_t id, const MotorSpringParams &p) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(id, EConstraintSubType::SwingTwist);
    if (c == nullptr) return false;
    ApplyMotorSpring(c->GetSwingMotorSettings(), p);
    return true;
  }

  bool SetTwistMotorSpring(uint32_t id, const MotorSpringParams &p) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(id, EConstraintSubType::SwingTwist);
    if (c == nullptr) return false;
    ApplyMotorSpring(c->GetTwistMotorSettings(), p);
    return true;
  }

  bool SetSixDOFMotorSpring(uint32_t id, int32_t axis, const MotorSpringParams &p) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(id, EConstraintSubType::SixDOF);
    if (c == nullptr || axis < 0 || axis >= SixDOFConstraintSettings::EAxis::Num) return false;
    ApplyMotorSpring(c->GetMotorSettings(static_cast<SixDOFConstraint::EAxis>(axis)), p);
    return true;
  }

  // Motor spring settings getters
  bool GetHingeMotorSpring(uint32_t id, MotorSettings &out) {
    HingeConstraint *c = GetConstraintAs<HingeConstraint>(id, EConstraintSubType::Hinge);
    if (c == nullptr) return false;
    out = c->GetMotorSettings();
    return true;
  }

  bool GetSliderMotorSpring(uint32_t id, MotorSettings &out) {
    SliderConstraint *c = GetConstraintAs<SliderConstraint>(id, EConstraintSubType::Slider);
    if (c == nullptr) return false;
    out = c->GetMotorSettings();
    return true;
  }

  bool GetSwingMotorSpring(uint32_t id, MotorSettings &out) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(id, EConstraintSubType::SwingTwist);
    if (c == nullptr) return false;
    out = c->GetSwingMotorSettings();
    return true;
  }

  bool GetTwistMotorSpring(uint32_t id, MotorSettings &out) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(id, EConstraintSubType::SwingTwist);
    if (c == nullptr) return false;
    out = c->GetTwistMotorSettings();
    return true;
  }

  bool GetSixDOFMotorSpring(uint32_t id, int32_t axis, MotorSettings &out) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(id, EConstraintSubType::SixDOF);
    if (c == nullptr || axis < 0 || axis >= SixDOFConstraintSettings::EAxis::Num) return false;
    out = c->GetMotorSettings(static_cast<SixDOFConstraint::EAxis>(axis));
    return true;
  }

  // Constraint state getters
  bool GetHingeAngle(uint32_t constraint_id, float &out_angle) {
    HingeConstraint *c = GetConstraintAs<HingeConstraint>(constraint_id, EConstraintSubType::Hinge);
    if (c == nullptr) return false;
    out_angle = c->GetCurrentAngle();
    return true;
  }

  bool GetHingeMotorState(uint32_t constraint_id, int32_t &out_state) {
    HingeConstraint *c = GetConstraintAs<HingeConstraint>(constraint_id, EConstraintSubType::Hinge);
    if (c == nullptr) return false;
    out_state = static_cast<int32_t>(c->GetMotorState());
    return true;
  }

  bool GetSliderPosition(uint32_t constraint_id, float &out_position) {
    SliderConstraint *c = GetConstraintAs<SliderConstraint>(constraint_id, EConstraintSubType::Slider);
    if (c == nullptr) return false;
    out_position = c->GetCurrentPosition();
    return true;
  }

  bool GetSliderMotorState(uint32_t constraint_id, int32_t &out_state) {
    SliderConstraint *c = GetConstraintAs<SliderConstraint>(constraint_id, EConstraintSubType::Slider);
    if (c == nullptr) return false;
    out_state = static_cast<int32_t>(c->GetMotorState());
    return true;
  }

  bool GetSixDOFRotation(uint32_t constraint_id, Quat &out_rotation) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(constraint_id, EConstraintSubType::SixDOF);
    if (c == nullptr) return false;
    out_rotation = c->GetRotationInConstraintSpace();
    return true;
  }

  bool GetSixDOFLimits(uint32_t constraint_id, Vec3 &out_tmin, Vec3 &out_tmax, Vec3 &out_rmin, Vec3 &out_rmax) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(constraint_id, EConstraintSubType::SixDOF);
    if (c == nullptr) return false;
    out_tmin = c->GetTranslationLimitsMin();
    out_tmax = c->GetTranslationLimitsMax();
    out_rmin = c->GetRotationLimitsMin();
    out_rmax = c->GetRotationLimitsMax();
    return true;
  }

  bool GetSixDOFMotorState(uint32_t constraint_id, int32_t axis, int32_t &out_state) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(constraint_id, EConstraintSubType::SixDOF);
    if (c == nullptr || axis < 0 || axis >= SixDOFConstraintSettings::EAxis::Num) return false;
    out_state = static_cast<int32_t>(c->GetMotorState(static_cast<SixDOFConstraint::EAxis>(axis)));
    return true;
  }

  bool GetSwingTwistRotation(uint32_t constraint_id, Quat &out_rotation) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(constraint_id, EConstraintSubType::SwingTwist);
    if (c == nullptr) return false;
    out_rotation = c->GetRotationInConstraintSpace();
    return true;
  }

  bool GetSwingTwistMotorState(uint32_t constraint_id, int32_t &out_swing_state, int32_t &out_twist_state) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(constraint_id, EConstraintSubType::SwingTwist);
    if (c == nullptr) return false;
    out_swing_state = static_cast<int32_t>(c->GetSwingMotorState());
    out_twist_state = static_cast<int32_t>(c->GetTwistMotorState());
    return true;
  }

  // Lambda (constraint impulse) getters
  bool GetHingeLambdas(uint32_t id, Vec3 &pos, float &rx, float &ry, float &rlim, float &motor) {
    HingeConstraint *c = GetConstraintAs<HingeConstraint>(id, EConstraintSubType::Hinge);
    if (!c) return false;
    pos = c->GetTotalLambdaPosition();
    auto r = c->GetTotalLambdaRotation(); rx = r[0]; ry = r[1];
    rlim = c->GetTotalLambdaRotationLimits();
    motor = c->GetTotalLambdaMotor();
    return true;
  }

  bool GetSliderLambdas(uint32_t id, float &px, float &py, float &plim, Vec3 &rot, float &motor) {
    SliderConstraint *c = GetConstraintAs<SliderConstraint>(id, EConstraintSubType::Slider);
    if (!c) return false;
    auto p = c->GetTotalLambdaPosition(); px = p[0]; py = p[1];
    plim = c->GetTotalLambdaPositionLimits();
    rot = c->GetTotalLambdaRotation();
    motor = c->GetTotalLambdaMotor();
    return true;
  }

  bool GetSwingTwistLambdas(uint32_t id, Vec3 &pos, float &twist, float &swy, float &swz, Vec3 &motor) {
    SwingTwistConstraint *c = GetConstraintAs<SwingTwistConstraint>(id, EConstraintSubType::SwingTwist);
    if (!c) return false;
    pos = c->GetTotalLambdaPosition();
    twist = c->GetTotalLambdaTwist();
    swy = c->GetTotalLambdaSwingY();
    swz = c->GetTotalLambdaSwingZ();
    motor = c->GetTotalLambdaMotor();
    return true;
  }

  bool GetSixDOFLambdas(uint32_t id, Vec3 &pos, Vec3 &rot, Vec3 &mtrans, Vec3 &mrot) {
    SixDOFConstraint *c = GetConstraintAs<SixDOFConstraint>(id, EConstraintSubType::SixDOF);
    if (!c) return false;
    pos = c->GetTotalLambdaPosition();
    rot = c->GetTotalLambdaRotation();
    mtrans = c->GetTotalLambdaMotorTranslation();
    mrot = c->GetTotalLambdaMotorRotation();
    return true;
  }

  bool GetConeLambdas(uint32_t id, Vec3 &pos, float &rot) {
    ConeConstraint *c = GetConstraintAs<ConeConstraint>(id, EConstraintSubType::Cone);
    if (!c) return false;
    pos = c->GetTotalLambdaPosition();
    rot = c->GetTotalLambdaRotation();
    return true;
  }

  bool GetPointLambdas(uint32_t id, Vec3 &pos) {
    PointConstraint *c = GetConstraintAs<PointConstraint>(id, EConstraintSubType::Point);
    if (!c) return false;
    pos = c->GetTotalLambdaPosition();
    return true;
  }

  bool GetFixedLambdas(uint32_t id, Vec3 &pos, Vec3 &rot) {
    FixedConstraint *c = GetConstraintAs<FixedConstraint>(id, EConstraintSubType::Fixed);
    if (!c) return false;
    pos = c->GetTotalLambdaPosition();
    rot = c->GetTotalLambdaRotation();
    return true;
  }

  bool GetDistanceLambda(uint32_t id, float &out) {
    DistanceConstraint *c = GetConstraintAs<DistanceConstraint>(id, EConstraintSubType::Distance);
    if (!c) return false;
    out = c->GetTotalLambdaPosition();
    return true;
  }

  bool GetPulleyLambda(uint32_t id, float &out) {
    PulleyConstraint *c = GetConstraintAs<PulleyConstraint>(id, EConstraintSubType::Pulley);
    if (!c) return false;
    out = c->GetTotalLambdaPosition();
    return true;
  }

  bool GetGearLambda(uint32_t id, float &out) {
    GearConstraint *c = GetConstraintAs<GearConstraint>(id, EConstraintSubType::Gear);
    if (!c) return false;
    out = c->GetTotalLambda();
    return true;
  }

  bool GetPathLambdas(uint32_t id, float &px, float &py, float &plim, float &motor, float &rhx, float &rhy, Vec3 &rot) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(id, EConstraintSubType::Path);
    if (!c) return false;
    auto p = c->GetTotalLambdaPosition(); px = p[0]; py = p[1];
    plim = c->GetTotalLambdaPositionLimits();
    motor = c->GetTotalLambdaMotor();
    auto rh = c->GetTotalLambdaRotationHinge(); rhx = rh[0]; rhy = rh[1];
    rot = c->GetTotalLambdaRotation();
    return true;
  }

  bool GetPulleyLength(uint32_t constraint_id, float &out_length) {
    PulleyConstraint *c = GetConstraintAs<PulleyConstraint>(constraint_id, EConstraintSubType::Pulley);
    if (c == nullptr) return false;
    out_length = c->GetCurrentLength();
    return true;
  }

  bool SetPulleyLengthLimits(uint32_t constraint_id, float min_length, float max_length) {
    PulleyConstraint *c = GetConstraintAs<PulleyConstraint>(constraint_id, EConstraintSubType::Pulley);
    if (c == nullptr) return false;
    c->SetLength(min_length, max_length);
    return true;
  }

  bool GetPulleyLengthLimits(uint32_t constraint_id, float &out_min, float &out_max) {
    PulleyConstraint *c = GetConstraintAs<PulleyConstraint>(constraint_id, EConstraintSubType::Pulley);
    if (c == nullptr) return false;
    out_min = c->GetMinLength();
    out_max = c->GetMaxLength();
    return true;
  }

  uint32_t CreatePathConstraint(uint32_t a, uint32_t b,
                                 const std::vector<float> &flat_points,
                                 bool closed,
                                 Vec3Arg path_pos, QuatArg path_rot,
                                 float path_fraction, float max_friction,
                                 int rotation_type) {
    const BodyID ids[2] = {BodyID(a), BodyID(b)};
    BodyLockMultiWrite lock(mPhysicsSystem.GetBodyLockInterface(), ids, 2);
    Body *body_a = lock.GetBody(0);
    Body *body_b = lock.GetBody(1);
    if (body_a == nullptr || body_b == nullptr) return 0;

    Ref<PathConstraintPathHermite> path = new PathConstraintPathHermite();
    const size_t pt_count = flat_points.size() / 9;
    for (size_t i = 0; i < pt_count; i++) {
      const size_t base = i * 9;
      path->AddPoint(
        Vec3(flat_points[base+0], flat_points[base+1], flat_points[base+2]),
        Vec3(flat_points[base+3], flat_points[base+4], flat_points[base+5]),
        Vec3(flat_points[base+6], flat_points[base+7], flat_points[base+8]));
    }
    path->SetIsLooping(closed);

    PathConstraintSettings settings;
    settings.mPath = path;
    settings.mPathPosition = path_pos;
    settings.mPathRotation = path_rot;
    settings.mPathFraction = path_fraction;
    settings.mMaxFrictionForce = max_friction;
    settings.mRotationConstraintType = static_cast<EPathRotationConstraintType>(rotation_type);

    Ref<Constraint> constraint = new PathConstraint(*body_a, *body_b, settings);
    return StoreConstraint(constraint);
  }

  bool GetPathFraction(uint32_t constraint_id, float &out_fraction) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(constraint_id, EConstraintSubType::Path);
    if (c == nullptr) return false;
    out_fraction = c->GetPathFraction();
    return true;
  }

  bool GetPathMaxFraction(uint32_t constraint_id, float &out_max) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(constraint_id, EConstraintSubType::Path);
    if (c == nullptr) return false;
    const PathConstraintPath *path = c->GetPath();
    out_max = path != nullptr ? path->GetPathMaxFraction() : 0.0f;
    return true;
  }

  bool SetPathMotorState(uint32_t constraint_id, int state) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(constraint_id, EConstraintSubType::Path);
    if (c == nullptr) return false;
    c->SetPositionMotorState(static_cast<EMotorState>(state));
    return true;
  }

  bool SetPathTargetVelocity(uint32_t constraint_id, float velocity) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(constraint_id, EConstraintSubType::Path);
    if (c == nullptr) return false;
    c->SetTargetVelocity(velocity);
    return true;
  }

  bool SetPathTargetFraction(uint32_t constraint_id, float fraction) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(constraint_id, EConstraintSubType::Path);
    if (c == nullptr) return false;
    c->SetTargetPathFraction(fraction);
    return true;
  }

  bool GetPathMotorState(uint32_t constraint_id, int32_t &out_state, float &out_vel, float &out_fraction) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(constraint_id, EConstraintSubType::Path);
    if (c == nullptr) return false;
    out_state = static_cast<int32_t>(c->GetPositionMotorState());
    out_vel = c->GetTargetVelocity();
    out_fraction = c->GetTargetPathFraction();
    return true;
  }

  bool SetPathMotorSpring(uint32_t constraint_id, const MotorSpringParams &p) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(constraint_id, EConstraintSubType::Path);
    if (c == nullptr) return false;
    ApplyMotorSpring(c->GetPositionMotorSettings(), p);
    return true;
  }

  bool GetPathMotorSpring(uint32_t constraint_id, MotorSettings &out) {
    PathConstraint *c = GetConstraintAs<PathConstraint>(constraint_id, EConstraintSubType::Path);
    if (c == nullptr) return false;
    out = c->GetPositionMotorSettings();
    return true;
  }

  */

  /*
  uint32_t CreateSkeleton() {
    const uint32_t id = mNextSkeletonId++;
    mSkeletons[id] = new Skeleton();
    return id;
  }

  bool AddSkeletonJoint(uint32_t skeleton_id, const std::string &name, int32_t parent_idx) {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return false;
    if (parent_idx < 0) {
      it->second->AddJoint(name);
    } else {
      it->second->AddJoint(name, parent_idx);
    }
    return true;
  }

  bool FinalizeSkeleton(uint32_t skeleton_id) {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return false;
    it->second->CalculateParentJointIndices();
    return it->second->AreJointsCorrectlyOrdered();
  }

  int GetSkeletonJointCount(uint32_t skeleton_id) const {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return -1;
    return it->second->GetJointCount();
  }

  bool GetSkeletonJointInfo(uint32_t skeleton_id, int32_t joint_index, std::string &out_name, int32_t &out_parent_index) const {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return false;
    if (joint_index < 0 || joint_index >= it->second->GetJointCount()) return false;
    const Skeleton::Joint &joint = it->second->GetJoint(joint_index);
    out_name = joint.mName.c_str();
    out_parent_index = joint.mParentJointIndex;
    return true;
  }

  int GetSkeletonJointIndex(uint32_t skeleton_id, const std::string &name) const {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return -1;
    return it->second->GetJointIndex(name);
  }

  uint32_t CreateRagdollSettings(uint32_t skeleton_id, float capsule_half_height, float capsule_radius, float spacing) {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return 0;

    Ref<RagdollSettings> settings = new RagdollSettings();
    settings->mSkeleton = it->second;

    const int count = it->second->GetJointCount();
    settings->mParts.resize(count);

    for (int i = 0; i < count; ++i) {
      CapsuleShapeSettings shape_settings(capsule_half_height, capsule_radius);
      const ShapeSettings::ShapeResult shape_result = shape_settings.Create();
      if (shape_result.HasError()) return 0;

      auto &part = settings->mParts[i];
      part.SetShape(shape_result.Get());
      part.mPosition = RVec3(0.0, -spacing * static_cast<double>(i), 0.0);
      part.mRotation = Quat::sIdentity();
      part.mMotionType = EMotionType::Dynamic;
      part.mObjectLayer = Layers::MOVING;

      const int parent = it->second->GetJoint(i).mParentJointIndex;
      if (parent >= 0) {
        auto *fixed = new FixedConstraintSettings();
        fixed->mSpace = EConstraintSpace::WorldSpace;
        fixed->mAutoDetectPoint = true;
        part.mToParent = fixed;
      }
    }

    settings->CalculateConstraintPriorities();
    settings->CalculateBodyIndexToConstraintIndex();
    settings->CalculateConstraintIndexToBodyIdxPair();
    settings->DisableParentChildCollisions();

    const uint32_t id = mNextRagdollSettingsId++;
    mRagdollSettings[id] = settings;
    return id;
  }

  uint32_t CreateRagdoll(uint32_t settings_id, uint32_t collision_group, uint64_t user_data, bool activate) {
    auto it = mRagdollSettings.find(settings_id);
    if (it == mRagdollSettings.end()) return 0;

    Ragdoll *raw = it->second->CreateRagdoll(collision_group, user_data, &mPhysicsSystem);
    if (raw == nullptr) return 0;

    Ref<Ragdoll> ragdoll = raw;
    ragdoll->AddToPhysicsSystem(activate ? EActivation::Activate : EActivation::DontActivate, true);

    const uint32_t id = mNextRagdollId++;
    mRagdolls[id] = ragdoll;

    // Register each joint constraint in mConstraints so callers can control motors etc.
    const int body_count = static_cast<int>(ragdoll->GetBodyCount());
    std::vector<uint32_t> constraint_ids(body_count, 0u);
    for (int ji = 0; ji < body_count; ++ji) {
      const int ci = it->second->GetConstraintIndexForBodyIndex(ji);
      if (ci < 0) continue;
      TwoBodyConstraint *c = ragdoll->GetConstraint(ci);
      if (c == nullptr) continue;
      const uint32_t cid = mNextConstraintId++;
      mConstraints[cid] = c;
      constraint_ids[ji] = cid;
    }
    mRagdollConstraintIds[id] = std::move(constraint_ids);

    return id;
  }

  bool DestroyRagdoll(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;

    // Erase constraint refs without calling RemoveConstraint —
    // RemoveFromPhysicsSystem(true) below handles physics system cleanup.
    auto cit = mRagdollConstraintIds.find(ragdoll_id);
    if (cit != mRagdollConstraintIds.end()) {
      for (uint32_t cid : cit->second) {
        if (cid != 0) mConstraints.erase(cid);
      }
      mRagdollConstraintIds.erase(cit);
    }

    it->second->RemoveFromPhysicsSystem(true);
    mRagdolls.erase(it);
    return true;
  }

  int GetRagdollBodyCount(uint32_t ragdoll_id) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return -1;
    return static_cast<int>(it->second->GetBodyCount());
  }

  bool GetRagdollBoneBodyId(uint32_t ragdoll_id, int index, uint32_t &out_body_id) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    if (index < 0 || index >= static_cast<int>(it->second->GetBodyCount())) return false;
    out_body_id = it->second->GetBodyID(index).GetIndexAndSequenceNumber();
    return true;
  }

  bool GetRagdollBoneTransform(uint32_t ragdoll_id, int index, RVec3 &out_pos, Quat &out_rot) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    if (index < 0 || index >= static_cast<int>(it->second->GetBodyCount())) return false;

    const BodyID body_id = it->second->GetBodyID(index);
    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), body_id);
    if (!lock.Succeeded()) return false;

    out_pos = lock.GetBody().GetPosition();
    out_rot = lock.GetBody().GetRotation();
    return true;
  }

  bool SetRagdollBoneTransform(uint32_t ragdoll_id, int index, RVec3Arg pos, QuatArg rot, bool activate) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    if (index < 0 || index >= static_cast<int>(it->second->GetBodyCount())) return false;

    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    bi.SetPositionAndRotation(
        it->second->GetBodyID(index),
        pos,
        rot,
        activate ? EActivation::Activate : EActivation::DontActivate);
    return true;
  }

  bool SetRagdollJointShape(uint32_t settings_id, int joint_index,
                             int kind, float half_h, float radius,
                             float hx, float hy, float hz) {
    auto it = mRagdollSettings.find(settings_id);
    if (it == mRagdollSettings.end()) return false;
    auto &parts = it->second->mParts;
    if (joint_index < 0 || joint_index >= static_cast<int>(parts.size())) return false;

    ShapeSettings::ShapeResult result;
    switch (kind) {
      case 0: { CapsuleShapeSettings s(half_h, radius); result = s.Create(); break; }
      case 1: { BoxShapeSettings s(Vec3(hx, hy, hz)); result = s.Create(); break; }
      case 2: { SphereShapeSettings s(radius); result = s.Create(); break; }
      default: return false;
    }
    if (result.HasError()) return false;
    parts[joint_index].SetShape(result.Get());
    return true;
  }

  bool SetRagdollJointTransform(uint32_t settings_id, int joint_index,
                                 RVec3Arg pos, QuatArg rot) {
    auto it = mRagdollSettings.find(settings_id);
    if (it == mRagdollSettings.end()) return false;
    auto &parts = it->second->mParts;
    if (joint_index < 0 || joint_index >= static_cast<int>(parts.size())) return false;
    parts[joint_index].mPosition = pos;
    parts[joint_index].mRotation = rot;
    return true;
  }

  bool SetRagdollJointConstraint(uint32_t settings_id, int joint_index, RagdollJointConstraintConfig cfg) {
    auto it = mRagdollSettings.find(settings_id);
    if (it == mRagdollSettings.end()) return false;
    auto &parts = it->second->mParts;
    if (joint_index <= 0 || joint_index >= static_cast<int>(parts.size())) return false;

    const RVec3 pivot = parts[joint_index].mPosition;

    if (cfg.autoAxes && cfg.type != 0) {
      const int parent_idx = it->second->mSkeleton->GetJoint(joint_index).mParentJointIndex;
      const Vec3 child_pos(static_cast<float>(pivot.GetX()), static_cast<float>(pivot.GetY()), static_cast<float>(pivot.GetZ()));
      const RVec3 parent_rpos = parts[parent_idx].mPosition;
      const Vec3 parent_pos(static_cast<float>(parent_rpos.GetX()), static_cast<float>(parent_rpos.GetY()), static_cast<float>(parent_rpos.GetZ()));
      Vec3 bone_dir = parent_pos - child_pos;
      const float bone_len = bone_dir.Length();
      bone_dir = (bone_len < 1.0e-6f) ? Vec3(0.0f, 1.0f, 0.0f) : bone_dir / bone_len;
      Vec3 perp = bone_dir.Cross(Vec3(1.0f, 0.0f, 0.0f));
      if (perp.LengthSq() < 1.0e-6f) perp = bone_dir.Cross(Vec3(0.0f, 0.0f, 1.0f));
      perp = perp.Normalized();

      if (cfg.type == 1) {
        cfg.twistAxis1 = cfg.twistAxis2 = bone_dir;
        cfg.planeAxis1 = cfg.planeAxis2 = perp;
      } else if (cfg.type == 2) {
        cfg.hingeAxis1 = cfg.hingeAxis2 = perp;
        cfg.normalAxis1 = cfg.normalAxis2 = bone_dir;
      } else if (cfg.type == 3) {
        cfg.coneAxis1 = cfg.coneAxis2 = bone_dir;
      }
    }

    switch (cfg.type) {
      case 0: {
        auto *s = new FixedConstraintSettings();
        s->mSpace = EConstraintSpace::WorldSpace;
        s->mAutoDetectPoint = true;
        parts[joint_index].mToParent = s;
        break;
      }
      case 1: {
        auto *s = new SwingTwistConstraintSettings();
        s->mSpace = EConstraintSpace::WorldSpace;
        s->mPosition1 = pivot;
        s->mPosition2 = pivot;
        s->mTwistAxis1 = cfg.twistAxis1;
        s->mTwistAxis2 = cfg.twistAxis2;
        s->mPlaneAxis1 = cfg.planeAxis1;
        s->mPlaneAxis2 = cfg.planeAxis2;
        s->mNormalHalfConeAngle = cfg.normalHalfCone;
        s->mPlaneHalfConeAngle = cfg.planeHalfCone;
        s->mTwistMinAngle = cfg.twistMin;
        s->mTwistMaxAngle = cfg.twistMax;
        parts[joint_index].mToParent = s;
        break;
      }
      case 2: {
        auto *s = new HingeConstraintSettings();
        s->mSpace = EConstraintSpace::WorldSpace;
        s->mPoint1 = pivot;
        s->mPoint2 = pivot;
        s->mHingeAxis1 = cfg.hingeAxis1;
        s->mHingeAxis2 = cfg.hingeAxis2;
        s->mNormalAxis1 = cfg.normalAxis1;
        s->mNormalAxis2 = cfg.normalAxis2;
        s->mLimitsMin = cfg.hingeMin;
        s->mLimitsMax = cfg.hingeMax;
        parts[joint_index].mToParent = s;
        break;
      }
      case 3: {
        auto *s = new ConeConstraintSettings();
        s->mSpace = EConstraintSpace::WorldSpace;
        s->mPoint1 = pivot;
        s->mPoint2 = pivot;
        s->mTwistAxis1 = cfg.coneAxis1;
        s->mTwistAxis2 = cfg.coneAxis2;
        s->mHalfConeAngle = cfg.halfConeAngle;
        parts[joint_index].mToParent = s;
        break;
      }
      default: return false;
    }
    return true;
  }

  bool GetRagdollConstraintIds(uint32_t ragdoll_id, std::vector<uint32_t> &out) const {
    auto it = mRagdollConstraintIds.find(ragdoll_id);
    if (it == mRagdollConstraintIds.end()) return false;
    out = it->second;
    return true;
  }

  // --- SkeletonPose ---

  uint32_t CreateSkeletonPose(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return 0;
    auto pose = std::make_unique<SkeletonPose>();
    pose->SetSkeleton(it->second->GetRagdollSettings()->GetSkeleton());
    const uint32_t id = mNextSkeletonPoseId++;
    mSkeletonPoses[id] = std::move(pose);
    return id;
  }

  bool DestroySkeletonPose(uint32_t pose_id) {
    return mSkeletonPoses.erase(pose_id) > 0;
  }

  bool SetPoseJoint(uint32_t pose_id, int ji, float tx, float ty, float tz,
                    float rx, float ry, float rz, float rw) {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    if (ji < 0 || ji >= (int)it->second->GetJointCount()) return false;
    auto &j = it->second->GetJoint(ji);
    j.mTranslation = Vec3(tx, ty, tz);
    j.mRotation = Quat(rx, ry, rz, rw);
    return true;
  }

  bool GetPoseJoint(uint32_t pose_id, int ji, Vec3 &out_t, Quat &out_r) const {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    if (ji < 0 || ji >= (int)it->second->GetJointCount()) return false;
    const auto &j = it->second->GetJoint(ji);
    out_t = j.mTranslation;
    out_r = j.mRotation;
    return true;
  }

  bool SetPoseRootOffset(uint32_t pose_id, double x, double y, double z) {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    it->second->SetRootOffset(RVec3(x, y, z));
    return true;
  }

  bool GetPoseRootOffset(uint32_t pose_id, RVec3 &out) const {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    out = it->second->GetRootOffset();
    return true;
  }

  bool CalculatePoseJointMatrices(uint32_t pose_id) {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    it->second->CalculateJointMatrices();
    return true;
  }

  int GetPoseJointCount(uint32_t pose_id) const {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return -1;
    return (int)it->second->GetJointCount();
  }

  // --- Extended Ragdoll API ---

  bool RagdollSetPose(uint32_t ragdoll_id, uint32_t pose_id, bool lock_bodies) {
    auto ri = mRagdolls.find(ragdoll_id);
    if (ri == mRagdolls.end()) return false;
    auto pi = mSkeletonPoses.find(pose_id);
    if (pi == mSkeletonPoses.end()) return false;
    ri->second->SetPose(*pi->second, lock_bodies);
    return true;
  }

  bool RagdollGetPose(uint32_t ragdoll_id, uint32_t pose_id, bool lock_bodies) {
    auto ri = mRagdolls.find(ragdoll_id);
    if (ri == mRagdolls.end()) return false;
    auto pi = mSkeletonPoses.find(pose_id);
    if (pi == mSkeletonPoses.end()) return false;
    ri->second->GetPose(*pi->second, lock_bodies);
    return true;
  }

  bool RagdollDriveToPoseKinematics(uint32_t ragdoll_id, uint32_t pose_id, float dt, bool lock_bodies) {
    auto ri = mRagdolls.find(ragdoll_id);
    if (ri == mRagdolls.end()) return false;
    auto pi = mSkeletonPoses.find(pose_id);
    if (pi == mSkeletonPoses.end()) return false;
    ri->second->DriveToPoseUsingKinematics(*pi->second, dt, lock_bodies);
    return true;
  }

  bool RagdollDriveToPoseMotors(uint32_t ragdoll_id, uint32_t pose_id) {
    auto ri = mRagdolls.find(ragdoll_id);
    if (ri == mRagdolls.end()) return false;
    auto pi = mSkeletonPoses.find(pose_id);
    if (pi == mSkeletonPoses.end()) return false;
    ri->second->DriveToPoseUsingMotors(*pi->second);
    return true;
  }

  bool RagdollActivate(uint32_t ragdoll_id, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->Activate(lock_bodies);
    return true;
  }

  int RagdollIsActive(uint32_t ragdoll_id) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return -1;
    return it->second->IsActive() ? 1 : 0;
  }

  bool RagdollGetRootTransform(uint32_t ragdoll_id, RVec3 &out_pos, Quat &out_rot) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->GetRootTransform(out_pos, out_rot);
    return true;
  }

  bool RagdollGetWorldSpaceBounds(uint32_t ragdoll_id, Vec3 &out_min, Vec3 &out_max) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    AABox box = it->second->GetWorldSpaceBounds();
    out_min = box.mMin;
    out_max = box.mMax;
    return true;
  }

  bool RagdollSetGroupID(uint32_t ragdoll_id, uint32_t group_id, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->SetGroupID(static_cast<CollisionGroup::GroupID>(group_id), lock_bodies);
    return true;
  }

  bool RagdollResetWarmStart(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->ResetWarmStart();
    return true;
  }

  bool RagdollSetLinearVelocity(uint32_t ragdoll_id, float vx, float vy, float vz, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->SetLinearVelocity(Vec3(vx, vy, vz), lock_bodies);
    return true;
  }

  bool RagdollAddLinearVelocity(uint32_t ragdoll_id, float vx, float vy, float vz, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->AddLinearVelocity(Vec3(vx, vy, vz), lock_bodies);
    return true;
  }

  bool RagdollSetLinearAndAngularVelocity(uint32_t ragdoll_id,
      float lvx, float lvy, float lvz, float avx, float avy, float avz, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->SetLinearAndAngularVelocity(Vec3(lvx, lvy, lvz), Vec3(avx, avy, avz), lock_bodies);
    return true;
  }

  bool RagdollAddImpulse(uint32_t ragdoll_id, float ix, float iy, float iz, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->AddImpulse(Vec3(ix, iy, iz), lock_bodies);
    return true;
  }

  bool RagdollAddToPhysicsSystem(uint32_t ragdoll_id, bool activate) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->AddToPhysicsSystem(activate ? EActivation::Activate : EActivation::DontActivate);
    return true;
  }

  bool RagdollRemoveFromPhysicsSystem(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->RemoveFromPhysicsSystem();
    return true;
  }

  bool RagdollStabilize(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    return const_cast<RagdollSettings *>(it->second->GetRagdollSettings())->Stabilize();
  }

  */
  /*
  // --- Serialization ---

  // Compact state snapshot (56 bytes/body).
  // Per body: bodyId(u32) posX posY posZ(f32×3) rotX rotY rotZ rotW(f32×4) lvX lvY lvZ(f32×3) avX avY avZ(f32×3)
  std::vector<uint8_t> SnapshotState() const {
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
  }

  bool ApplySnapshot(const uint8_t *data, size_t len) {
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
  }

  // Full scene save using Jolt PhysicsScene (current pos/rot/vel + shapes).
  // Note: custom constraints from mConstraints are NOT included.
  std::vector<uint8_t> SaveScene() const {
    PhysicsScene scene;
    scene.FromPhysicsSystem(&mPhysicsSystem);
    std::ostringstream oss(std::ios::binary);
    StreamOutWrapper stream(oss);
    // stream, inSaveShapes, inSaveGroupFilter
    scene.SaveBinaryState(stream, true, true);
    const std::string &str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
  }

  // Restore bodies from saved binary scene data. Returns body count created, -1 on error.
  int LoadScene(const uint8_t *data, size_t len) {
    std::string str(reinterpret_cast<const char *>(data), len);
    std::istringstream iss(str, std::ios::binary);
    StreamInWrapper stream(iss);
    auto result = PhysicsScene::sRestoreFromBinaryState(stream);
    if (result.HasError()) return -1;
    Ref<PhysicsScene> scene = result.Get();
    if (!scene->CreateBodies(&mPhysicsSystem)) return -1;
    return (int)scene->GetNumBodies();
  }

  */
  /*
  // ── CharacterVirtual ──────────────────────────────────────────────────────

  uint32_t CreateCharacter(float half_height, float radius, double x, double y, double z,
                           float mass, float max_strength, float max_slope_angle) {
    CapsuleShapeSettings cs(half_height, radius);
    auto cr = cs.Create();
    if (cr.HasError()) return 0;
    CharacterVirtualSettings settings;
    settings.mMass = mass;
    settings.mMaxStrength = max_strength;
    settings.mMaxSlopeAngle = max_slope_angle;
    settings.mShape = cr.Get();
    settings.mSupportingVolume = Plane(Vec3::sAxisY(), -radius);
    const uint32_t id = mNextCharacterId++;
    mCharacters[id] = std::make_unique<CharacterVirtual>(
        &settings, RVec3(x, y, z), Quat::sIdentity(), 0, &mPhysicsSystem);
    return id;
  }

  bool DestroyCharacter(uint32_t id) { return mCharacters.erase(id) > 0; }

  bool CharacterUpdate(uint32_t id, float dt) {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return false;
    CharacterVirtual *ch = it->second.get();
    Vec3 gravity = mPhysicsSystem.GetGravity();
    if (ch->GetGroundState() != CharacterBase::EGroundState::OnGround)
      ch->SetLinearVelocity(ch->GetLinearVelocity() + gravity * dt);
    DefaultBroadPhaseLayerFilter bp_filter(mObjectVsBroadPhaseLayerFilter, Layers::MOVING);
    DefaultObjectLayerFilter obj_filter(mObjectLayerPairFilter, Layers::MOVING);
    BodyFilter body_filter;
    ShapeFilter shape_filter;
    ch->Update(dt, gravity, bp_filter, obj_filter, body_filter, shape_filter, mTempAllocator);
    return true;
  }

  bool SetCharacterLinearVelocity(uint32_t id, float vx, float vy, float vz) {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return false;
    it->second->SetLinearVelocity(Vec3(vx, vy, vz));
    return true;
  }

  bool GetCharacterLinearVelocity(uint32_t id, Vec3 &out) const {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return false;
    out = it->second->GetLinearVelocity();
    return true;
  }

  bool SetCharacterPosition(uint32_t id, double x, double y, double z) {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return false;
    it->second->SetPosition(RVec3(x, y, z));
    DefaultBroadPhaseLayerFilter bp_filter(mObjectVsBroadPhaseLayerFilter, Layers::MOVING);
    DefaultObjectLayerFilter obj_filter(mObjectLayerPairFilter, Layers::MOVING);
    BodyFilter body_filter;
    ShapeFilter shape_filter;
    it->second->RefreshContacts(bp_filter, obj_filter, body_filter, shape_filter, mTempAllocator);
    return true;
  }

  bool GetCharacterPosition(uint32_t id, RVec3 &out) const {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return false;
    out = it->second->GetPosition();
    return true;
  }

  bool SetCharacterRotation(uint32_t id, float rx, float ry, float rz, float rw) {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return false;
    it->second->SetRotation(Quat(rx, ry, rz, rw));
    return true;
  }

  bool GetCharacterRotation(uint32_t id, Quat &out) const {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return false;
    out = it->second->GetRotation();
    return true;
  }

  int GetCharacterGroundState(uint32_t id) const {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return -1;
    return static_cast<int>(it->second->GetGroundState());
  }

  bool GetCharacterGroundNormal(uint32_t id, Vec3 &out) const {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return false;
    out = it->second->GetGroundNormal();
    return true;
  }

  uint32_t GetCharacterGroundBodyId(uint32_t id) const {
    auto it = mCharacters.find(id);
    if (it == mCharacters.end()) return 0;
    const BodyID bid = it->second->GetGroundBodyID();
    return bid.IsInvalid() ? 0 : bid.GetIndexAndSequenceNumber();
  }
  */
  /*
#ifdef JPH_DEBUG_RENDERER
  // ── DebugRenderer ─────────────────────────────────────────────────────────

  struct DebugGeoResult {
    std::vector<float>    linePos, triPos;
    std::vector<uint32_t> lineCol, triCol;
  };

  DebugGeoResult GetDebugGeometry(bool draw_bodies, bool draw_constraints,
                                   bool draw_constraint_limits, bool wireframe) {
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
  }
#endif
  */
 private:
  /*
  template <class T>
  T *GetConstraintAs(uint32_t constraint_id, EConstraintSubType subtype) {
    auto it = mConstraints.find(constraint_id);
    if (it == mConstraints.end() || it->second == nullptr || it->second->GetSubType() != subtype) return nullptr;
    return static_cast<T *>(it->second.GetPtr());
  }
  */
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

  void SetUInt32(napi_value payload, const char *key, uint32_t value) const {
    napi_value v;
    napi_create_uint32(mEnv, value, &v);
    napi_set_named_property(mEnv, payload, key, v);
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
  /*
  uint32_t StoreConstraint(const Ref<Constraint> &constraint) {
    if (constraint == nullptr) return 0;
    const uint32_t id = mNextConstraintId++;
    mPhysicsSystem.AddConstraint(constraint.GetPtr());
    mConstraints[id] = constraint;
    return id;
  }
  */
  /*
  uint32_t CreateBodyFromShape(const Shape *shape, double x, double y, double z, bool dynamic, float restitution, float friction) {
    BodyCreationSettings settings(
        shape,
        RVec3(x, y, z),
        Quat::sIdentity(),
        dynamic ? EMotionType::Dynamic : EMotionType::Static,
        dynamic ? Layers::MOVING : Layers::NON_MOVING);

    settings.mRestitution = restitution;
    settings.mFriction = friction;

    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    const BodyID id = bi.CreateAndAddBody(settings, dynamic ? EActivation::Activate : EActivation::DontActivate);
    return id.GetIndexAndSequenceNumber();
  }

  void CreateGround() {
    CreateBox(100.0f, 1.0f, 100.0f, 0.0, -1.0, 0.0, false, 0.0f, 0.5f);
  }
  */

  TempAllocatorImpl mTempAllocator;
  JobSystemThreadPool mJobSystem;
  BPLayerInterfaceImpl mBroadPhaseLayerInterface;
  ObjectVsBroadPhaseLayerFilterImpl mObjectVsBroadPhaseLayerFilter;
  ObjectLayerPairFilterImpl mObjectLayerPairFilter;
  PhysicsSystem mPhysicsSystem;
  ActivationListenerImpl mActivationListener;
  ContactListenerImpl mContactListener;
  napi_env mEnv = nullptr;
  napi_ref mBodyActivationCallbackRef = nullptr;
  napi_ref mContactCallbackRef = nullptr;
  std::mutex mPendingEventsMutex;
  std::vector<PendingEvent> mPendingEvents;

  uint32_t mNextConstraintId = 1;
  std::unordered_map<uint32_t, Ref<Constraint>> mConstraints;

  uint32_t mNextSkeletonId = 1;
  std::unordered_map<uint32_t, Ref<Skeleton>> mSkeletons;

  uint32_t mNextRagdollSettingsId = 1;
  std::unordered_map<uint32_t, Ref<RagdollSettings>> mRagdollSettings;

  uint32_t mNextRagdollId = 1;
  std::unordered_map<uint32_t, Ref<Ragdoll>> mRagdolls;
  std::unordered_map<uint32_t, std::vector<uint32_t>> mRagdollConstraintIds;

  uint32_t mNextSkeletonPoseId = 1;
  std::unordered_map<uint32_t, std::unique_ptr<SkeletonPose>> mSkeletonPoses;

  std::unordered_map<uint32_t, Ref<MutableCompoundShape>> mMutableCompounds;

  uint32_t mNextCharacterId = 1;
  std::unordered_map<uint32_t, std::unique_ptr<CharacterVirtual>> mCharacters;
};


