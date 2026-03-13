class BodyManager {

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


  uint32_t CreateSkeleton() {
    const uint32_t id = mNextSkeletonId++;
    mSkeletons[id] = new Skeleton();
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


  void CreateGround() {
    CreateBox(100.0f, 1.0f, 100.0f, 0.0, -1.0, 0.0, false, 0.0f, 0.5f);
  }




}
