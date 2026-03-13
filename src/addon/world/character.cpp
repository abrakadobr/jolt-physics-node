class VirtualCharacter {


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




}
