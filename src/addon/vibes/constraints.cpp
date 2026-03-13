
// Motor spring/damping settings parsed from JS object.
// JS keys: frequency | stiffness, damping, maxForce, minForce, maxTorque, minTorque
struct MotorSpringParams {
  ESpringMode mode = ESpringMode::FrequencyAndDamping;
  float frequency = 2.0f;   // or stiffness when mode=StiffnessAndDamping
  float damping = 1.0f;
  float min_force = -FLT_MAX, max_force = FLT_MAX;
  float min_torque = -FLT_MAX, max_torque = FLT_MAX;
};

// Per-joint constraint config for ragdoll — parsed from JS config object.
// type: 0=fixed, 1=swingTwist, 2=hinge, 3=cone
struct RagdollJointConstraintConfig {
  int type = 0;
  float normalHalfCone = 0.5f, planeHalfCone = 0.3f;
  float twistMin = -0.5f, twistMax = 0.5f;
  Vec3 twistAxis1, twistAxis2, planeAxis1, planeAxis2;
  float hingeMin = -3.14159265f, hingeMax = 3.14159265f;
  Vec3 hingeAxis1, hingeAxis2, normalAxis1, normalAxis2;
  float halfConeAngle = 0.5f;
  Vec3 coneAxis1, coneAxis2;
  bool autoAxes = true; // compute axes from bone direction when true
};


static void ParseSpringSettings(napi_env env, napi_value obj, SpringSettings &out) {
  napi_valuetype vt = napi_undefined;
  if (napi_typeof(env, obj, &vt) != napi_ok || vt != napi_object) return;
  auto getF = [&](const char *key, float &dest) {
    napi_value v;
    if (napi_get_named_property(env, obj, key, &v) == napi_ok) {
      double d = 0.0;
      if (GetDoubleArg(env, v, &d)) dest = static_cast<float>(d);
    }
  };
  napi_value mode_v;
  if (napi_get_named_property(env, obj, "mode", &mode_v) == napi_ok) {
    std::string ms;
    if (GetStringArg(env, mode_v, &ms) && ms == "stiffness")
      out.mMode = ESpringMode::StiffnessAndDamping;
  }
  napi_value stiff_v;
  if (napi_get_named_property(env, obj, "stiffness", &stiff_v) == napi_ok) {
    double d = 0.0;
    if (GetDoubleArg(env, stiff_v, &d)) {
      out.mMode = ESpringMode::StiffnessAndDamping;
      out.mFrequency = static_cast<float>(d);
    }
  }
  getF("frequency", out.mFrequency);
  getF("damping", out.mDamping);
}

static napi_value MakeSpringSettingsObject(napi_env env, const SpringSettings &ss) {
  napi_value out; napi_create_object(env, &out);
  const char *mode_str = (ss.mMode == ESpringMode::StiffnessAndDamping) ? "stiffness" : "frequency";
  napi_value mv; napi_create_string_utf8(env, mode_str, NAPI_AUTO_LENGTH, &mv);
  napi_set_named_property(env, out, "mode", mv);
  napi_value fv, dv;
  napi_create_double(env, static_cast<double>(ss.mFrequency), &fv);
  napi_create_double(env, static_cast<double>(ss.mDamping), &dv);
  napi_set_named_property(env, out, "frequency", fv);
  napi_set_named_property(env, out, "damping", dv);
  return out;
}


static void ParseRagdollJointConstraintConfig(napi_env env, napi_value obj, RagdollJointConstraintConfig &out) {
  napi_valuetype vt = napi_undefined;
  if (napi_typeof(env, obj, &vt) != napi_ok || vt != napi_object) return;

  napi_value tv;
  if (napi_get_named_property(env, obj, "type", &tv) == napi_ok) {
    std::string ts;
    if (GetStringArg(env, tv, &ts)) {
      if (ts == "swingTwist") out.type = 1;
      else if (ts == "hinge") out.type = 2;
      else if (ts == "cone") out.type = 3;
      else out.type = 0;
    }
  }

  auto getF = [&](const char *key, float &dest) {
    napi_value v;
    if (napi_get_named_property(env, obj, key, &v) == napi_ok) {
      double d = 0.0;
      if (GetDoubleArg(env, v, &d)) dest = static_cast<float>(d);
    }
  };
  getF("normalHalfCone", out.normalHalfCone);
  getF("planeHalfCone", out.planeHalfCone);
  getF("twistMin", out.twistMin);
  getF("twistMax", out.twistMax);
  getF("minAngle", out.hingeMin);
  getF("maxAngle", out.hingeMax);
  getF("halfConeAngle", out.halfConeAngle);

  bool anyAxis = false;
  if (GetVec3ObjProp(env, obj, "twistAxis1", out.twistAxis1)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "twistAxis2", out.twistAxis2)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "planeAxis1", out.planeAxis1)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "planeAxis2", out.planeAxis2)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "hingeAxis1", out.hingeAxis1)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "hingeAxis2", out.hingeAxis2)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "normalAxis1", out.normalAxis1)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "normalAxis2", out.normalAxis2)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "coneAxis1", out.coneAxis1)) anyAxis = true;
  if (GetVec3ObjProp(env, obj, "coneAxis2", out.coneAxis2)) anyAxis = true;

  napi_value av;
  if (napi_get_named_property(env, obj, "autoAxes", &av) == napi_ok) {
    bool b = true;
    if (GetBoolArg(env, av, &b)) out.autoAxes = b;
  } else if (anyAxis) {
    out.autoAxes = false;
  }
}

void ParseMotorSpringParams(napi_env env, napi_value obj, MotorSpringParams &out) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, obj, &type) != napi_ok || type != napi_object) return;

  auto getOpt = [&](const char *key, float &dest) {
    napi_value v;
    if (napi_get_named_property(env, obj, key, &v) == napi_ok) {
      double d = 0.0;
      if (GetDoubleArg(env, v, &d)) dest = static_cast<float>(d);
    }
  };


  // Mode
  napi_value mode_v;
  if (napi_get_named_property(env, obj, "mode", &mode_v) == napi_ok) {
    std::string mode_str;
    if (GetStringArg(env, mode_v, &mode_str) && mode_str == "stiffness")
      out.mode = ESpringMode::StiffnessAndDamping;
  }

  // Auto-detect mode from key names if not explicit
  napi_value stiff_v;
  if (napi_get_named_property(env, obj, "stiffness", &stiff_v) == napi_ok) {
    double d = 0.0;
    if (GetDoubleArg(env, stiff_v, &d)) {
      out.mode = ESpringMode::StiffnessAndDamping;
      out.frequency = static_cast<float>(d);
    }
  }
  getOpt("frequency", out.frequency);
  getOpt("damping", out.damping);

  // Force limits: symmetric shorthand or asymmetric
  float max_force = FLT_MAX;
  getOpt("maxForce", max_force);
  if (max_force != FLT_MAX) { out.min_force = -max_force; out.max_force = max_force; }
  getOpt("minForce", out.min_force);
  getOpt("maxForce", out.max_force);

  // Torque limits
  float max_torque = FLT_MAX;
  getOpt("maxTorque", max_torque);
  if (max_torque != FLT_MAX) { out.min_torque = -max_torque; out.max_torque = max_torque; }
  getOpt("minTorque", out.min_torque);
  getOpt("maxTorque", out.max_torque);
}

void ApplyMotorSpring(MotorSettings &ms, const MotorSpringParams &p) {
  ms.mSpringSettings = SpringSettings(p.mode, p.frequency, p.damping);
  ms.mMinForceLimit = p.min_force;
  ms.mMaxForceLimit = p.max_force;
  ms.mMinTorqueLimit = p.min_torque;
  ms.mMaxTorqueLimit = p.max_torque;
}


napi_value MakeMotorSpringObject(napi_env env, const MotorSettings &ms) {
  napi_value obj;
  napi_create_object(env, &obj);

  napi_value mode_v;
  const char *mode_str = (ms.mSpringSettings.mMode == ESpringMode::StiffnessAndDamping) ? "stiffness" : "frequency";
  napi_create_string_utf8(env, mode_str, NAPI_AUTO_LENGTH, &mode_v);
  napi_set_named_property(env, obj, "mode", mode_v);

  napi_value freq_v, damp_v, minF_v, maxF_v, minT_v, maxT_v;
  napi_create_double(env, ms.mSpringSettings.mFrequency, &freq_v);
  napi_create_double(env, ms.mSpringSettings.mDamping, &damp_v);
  napi_create_double(env, ms.mMinForceLimit, &minF_v);
  napi_create_double(env, ms.mMaxForceLimit, &maxF_v);
  napi_create_double(env, ms.mMinTorqueLimit, &minT_v);
  napi_create_double(env, ms.mMaxTorqueLimit, &maxT_v);

  napi_set_named_property(env, obj, "frequency", freq_v);
  napi_set_named_property(env, obj, "damping", damp_v);
  napi_set_named_property(env, obj, "minForceLimit", minF_v);
  napi_set_named_property(env, obj, "maxForceLimit", maxF_v);
  napi_set_named_property(env, obj, "minTorqueLimit", minT_v);
  napi_set_named_property(env, obj, "maxTorqueLimit", maxT_v);
  return obj;
}


