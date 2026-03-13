
class PhysicsManager {

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



}
