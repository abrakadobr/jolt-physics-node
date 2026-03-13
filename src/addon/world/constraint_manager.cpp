class ConstraintManger {


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


  template <class T>
  T *GetConstraintAs(uint32_t constraint_id, EConstraintSubType subtype) {
    auto it = mConstraints.find(constraint_id);
    if (it == mConstraints.end() || it->second == nullptr || it->second->GetSubType() != subtype) return nullptr;
    return static_cast<T *>(it->second.GetPtr());
  }


  uint32_t StoreConstraint(const Ref<Constraint> &constraint) {
    if (constraint == nullptr) return 0;
    const uint32_t id = mNextConstraintId++;
    mPhysicsSystem.AddConstraint(constraint.GetPtr());
    mConstraints[id] = constraint;
    return id;
  }
 


}
