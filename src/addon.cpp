#include <node_api.h>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/GearConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/PathConstraint.h>
#include <Jolt/Physics/Constraints/PathConstraintPathHermite.h>
#include <Jolt/Physics/Constraints/RackAndPinionConstraint.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Skeleton/Skeleton.h>
#include <Jolt/Skeleton/SkeletonPose.h>
#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/Core/StreamWrapper.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace JPH;

namespace {

void ThrowTypeError(napi_env env, const char *message) { napi_throw_type_error(env, nullptr, message); }
void ThrowError(napi_env env, const char *message) { napi_throw_error(env, nullptr, message); }

bool GetDoubleArg(napi_env env, napi_value value, double *out) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, value, &type) != napi_ok || type != napi_number) return false;
  return napi_get_value_double(env, value, out) == napi_ok;
}

bool GetBoolArg(napi_env env, napi_value value, bool *out) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, value, &type) != napi_ok || type != napi_boolean) return false;
  return napi_get_value_bool(env, value, out) == napi_ok;
}

bool GetUInt32Arg(napi_env env, napi_value value, uint32_t *out) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, value, &type) != napi_ok || type != napi_number) return false;
  return napi_get_value_uint32(env, value, out) == napi_ok;
}

bool GetInt32Arg(napi_env env, napi_value value, int32_t *out) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, value, &type) != napi_ok || type != napi_number) return false;
  return napi_get_value_int32(env, value, out) == napi_ok;
}

bool GetStringArg(napi_env env, napi_value value, std::string *out) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, value, &type) != napi_ok || type != napi_string) return false;
  size_t size = 0;
  if (napi_get_value_string_utf8(env, value, nullptr, 0, &size) != napi_ok) return false;
  out->resize(size);
  size_t written = 0;
  if (napi_get_value_string_utf8(env, value, out->data(), size + 1, &written) != napi_ok) return false;
  out->resize(written);
  return true;
}

napi_value MakeVec3Object(napi_env env, const RVec3 &p) {
  napi_value obj, x, y, z;
  napi_create_object(env, &obj);
  napi_create_double(env, static_cast<double>(p.GetX()), &x);
  napi_create_double(env, static_cast<double>(p.GetY()), &y);
  napi_create_double(env, static_cast<double>(p.GetZ()), &z);
  napi_set_named_property(env, obj, "x", x);
  napi_set_named_property(env, obj, "y", y);
  napi_set_named_property(env, obj, "z", z);
  return obj;
}

napi_value MakeQuatObject(napi_env env, QuatArg q) {
  napi_value obj, x, y, z, w;
  napi_create_object(env, &obj);
  napi_create_double(env, static_cast<double>(q.GetX()), &x);
  napi_create_double(env, static_cast<double>(q.GetY()), &y);
  napi_create_double(env, static_cast<double>(q.GetZ()), &z);
  napi_create_double(env, static_cast<double>(q.GetW()), &w);
  napi_set_named_property(env, obj, "x", x);
  napi_set_named_property(env, obj, "y", y);
  napi_set_named_property(env, obj, "z", z);
  napi_set_named_property(env, obj, "w", w);
  return obj;
}

namespace Layers {
constexpr ObjectLayer NON_MOVING = 0;
constexpr ObjectLayer MOVING = 1;
constexpr ObjectLayer NUM_LAYERS = 2;
}  // namespace Layers

namespace BroadPhaseLayers {
constexpr BroadPhaseLayer NON_MOVING(0);
constexpr BroadPhaseLayer MOVING(1);
constexpr uint NUM_LAYERS = 2;
}  // namespace BroadPhaseLayers

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
 public:
  BPLayerInterfaceImpl() {
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
  }

  uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

  BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override { return mObjectToBroadPhase[inLayer]; }

 private:
  BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
 public:
  bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
    switch (inLayer1) {
      case Layers::NON_MOVING:
        return inLayer2 == BroadPhaseLayers::MOVING;
      case Layers::MOVING:
        return true;
      default:
        return false;
    }
  }
};

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
 public:
  bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
    switch (inObject1) {
      case Layers::NON_MOVING:
        return inObject2 == Layers::MOVING;
      case Layers::MOVING:
        return true;
      default:
        return false;
    }
  }
};

// Filters object layers by bitmask: bit N = layer N is allowed.
class MaskObjectLayerFilter final : public ObjectLayerFilter {
 public:
  explicit MaskObjectLayerFilter(uint32_t mask) : mMask(mask) {}
  bool ShouldCollide(ObjectLayer inLayer) const override {
    return inLayer < 32 && (mMask & (1u << inLayer)) != 0;
  }

 private:
  uint32_t mMask;
};

struct QueryFilters {
  uint32_t layer_mask = 0xFFFFFFFFu;  // all layers by default
  std::vector<uint32_t> exclude_ids;
};

// Parse optional query filter object { layerMask?: uint32, excludeBodyIds?: uint32[] }
bool ParseQueryFilters(napi_env env, napi_value opts, QueryFilters &out) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, opts, &type) != napi_ok || type != napi_object) return true;

  napi_value lm_v;
  if (napi_get_named_property(env, opts, "layerMask", &lm_v) == napi_ok) {
    uint32_t mask = 0;
    if (GetUInt32Arg(env, lm_v, &mask)) out.layer_mask = mask;
  }

  napi_value exc_v;
  if (napi_get_named_property(env, opts, "excludeBodyIds", &exc_v) == napi_ok) {
    bool is_array = false;
    if (napi_is_array(env, exc_v, &is_array) == napi_ok && is_array) {
      uint32_t len = 0;
      napi_get_array_length(env, exc_v, &len);
      out.exclude_ids.reserve(len);
      for (uint32_t i = 0; i < len; ++i) {
        napi_value elem;
        if (napi_get_element(env, exc_v, i, &elem) == napi_ok) {
          uint32_t id = 0;
          if (GetUInt32Arg(env, elem, &id)) out.exclude_ids.push_back(id);
        }
      }
    }
  }
  return true;
}

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

static bool GetVec3ObjProp(napi_env env, napi_value obj, const char *key, Vec3 &out) {
  napi_value v;
  if (napi_get_named_property(env, obj, key, &v) != napi_ok) return false;
  napi_valuetype vt = napi_undefined;
  if (napi_typeof(env, v, &vt) != napi_ok || vt != napi_object) return false;
  napi_value xv, yv, zv;
  double x = 0, y = 0, z = 0;
  if (napi_get_named_property(env, v, "x", &xv) != napi_ok || !GetDoubleArg(env, xv, &x)) return false;
  if (napi_get_named_property(env, v, "y", &yv) != napi_ok || !GetDoubleArg(env, yv, &y)) return false;
  if (napi_get_named_property(env, v, "z", &zv) != napi_ok || !GetDoubleArg(env, zv, &z)) return false;
  out = Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
  return true;
}

static bool GetQuatObjProp(napi_env env, napi_value obj, const char *key, Quat &out) {
  napi_value v;
  if (napi_get_named_property(env, obj, key, &v) != napi_ok) return false;
  napi_valuetype vt = napi_undefined;
  if (napi_typeof(env, v, &vt) != napi_ok || vt != napi_object) return false;
  napi_value xv, yv, zv, wv;
  double x = 0, y = 0, z = 0, w = 1;
  if (napi_get_named_property(env, v, "x", &xv) == napi_ok) { double d; if (GetDoubleArg(env, xv, &d)) x = d; }
  if (napi_get_named_property(env, v, "y", &yv) == napi_ok) { double d; if (GetDoubleArg(env, yv, &d)) y = d; }
  if (napi_get_named_property(env, v, "z", &zv) == napi_ok) { double d; if (GetDoubleArg(env, zv, &d)) z = d; }
  if (napi_get_named_property(env, v, "w", &wv) == napi_ok) { double d; if (GetDoubleArg(env, wv, &d)) w = d; }
  out = Quat(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w));
  return true;
}

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

struct SubShapeSpec {
  Ref<Shape> shape;
  Vec3 pos = Vec3::sZero();
  Quat rot = Quat::sIdentity();
};

static bool ParseSubShapeSpec(napi_env env, napi_value obj, SubShapeSpec &out) {
  std::string kind;
  napi_value kind_v;
  if (napi_get_named_property(env, obj, "kind", &kind_v) != napi_ok || !GetStringArg(env, kind_v, &kind)) return false;
  GetVec3ObjProp(env, obj, "position", out.pos);
  GetQuatObjProp(env, obj, "rotation", out.rot);
  auto getF = [&](const char *key, float def) -> float {
    napi_value v;
    if (napi_get_named_property(env, obj, key, &v) == napi_ok) {
      double d = 0.0;
      if (GetDoubleArg(env, v, &d)) return static_cast<float>(d);
    }
    return def;
  };
  ShapeSettings::ShapeResult result;
  if (kind == "sphere") {
    result = SphereShapeSettings(getF("radius", 0.5f)).Create();
  } else if (kind == "box") {
    Vec3 he(0.5f, 0.5f, 0.5f);
    GetVec3ObjProp(env, obj, "halfExtents", he);
    result = BoxShapeSettings(he).Create();
  } else if (kind == "capsule") {
    result = CapsuleShapeSettings(getF("halfHeight", 0.5f), getF("radius", 0.2f)).Create();
  } else if (kind == "cylinder") {
    result = CylinderShapeSettings(getF("halfHeight", 0.5f), getF("radius", 0.2f)).Create();
  } else {
    return false;
  }
  if (result.HasError()) return false;
  out.shape = result.Get();
  return true;
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

static inline void SetF64Prop(napi_env env, napi_value obj, const char *key, float val) {
  napi_value v; napi_create_double(env, static_cast<double>(val), &v);
  napi_set_named_property(env, obj, key, v);
}

static inline napi_value MakeVec2Object(napi_env env, float x, float y) {
  napi_value obj, vx, vy;
  napi_create_object(env, &obj);
  napi_create_double(env, static_cast<double>(x), &vx);
  napi_create_double(env, static_cast<double>(y), &vy);
  napi_set_named_property(env, obj, "x", vx);
  napi_set_named_property(env, obj, "y", vy);
  return obj;
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

class IndexedMaterial : public PhysicsMaterial {
 public:
  IndexedMaterial(uint32_t idx, float friction, float restitution)
      : mIndex(idx), mFriction(friction), mRestitution(restitution) {}
  virtual const char *GetDebugName() const override { return "IndexedMaterial"; }
  uint32_t mIndex;
  float mFriction;
  float mRestitution;
};

static uint32_t GetHitMaterialIndex(const Body &body, const SubShapeID &sub_shape_id) {
  const PhysicsMaterial *mat = body.GetShape()->GetMaterial(sub_shape_id);
  if (!mat || mat == PhysicsMaterial::sDefault) return 0;
  return static_cast<const IndexedMaterial *>(mat)->mIndex;
}

#ifdef JPH_DEBUG_RENDERER
class CollectingDebugRenderer : public DebugRenderer {
 public:
  CollectingDebugRenderer() { Initialize(); }
  struct LineV  { float x1,y1,z1,x2,y2,z2; uint32_t color; };
  struct TriV   { float x1,y1,z1,x2,y2,z2,x3,y3,z3; uint32_t color; };
  std::vector<LineV> lines;
  std::vector<TriV>  tris;
  void Clear() { lines.clear(); tris.clear(); }

  void DrawLine(RVec3Arg from, RVec3Arg to, ColorArg c) override {
    lines.push_back({(float)from.GetX(),(float)from.GetY(),(float)from.GetZ(),
                     (float)to.GetX(),(float)to.GetY(),(float)to.GetZ(), c.GetUInt32()});
  }
  void DrawTriangle(RVec3Arg v1, RVec3Arg v2, RVec3Arg v3, ColorArg c, ECastShadow) override {
    tris.push_back({(float)v1.GetX(),(float)v1.GetY(),(float)v1.GetZ(),
                    (float)v2.GetX(),(float)v2.GetY(),(float)v2.GetZ(),
                    (float)v3.GetX(),(float)v3.GetY(),(float)v3.GetZ(), c.GetUInt32()});
  }
  void DrawText3D(RVec3Arg, const string_view &, ColorArg, float) override {}

  class BatchImpl : public RefTargetVirtual {
   public:
    std::vector<Triangle> mTriangles;
    std::atomic_int mRefCount{0};
    BatchImpl(const Triangle *t, int n) : mTriangles(t, t+n) {}
    void AddRef() override { ++mRefCount; }
    void Release() override { if (--mRefCount == 0) delete this; }
  };

  Batch CreateTriangleBatch(const Triangle *t, int n) override { return new BatchImpl(t,n); }
  Batch CreateTriangleBatch(const Vertex *v, int, const uint32_t *idx, int ic) override {
    std::vector<Triangle> out; out.reserve(ic/3);
    for (int k=0; k+2<ic; k+=3) {
      Triangle tri;
      for (int j=0; j<3; j++) {
        auto &s=v[idx[k+j]];
        tri.mV[j].mPosition=Float3(s.mPosition.x,s.mPosition.y,s.mPosition.z);
        tri.mV[j].mNormal=Float3(s.mNormal.x,s.mNormal.y,s.mNormal.z);
        tri.mV[j].mColor=s.mColor; tri.mV[j].mUV=Float2(s.mUV.x,s.mUV.y);
      }
      out.push_back(tri);
    }
    return new BatchImpl(out.data(),(int)out.size());
  }
  void DrawGeometry(RMat44Arg mat, const AABox &, float, ColorArg color,
                    const GeometryRef &geom, ECullMode, ECastShadow, EDrawMode) override {
    if (!geom || geom->mLODs.empty()) return;
    const auto *impl=static_cast<const BatchImpl*>(geom->mLODs[0].mTriangleBatch.GetPtr());
    if (!impl) return;
    const uint32_t c=color.GetUInt32();
    for (const Triangle &tri : impl->mTriangles) {
      RVec3 v1=mat*Vec3(tri.mV[0].mPosition.x,tri.mV[0].mPosition.y,tri.mV[0].mPosition.z);
      RVec3 v2=mat*Vec3(tri.mV[1].mPosition.x,tri.mV[1].mPosition.y,tri.mV[1].mPosition.z);
      RVec3 v3=mat*Vec3(tri.mV[2].mPosition.x,tri.mV[2].mPosition.y,tri.mV[2].mPosition.z);
      tris.push_back({(float)v1.GetX(),(float)v1.GetY(),(float)v1.GetZ(),
                      (float)v2.GetX(),(float)v2.GetY(),(float)v2.GetZ(),
                      (float)v3.GetX(),(float)v3.GetY(),(float)v3.GetZ(),c});
    }
  }
};
#endif

/*
class PhysicsWorld {
 public:
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

  class ActivationListenerImpl final : public BodyActivationListener {
   public:
    explicit ActivationListenerImpl(PhysicsWorld *owner) : mOwner(owner) {}

    void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override { mOwner->QueueBodyActivation(true, inBodyID, inBodyUserData); }

    void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override { mOwner->QueueBodyActivation(false, inBodyID, inBodyUserData); }

   private:
    PhysicsWorld *mOwner;
  };

  class ContactListenerImpl final : public ContactListener {
   public:
    explicit ContactListenerImpl(PhysicsWorld *owner) : mOwner(owner) {}

    void OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
      ApplyMaterialSettings(inBody1, inBody2, inManifold, ioSettings);
      mOwner->QueueContactEvent(PendingEventType::ContactAdded, inBody1.GetID(), inBody2.GetID(), inManifold);
    }

    void OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
      ApplyMaterialSettings(inBody1, inBody2, inManifold, ioSettings);
      mOwner->QueueContactEvent(PendingEventType::ContactPersisted, inBody1.GetID(), inBody2.GetID(), inManifold);
    }

    void OnContactRemoved(const SubShapeIDPair &inSubShapePair) override {
      mOwner->QueueContactRemoved(inSubShapePair.GetBody1ID(), inSubShapePair.GetBody2ID());
    }

   private:
    static void ApplyMaterialSettings(const Body &b1, const Body &b2,
                                       const ContactManifold &m, ContactSettings &s) {
      const PhysicsMaterial *mat1 = b1.GetShape()->GetMaterial(m.mSubShapeID1);
      const PhysicsMaterial *mat2 = b2.GetShape()->GetMaterial(m.mSubShapeID2);
      bool has1 = mat1 && mat1 != PhysicsMaterial::sDefault;
      bool has2 = mat2 && mat2 != PhysicsMaterial::sDefault;
      if (!has1 && !has2) return;
      float f1 = has1 ? static_cast<const IndexedMaterial *>(mat1)->mFriction : b1.GetFriction();
      float f2 = has2 ? static_cast<const IndexedMaterial *>(mat2)->mFriction : b2.GetFriction();
      float r1 = has1 ? static_cast<const IndexedMaterial *>(mat1)->mRestitution : b1.GetRestitution();
      float r2 = has2 ? static_cast<const IndexedMaterial *>(mat2)->mRestitution : b2.GetRestitution();
      s.mCombinedFriction = sqrt(f1 * f2);
      s.mCombinedRestitution = max(r1, r2);
    }
    PhysicsWorld *mOwner;
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

 private:
  template <class T>
  T *GetConstraintAs(uint32_t constraint_id, EConstraintSubType subtype) {
    auto it = mConstraints.find(constraint_id);
    if (it == mConstraints.end() || it->second == nullptr || it->second->GetSubType() != subtype) return nullptr;
    return static_cast<T *>(it->second.GetPtr());
  }

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

  uint32_t StoreConstraint(const Ref<Constraint> &constraint) {
    if (constraint == nullptr) return 0;
    const uint32_t id = mNextConstraintId++;
    mPhysicsSystem.AddConstraint(constraint.GetPtr());
    mConstraints[id] = constraint;
    return id;
  }

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
*/

/*
struct WorldHandle {
  PhysicsWorld *world = nullptr;
};

std::mutex gInitMutex;
std::atomic<uint32_t> gWorldCount{0};
bool gJoltInitialized = false;

void InitJoltIfNeeded() {
  std::lock_guard<std::mutex> guard(gInitMutex);
  if (gJoltInitialized) return;
  RegisterDefaultAllocator();
  Factory::sInstance = new Factory();
  RegisterTypes();
  gJoltInitialized = true;
}

void ShutdownJoltIfNeeded() {
  std::lock_guard<std::mutex> guard(gInitMutex);
  if (!gJoltInitialized || gWorldCount.load() != 0) return;
  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
  gJoltInitialized = false;
}

bool GetWorldHandle(napi_env env, napi_value value, WorldHandle **out) {
  void *data = nullptr;
  if (napi_get_value_external(env, value, &data) != napi_ok || data == nullptr) return false;
  auto *handle = static_cast<WorldHandle *>(data);
  if (handle->world == nullptr) {
    ThrowError(env, "World is already destroyed");
    return false;
  }
  *out = handle;
  return true;
}

void FinalizeWorld(napi_env env, void *finalize_data, void *finalize_hint) {
  (void)env;
  (void)finalize_hint;

  auto *handle = static_cast<WorldHandle *>(finalize_data);
  if (handle == nullptr) return;

  if (handle->world != nullptr) {
    delete handle->world;
    handle->world = nullptr;
    gWorldCount.fetch_sub(1);
    ShutdownJoltIfNeeded();
  }

  delete handle;
}

napi_value CreateWorld(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok) return nullptr;

  double gravity = 9.81;
  if (argc == 1 && !GetDoubleArg(env, args[0], &gravity)) {
    ThrowTypeError(env, "createWorld(gravity?): gravity must be a number");
    return nullptr;
  }

  InitJoltIfNeeded();

  auto *handle = new WorldHandle();
  handle->world = new PhysicsWorld(env, static_cast<float>(gravity));
  gWorldCount.fetch_add(1);

  napi_value external;
  if (napi_create_external(env, handle, FinalizeWorld, nullptr, &external) != napi_ok) {
    delete handle->world;
    delete handle;
    gWorldCount.fetch_sub(1);
    ShutdownJoltIfNeeded();
    ThrowError(env, "Failed to create world handle");
    return nullptr;
  }

  return external;
}

napi_value DestroyWorld(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 1) {
    ThrowTypeError(env, "destroyWorld(world) expects 1 argument");
    return nullptr;
  }

  void *data = nullptr;
  if (napi_get_value_external(env, args[0], &data) != napi_ok || data == nullptr) {
    ThrowTypeError(env, "destroyWorld(world): world must be value returned by createWorld");
    return nullptr;
  }

  auto *handle = static_cast<WorldHandle *>(data);
  if (handle->world != nullptr) {
    delete handle->world;
    handle->world = nullptr;
    gWorldCount.fetch_sub(1);
    ShutdownJoltIfNeeded();
  }

  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

#define WORLD_FN_BEGIN(expected)                         \
  size_t argc = expected;                                \
  std::vector<napi_value> args(expected);               \
  if (napi_get_cb_info(env, info, &argc, args.data(), nullptr, nullptr) != napi_ok || argc < expected) { \
    ThrowTypeError(env, "Invalid arguments");          \
    return nullptr;                                      \
  }                                                      \
  WorldHandle *handle = nullptr;                         \
  if (!GetWorldHandle(env, args[0], &handle)) {         \
    ThrowTypeError(env, "world must be value returned by createWorld"); \
    return nullptr;                                      \
  }

// Like WORLD_FN_BEGIN but allows up to maxargs args (>= required are mandatory).
#define WORLD_FN_OPT(required, maxargs)                  \
  size_t argc = maxargs;                                 \
  std::vector<napi_value> args(maxargs);                \
  if (napi_get_cb_info(env, info, &argc, args.data(), nullptr, nullptr) != napi_ok || argc < required) { \
    ThrowTypeError(env, "Invalid arguments");          \
    return nullptr;                                      \
  }                                                      \
  WorldHandle *handle = nullptr;                         \
  if (!GetWorldHandle(env, args[0], &handle)) {         \
    ThrowTypeError(env, "world must be value returned by createWorld"); \
    return nullptr;                                      \
  }

napi_value StepWorld(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double dt = 0.0;
  if (!GetDoubleArg(env, args[1], &dt) || dt <= 0.0) {
    ThrowTypeError(env, "step(world, dt): dt must be positive number");
    return nullptr;
  }
  handle->world->Step(static_cast<float>(dt));
  handle->world->DispatchCallbacks();
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}
*/

/*
napi_value SetGravity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double gravity = 0.0;
  if (!GetDoubleArg(env, args[1], &gravity)) {
    ThrowTypeError(env, "setGravity(world, gravity): gravity must be number");
    return nullptr;
  }
  handle->world->SetGravity(static_cast<float>(gravity));
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

napi_value SetBodyActivationCallback(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  if (!handle->world->SetBodyActivationCallback(args[1])) {
    ThrowTypeError(env, "setBodyActivationCallback: callback must be function, null or undefined");
    return nullptr;
  }
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

napi_value SetContactCallback(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  if (!handle->world->SetContactCallback(args[1])) {
    ThrowTypeError(env, "setContactCallback: callback must be function, null or undefined");
    return nullptr;
  }
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

napi_value CreateSphere(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(8)
  double radius, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &radius) || radius <= 0 || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) ||
      !GetDoubleArg(env, args[4], &z) || !GetBoolArg(env, args[5], &dynamic) || !GetDoubleArg(env, args[6], &restitution) ||
      !GetDoubleArg(env, args[7], &friction)) {
    ThrowTypeError(env, "createSphere: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateSphere(static_cast<float>(radius), x, y, z, dynamic, static_cast<float>(restitution), static_cast<float>(friction));
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create sphere");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateBox(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  double hx, hy, hz, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &hx) || hx <= 0 || !GetDoubleArg(env, args[2], &hy) || hy <= 0 || !GetDoubleArg(env, args[3], &hz) || hz <= 0 ||
      !GetDoubleArg(env, args[4], &x) || !GetDoubleArg(env, args[5], &y) || !GetDoubleArg(env, args[6], &z) || !GetBoolArg(env, args[7], &dynamic) ||
      !GetDoubleArg(env, args[8], &restitution) || !GetDoubleArg(env, args[9], &friction)) {
    ThrowTypeError(env, "createBox: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateBox(
      static_cast<float>(hx),
      static_cast<float>(hy),
      static_cast<float>(hz),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create box");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateCapsule(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(9)
  double half_h, radius, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &half_h) || half_h < 0 || !GetDoubleArg(env, args[2], &radius) || radius <= 0 ||
      !GetDoubleArg(env, args[3], &x) || !GetDoubleArg(env, args[4], &y) || !GetDoubleArg(env, args[5], &z) ||
      !GetBoolArg(env, args[6], &dynamic) || !GetDoubleArg(env, args[7], &restitution) || !GetDoubleArg(env, args[8], &friction)) {
    ThrowTypeError(env, "createCapsule: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateCapsule(
      static_cast<float>(half_h),
      static_cast<float>(radius),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create capsule");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateCylinder(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(9)
  double half_h, radius, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &half_h) || half_h < 0 || !GetDoubleArg(env, args[2], &radius) || radius <= 0 ||
      !GetDoubleArg(env, args[3], &x) || !GetDoubleArg(env, args[4], &y) || !GetDoubleArg(env, args[5], &z) ||
      !GetBoolArg(env, args[6], &dynamic) || !GetDoubleArg(env, args[7], &restitution) || !GetDoubleArg(env, args[8], &friction)) {
    ThrowTypeError(env, "createCylinder: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateCylinder(
      static_cast<float>(half_h),
      static_cast<float>(radius),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create cylinder");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateTaperedCapsule(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  double half_h, top_r, bottom_r, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &half_h) || half_h < 0 || !GetDoubleArg(env, args[2], &top_r) || top_r <= 0 ||
      !GetDoubleArg(env, args[3], &bottom_r) || bottom_r <= 0 || !GetDoubleArg(env, args[4], &x) || !GetDoubleArg(env, args[5], &y) ||
      !GetDoubleArg(env, args[6], &z) || !GetBoolArg(env, args[7], &dynamic) || !GetDoubleArg(env, args[8], &restitution) ||
      !GetDoubleArg(env, args[9], &friction)) {
    ThrowTypeError(env, "createTaperedCapsule: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateTaperedCapsule(
      static_cast<float>(half_h),
      static_cast<float>(top_r),
      static_cast<float>(bottom_r),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create tapered capsule");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateTaperedCylinder(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  double half_h, top_r, bottom_r, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &half_h) || half_h < 0 || !GetDoubleArg(env, args[2], &top_r) || top_r <= 0 ||
      !GetDoubleArg(env, args[3], &bottom_r) || bottom_r <= 0 || !GetDoubleArg(env, args[4], &x) || !GetDoubleArg(env, args[5], &y) ||
      !GetDoubleArg(env, args[6], &z) || !GetBoolArg(env, args[7], &dynamic) || !GetDoubleArg(env, args[8], &restitution) ||
      !GetDoubleArg(env, args[9], &friction)) {
    ThrowTypeError(env, "createTaperedCylinder: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateTaperedCylinder(
      static_cast<float>(half_h),
      static_cast<float>(top_r),
      static_cast<float>(bottom_r),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create tapered cylinder");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateConvexHull(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(8)

  bool is_array = false;
  if (napi_is_array(env, args[1], &is_array) != napi_ok || !is_array) {
    ThrowTypeError(env, "createConvexHull: points must be number array [x,y,z,...]");
    return nullptr;
  }

  uint32_t len = 0;
  napi_get_array_length(env, args[1], &len);
  if (len < 12 || (len % 3) != 0) {
    ThrowTypeError(env, "createConvexHull: need at least 4 points");
    return nullptr;
  }

  std::vector<Vec3> points;
  points.reserve(len / 3);
  for (uint32_t i = 0; i < len; i += 3) {
    napi_value vx, vy, vz;
    double x, y, z;
    napi_get_element(env, args[1], i, &vx);
    napi_get_element(env, args[1], i + 1, &vy);
    napi_get_element(env, args[1], i + 2, &vz);
    if (!GetDoubleArg(env, vx, &x) || !GetDoubleArg(env, vy, &y) || !GetDoubleArg(env, vz, &z)) {
      ThrowTypeError(env, "createConvexHull: points must contain numbers");
      return nullptr;
    }
    points.emplace_back(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
  }

  double px, py, pz, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[2], &px) || !GetDoubleArg(env, args[3], &py) || !GetDoubleArg(env, args[4], &pz) ||
      !GetBoolArg(env, args[5], &dynamic) || !GetDoubleArg(env, args[6], &restitution) || !GetDoubleArg(env, args[7], &friction)) {
    ThrowTypeError(env, "createConvexHull: invalid body args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateConvexHull(points, px, py, pz, dynamic, static_cast<float>(restitution), static_cast<float>(friction));
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create convex hull");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateMesh(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(7, 8)

  bool vertices_is_array = false;
  bool indices_is_array = false;
  napi_is_array(env, args[1], &vertices_is_array);
  napi_is_array(env, args[2], &indices_is_array);
  if (!vertices_is_array || !indices_is_array) {
    ThrowTypeError(env, "createMesh: vertices/indices must be arrays");
    return nullptr;
  }

  uint32_t vlen = 0;
  uint32_t ilen = 0;
  napi_get_array_length(env, args[1], &vlen);
  napi_get_array_length(env, args[2], &ilen);
  if (vlen < 9 || (vlen % 3) != 0 || ilen < 3 || (ilen % 3) != 0) {
    ThrowTypeError(env, "createMesh: invalid array lengths");
    return nullptr;
  }

  std::vector<Float3> vertices;
  vertices.reserve(vlen / 3);
  for (uint32_t i = 0; i < vlen; i += 3) {
    napi_value vx, vy, vz;
    double x, y, z;
    napi_get_element(env, args[1], i, &vx);
    napi_get_element(env, args[1], i + 1, &vy);
    napi_get_element(env, args[1], i + 2, &vz);
    if (!GetDoubleArg(env, vx, &x) || !GetDoubleArg(env, vy, &y) || !GetDoubleArg(env, vz, &z)) {
      ThrowTypeError(env, "createMesh: vertices must contain numbers");
      return nullptr;
    }
    vertices.push_back(Float3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
  }

  // Parse optional 8th argument: opts { materialIndices, materials, restitution }
  std::vector<uint32_t> mat_index_per_tri;
  PhysicsMaterialList mat_list;
  double restitution = 0.0;

  if (argc > 7) {
    napi_value opts = args[7];
    napi_valuetype opts_type = napi_undefined;
    napi_typeof(env, opts, &opts_type);
    if (opts_type == napi_object) {
      // restitution override
      napi_value rv;
      if (napi_get_named_property(env, opts, "restitution", &rv) == napi_ok) GetDoubleArg(env, rv, &restitution);

      // materialIndices — one uint per triangle
      napi_value mi_v;
      if (napi_get_named_property(env, opts, "materialIndices", &mi_v) == napi_ok) {
        bool is_typed = false;
        bool is_arr = false;
        napi_is_typedarray(env, mi_v, &is_typed);
        napi_is_array(env, mi_v, &is_arr);
        uint32_t num_tris = ilen / 3;
        if (is_typed) {
          napi_typedarray_type ta_type;
          size_t byte_offset = 0, length = 0;
          void *data = nullptr;
          napi_value buf;
          napi_get_typedarray_info(env, mi_v, &ta_type, &length, &data, &buf, &byte_offset);
          mat_index_per_tri.resize(num_tris, 0);
          for (uint32_t k = 0; k < num_tris && k < static_cast<uint32_t>(length); ++k)
            mat_index_per_tri[k] = static_cast<uint32_t *>(data)[k];
        } else if (is_arr) {
          uint32_t mi_len = 0;
          napi_get_array_length(env, mi_v, &mi_len);
          mat_index_per_tri.resize(num_tris, 0);
          for (uint32_t k = 0; k < num_tris && k < mi_len; ++k) {
            napi_value elem;
            napi_get_element(env, mi_v, k, &elem);
            uint32_t idx = 0;
            GetUInt32Arg(env, elem, &idx);
            mat_index_per_tri[k] = idx;
          }
        }
      }

      // materials — array of { friction, restitution }
      napi_value mats_v;
      if (napi_get_named_property(env, opts, "materials", &mats_v) == napi_ok) {
        bool is_arr = false;
        napi_is_array(env, mats_v, &is_arr);
        if (is_arr) {
          uint32_t mats_len = 0;
          napi_get_array_length(env, mats_v, &mats_len);
          for (uint32_t k = 0; k < mats_len; ++k) {
            napi_value mobj;
            napi_get_element(env, mats_v, k, &mobj);
            double mf = 0.5, mr = 0.0;
            napi_value fv, rv2;
            if (napi_get_named_property(env, mobj, "friction", &fv) == napi_ok) GetDoubleArg(env, fv, &mf);
            if (napi_get_named_property(env, mobj, "restitution", &rv2) == napi_ok) GetDoubleArg(env, rv2, &mr);
            mat_list.push_back(new IndexedMaterial(k, static_cast<float>(mf), static_cast<float>(mr)));
          }
        }
      }

      // Auto-create material slots if indices given but no materials array
      if (!mat_index_per_tri.empty() && mat_list.empty()) {
        uint32_t max_idx = *std::max_element(mat_index_per_tri.begin(), mat_index_per_tri.end());
        for (uint32_t k = 0; k <= max_idx; ++k)
          mat_list.push_back(new IndexedMaterial(k, 0.5f, 0.0f));
      }
    }
  }

  std::vector<IndexedTriangle> tris;
  tris.reserve(ilen / 3);
  for (uint32_t i = 0; i < ilen; i += 3) {
    napi_value va, vb, vc;
    uint32_t a, b, c;
    napi_get_element(env, args[2], i, &va);
    napi_get_element(env, args[2], i + 1, &vb);
    napi_get_element(env, args[2], i + 2, &vc);
    if (!GetUInt32Arg(env, va, &a) || !GetUInt32Arg(env, vb, &b) || !GetUInt32Arg(env, vc, &c)) {
      ThrowTypeError(env, "createMesh: indices must contain uint numbers");
      return nullptr;
    }
    uint32_t tri_idx = i / 3;
    uint32_t mat_idx = (tri_idx < mat_index_per_tri.size()) ? mat_index_per_tri[tri_idx] : 0;
    tris.push_back(IndexedTriangle(a, b, c, mat_idx));
  }

  double x, y, z, friction;
  if (!GetDoubleArg(env, args[3], &x) || !GetDoubleArg(env, args[4], &y) || !GetDoubleArg(env, args[5], &z) ||
      !GetDoubleArg(env, args[6], &friction)) {
    ThrowTypeError(env, "createMesh: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateMesh(tris.empty() ? std::vector<Float3>() : vertices, tris, mat_list, x, y, z, static_cast<float>(friction), static_cast<float>(restitution));
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create mesh");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value GetBodyPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getBodyPosition: bodyId must be uint32");
    return nullptr;
  }
  RVec3 pos;
  if (!handle->world->GetBodyPosition(id, pos)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeVec3Object(env, pos);
}

napi_value GetBodyRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getBodyRotation: bodyId must be uint32");
    return nullptr;
  }
  Quat rot;
  if (!handle->world->GetBodyRotation(id, rot)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeQuatObject(env, rot);
}

napi_value SetBodyPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t id;
  double x, y, z;
  bool activate;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z) ||
      !GetBoolArg(env, args[5], &activate)) {
    ThrowTypeError(env, "setBodyPosition: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetBodyPosition(id, x, y, z, activate), &out);
  return out;
}

napi_value SetBodyRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(7)
  uint32_t id;
  double x, y, z, w;
  bool activate;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z) ||
      !GetDoubleArg(env, args[5], &w) || !GetBoolArg(env, args[6], &activate)) {
    ThrowTypeError(env, "setBodyRotation: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetBodyRotation(id, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w), activate), &out);
  return out;
}

napi_value GetLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getLinearVelocity: bodyId must be uint32");
    return nullptr;
  }
  Vec3 v;
  if (!handle->world->GetLinearVelocity(id, v)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeVec3Object(env, RVec3(v.GetX(), v.GetY(), v.GetZ()));
}

napi_value SetLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "setLinearVelocity: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetLinearVelocity(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value GetAngularVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getAngularVelocity: bodyId must be uint32");
    return nullptr;
  }
  Vec3 v;
  if (!handle->world->GetAngularVelocity(id, v)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeVec3Object(env, RVec3(v.GetX(), v.GetY(), v.GetZ()));
}

napi_value SetAngularVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "setAngularVelocity: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetAngularVelocity(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value ApplyImpulse(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "applyImpulse: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->ApplyImpulse(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value AddForce(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "addForce: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AddForce(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value AddTorque(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "addTorque: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AddTorque(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value AddAngularImpulse(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "addAngularImpulse: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AddAngularImpulse(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value SetFrictionValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  double friction;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &friction)) {
    ThrowTypeError(env, "setFriction: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetFrictionValue(id, static_cast<float>(friction)), &out);
  return out;
}

napi_value GetFrictionValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getFriction: invalid bodyId");
    return nullptr;
  }
  float value = 0.0f;
  if (!handle->world->GetFrictionValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_double(env, value, &out);
  return out;
}

napi_value SetRestitutionValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  double restitution;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &restitution)) {
    ThrowTypeError(env, "setRestitution: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetRestitutionValue(id, static_cast<float>(restitution)), &out);
  return out;
}

napi_value GetRestitutionValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getRestitution: invalid bodyId");
    return nullptr;
  }
  float value = 0.0f;
  if (!handle->world->GetRestitutionValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_double(env, value, &out);
  return out;
}

napi_value SetGravityFactorValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  double factor;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &factor)) {
    ThrowTypeError(env, "setGravityFactor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetGravityFactorValue(id, static_cast<float>(factor)), &out);
  return out;
}

napi_value GetGravityFactorValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getGravityFactor: invalid bodyId");
    return nullptr;
  }
  float value = 0.0f;
  if (!handle->world->GetGravityFactorValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_double(env, value, &out);
  return out;
}

napi_value SetMotionTypeValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  int32_t motion_type;
  bool activate;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &motion_type) || !GetBoolArg(env, args[3], &activate)) {
    ThrowTypeError(env, "setMotionType: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetMotionTypeValue(id, motion_type, activate), &out);
  return out;
}

napi_value GetMotionTypeValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getMotionType: invalid bodyId");
    return nullptr;
  }
  int32_t value = 0;
  if (!handle->world->GetMotionTypeValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, value, &out);
  return out;
}

napi_value SetMotionQualityValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  int32_t quality;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &quality)) {
    ThrowTypeError(env, "setMotionQuality: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetMotionQualityValue(id, quality), &out);
  return out;
}

napi_value GetMotionQualityValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getMotionQuality: invalid bodyId");
    return nullptr;
  }
  int32_t value = 0;
  if (!handle->world->GetMotionQualityValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, value, &out);
  return out;
}

napi_value SetObjectLayerValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  uint32_t layer;
  if (!GetUInt32Arg(env, args[1], &id) || !GetUInt32Arg(env, args[2], &layer)) {
    ThrowTypeError(env, "setObjectLayer: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetObjectLayerValue(id, layer), &out);
  return out;
}

napi_value GetObjectLayerValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getObjectLayer: invalid bodyId");
    return nullptr;
  }
  uint32_t value = 0;
  if (!handle->world->GetObjectLayerValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, value, &out);
  return out;
}

napi_value SetDampingValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  double linear_damping;
  double angular_damping;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &linear_damping) || !GetDoubleArg(env, args[3], &angular_damping)) {
    ThrowTypeError(env, "setDamping: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetDamping(id, static_cast<float>(linear_damping), static_cast<float>(angular_damping)), &out);
  return out;
}

napi_value GetDampingValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getDamping: invalid bodyId");
    return nullptr;
  }
  float linear = 0.0f;
  float angular = 0.0f;
  if (!handle->world->GetDamping(id, linear, angular)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_object(env, &out);
  napi_value lv;
  napi_value av;
  napi_create_double(env, linear, &lv);
  napi_create_double(env, angular, &av);
  napi_set_named_property(env, out, "linear", lv);
  napi_set_named_property(env, out, "angular", av);
  return out;
}

napi_value ActivateBody(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "activateBody: bodyId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->ActivateBody(id), &out);
  return out;
}

napi_value DeactivateBody(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "deactivateBody: bodyId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->DeactivateBody(id), &out);
  return out;
}

napi_value RemoveBody(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "removeBody: bodyId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->RemoveBody(id), &out);
  return out;
}

napi_value HasBody(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "hasBody: bodyId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->HasBody(id), &out);
  return out;
}

napi_value IsBodyActive(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "isBodyActive: bodyId must be uint32");
    return nullptr;
  }
  bool active = false;
  if (!handle->world->IsBodyActive(id, active)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, active, &out);
  return out;
}

napi_value SetBodySensor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  bool is_sensor;
  if (!GetUInt32Arg(env, args[1], &id) || !GetBoolArg(env, args[2], &is_sensor)) {
    ThrowTypeError(env, "setBodySensor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetBodySensor(id, is_sensor), &out);
  return out;
}

napi_value IsBodySensor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "isBodySensor: bodyId must be uint32");
    return nullptr;
  }
  bool is_sensor = false;
  if (!handle->world->IsBodySensor(id, is_sensor)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, is_sensor, &out);
  return out;
}

napi_value GetCenterOfMassPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getCenterOfMassPosition: bodyId must be uint32");
    return nullptr;
  }
  RVec3 pos;
  if (!handle->world->GetCenterOfMassPosition(id, pos)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeVec3Object(env, pos);
}

napi_value RayCastClosest(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(8, 9)
  double ox, oy, oz, dx, dy, dz, max_dist;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) || !GetDoubleArg(env, args[4], &dx) ||
      !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) || !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0) {
    ThrowTypeError(env, "rayCastClosest: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 8) ParseQueryFilters(env, args[8], filters);

  uint32_t body = 0;
  double fraction = 0;
  Vec3 normal;
  uint32_t mat_idx = 0;
  if (!handle->world->RayCastClosest(RVec3(ox, oy, oz), Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)), max_dist, filters, body, fraction, normal, mat_idx)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }

  napi_value out;
  napi_create_object(env, &out);

  napi_value body_v, fraction_v, normal_v, mat_v;
  napi_create_uint32(env, body, &body_v);
  napi_create_double(env, fraction, &fraction_v);
  normal_v = MakeVec3Object(env, RVec3(normal.GetX(), normal.GetY(), normal.GetZ()));
  napi_create_uint32(env, mat_idx, &mat_v);

  napi_set_named_property(env, out, "bodyId", body_v);
  napi_set_named_property(env, out, "fraction", fraction_v);
  napi_set_named_property(env, out, "normal", normal_v);
  napi_set_named_property(env, out, "materialIndex", mat_v);
  return out;
}

napi_value RayCastAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(8, 9)
  double ox, oy, oz, dx, dy, dz, max_dist;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) || !GetDoubleArg(env, args[4], &dx) ||
      !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) || !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0) {
    ThrowTypeError(env, "rayCastAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 8) ParseQueryFilters(env, args[8], filters);

  std::vector<PhysicsWorld::RayHitInfo> hits;
  if (!handle->world->RayCastAll(RVec3(ox, oy, oz), Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)), max_dist, filters, hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, fraction_v, mat_v;
    napi_value normal_v = MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ()));
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "normal", normal_v);
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value CollideSphereAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(6, 7)
  double cx, cy, cz, radius, max_sep;
  if (!GetDoubleArg(env, args[1], &cx) || !GetDoubleArg(env, args[2], &cy) || !GetDoubleArg(env, args[3], &cz) ||
      !GetDoubleArg(env, args[4], &radius) || radius <= 0.0 || !GetDoubleArg(env, args[5], &max_sep) || max_sep < 0.0) {
    ThrowTypeError(env, "collideSphereAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 6) ParseQueryFilters(env, args[6], filters);

  std::vector<PhysicsWorld::ShapeQueryHit> hits;
  if (!handle->world->CollideSphereAll(RVec3(cx, cy, cz), static_cast<float>(radius), static_cast<float>(max_sep), filters, hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, penetration_v, fraction_v, mat_v;
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].penetration_depth, &penetration_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "penetrationDepth", penetration_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "point", MakeVec3Object(env, RVec3(hits[i].point.GetX(), hits[i].point.GetY(), hits[i].point.GetZ())));
    napi_set_named_property(env, item, "normal", MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ())));
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value CastSphereAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(9, 10)
  double ox, oy, oz, dx, dy, dz, max_dist, radius;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) || !GetDoubleArg(env, args[4], &dx) ||
      !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) || !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0.0 ||
      !GetDoubleArg(env, args[8], &radius) || radius <= 0.0) {
    ThrowTypeError(env, "castSphereAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 9) ParseQueryFilters(env, args[9], filters);

  std::vector<PhysicsWorld::ShapeQueryHit> hits;
  if (!handle->world->CastSphereAll(
          RVec3(ox, oy, oz),
          Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)),
          static_cast<float>(max_dist),
          static_cast<float>(radius),
          filters,
          hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, penetration_v, fraction_v, mat_v;
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].penetration_depth, &penetration_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "penetrationDepth", penetration_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "point", MakeVec3Object(env, RVec3(hits[i].point.GetX(), hits[i].point.GetY(), hits[i].point.GetZ())));
    napi_set_named_property(env, item, "normal", MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ())));
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value CastBoxAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(11, 12)
  double ox, oy, oz, dx, dy, dz, max_dist, hx, hy, hz;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) ||
      !GetDoubleArg(env, args[4], &dx) || !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) ||
      !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0.0 ||
      !GetDoubleArg(env, args[8], &hx) || hx <= 0.0 ||
      !GetDoubleArg(env, args[9], &hy) || hy <= 0.0 ||
      !GetDoubleArg(env, args[10], &hz) || hz <= 0.0) {
    ThrowTypeError(env, "castBoxAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 11) ParseQueryFilters(env, args[11], filters);

  std::vector<PhysicsWorld::ShapeQueryHit> hits;
  if (!handle->world->CastBoxAll(
          RVec3(ox, oy, oz),
          Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)),
          static_cast<float>(max_dist),
          static_cast<float>(hx), static_cast<float>(hy), static_cast<float>(hz),
          filters, hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, penetration_v, fraction_v, mat_v;
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].penetration_depth, &penetration_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "penetrationDepth", penetration_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "point", MakeVec3Object(env, RVec3(hits[i].point.GetX(), hits[i].point.GetY(), hits[i].point.GetZ())));
    napi_set_named_property(env, item, "normal", MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ())));
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value CastCapsuleAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(10, 11)
  double ox, oy, oz, dx, dy, dz, max_dist, half_height, radius;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) ||
      !GetDoubleArg(env, args[4], &dx) || !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) ||
      !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0.0 ||
      !GetDoubleArg(env, args[8], &half_height) || half_height <= 0.0 ||
      !GetDoubleArg(env, args[9], &radius) || radius <= 0.0) {
    ThrowTypeError(env, "castCapsuleAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 10) ParseQueryFilters(env, args[10], filters);

  std::vector<PhysicsWorld::ShapeQueryHit> hits;
  if (!handle->world->CastCapsuleAll(
          RVec3(ox, oy, oz),
          Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)),
          static_cast<float>(max_dist),
          static_cast<float>(half_height),
          static_cast<float>(radius),
          filters, hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, penetration_v, fraction_v, mat_v;
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].penetration_depth, &penetration_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "penetrationDepth", penetration_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "point", MakeVec3Object(env, RVec3(hits[i].point.GetX(), hits[i].point.GetY(), hits[i].point.GetZ())));
    napi_set_named_property(env, item, "normal", MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ())));
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value QueryAABB(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(7, 8)
  double min_x, min_y, min_z, max_x, max_y, max_z;
  if (!GetDoubleArg(env, args[1], &min_x) || !GetDoubleArg(env, args[2], &min_y) || !GetDoubleArg(env, args[3], &min_z) ||
      !GetDoubleArg(env, args[4], &max_x) || !GetDoubleArg(env, args[5], &max_y) || !GetDoubleArg(env, args[6], &max_z) ||
      min_x > max_x || min_y > max_y || min_z > max_z) {
    ThrowTypeError(env, "queryAABB: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 7) ParseQueryFilters(env, args[7], filters);

  std::vector<uint32_t> bodies;
  if (!handle->world->QueryAABB(RVec3(min_x, min_y, min_z), RVec3(max_x, max_y, max_z), filters, bodies)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, bodies.size(), &arr);
  for (size_t i = 0; i < bodies.size(); ++i) {
    napi_value id;
    napi_create_uint32(env, bodies[i], &id);
    napi_set_element(env, arr, static_cast<uint32_t>(i), id);
  }
  return arr;
}

napi_value AreBodiesInContact(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t a, b;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b)) {
    ThrowTypeError(env, "areBodiesInContact: body ids must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AreBodiesInContact(a, b), &out);
  return out;
}

napi_value CreateFixedConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t a, b;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b)) {
    ThrowTypeError(env, "createFixedConstraint: invalid body ids");
    return nullptr;
  }
  uint32_t id = handle->world->CreateFixedConstraint(a, b);
  if (id == 0) {
    ThrowError(env, "Failed to create fixed constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateDistanceConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(11)
  uint32_t a, b;
  double ax, ay, az, bx, by, bz, min_d, max_d;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &ax) || !GetDoubleArg(env, args[4], &ay) ||
      !GetDoubleArg(env, args[5], &az) || !GetDoubleArg(env, args[6], &bx) || !GetDoubleArg(env, args[7], &by) || !GetDoubleArg(env, args[8], &bz) ||
      !GetDoubleArg(env, args[9], &min_d) || !GetDoubleArg(env, args[10], &max_d) || min_d > max_d) {
    ThrowTypeError(env, "createDistanceConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateDistanceConstraint(a, b, RVec3(ax, ay, az), RVec3(bx, by, bz), static_cast<float>(min_d), static_cast<float>(max_d));
  if (id == 0) {
    ThrowError(env, "Failed to create distance constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateHingeConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(12)
  uint32_t a, b;
  double px, py, pz, ax, ay, az, nx, ny, nz;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &ax) || !GetDoubleArg(env, args[7], &ay) || !GetDoubleArg(env, args[8], &az) ||
      !GetDoubleArg(env, args[9], &nx) || !GetDoubleArg(env, args[10], &ny) || !GetDoubleArg(env, args[11], &nz)) {
    ThrowTypeError(env, "createHingeConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateHingeConstraint(
      a,
      b,
      RVec3(px, py, pz),
      Vec3(static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az)),
      Vec3(static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)));
  if (id == 0) {
    ThrowError(env, "Failed to create hinge constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateSliderConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(14)
  uint32_t a, b;
  double px, py, pz, ax, ay, az, nx, ny, nz, min_l, max_l;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &ax) || !GetDoubleArg(env, args[7], &ay) || !GetDoubleArg(env, args[8], &az) ||
      !GetDoubleArg(env, args[9], &nx) || !GetDoubleArg(env, args[10], &ny) || !GetDoubleArg(env, args[11], &nz) || !GetDoubleArg(env, args[12], &min_l) ||
      !GetDoubleArg(env, args[13], &max_l) || min_l > max_l) {
    ThrowTypeError(env, "createSliderConstraint: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateSliderConstraint(
      a,
      b,
      RVec3(px, py, pz),
      Vec3(static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az)),
      Vec3(static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)),
      static_cast<float>(min_l),
      static_cast<float>(max_l));
  if (id == 0) {
    ThrowError(env, "Failed to create slider constraint");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreatePointConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t a, b;
  double px, py, pz;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz)) {
    ThrowTypeError(env, "createPointConstraint: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreatePointConstraint(a, b, RVec3(px, py, pz));
  if (id == 0) {
    ThrowError(env, "Failed to create point constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateConeConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  uint32_t a, b;
  double px, py, pz, ax, ay, az, half_angle;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &ax) || !GetDoubleArg(env, args[7], &ay) || !GetDoubleArg(env, args[8], &az) ||
      !GetDoubleArg(env, args[9], &half_angle)) {
    ThrowTypeError(env, "createConeConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateConeConstraint(
      a, b, RVec3(px, py, pz), Vec3(static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az)), static_cast<float>(half_angle));
  if (id == 0) {
    ThrowError(env, "Failed to create cone constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateSwingTwistConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(16)
  uint32_t a, b;
  double px, py, pz, tx, ty, tz, plx, ply, plz, normal_half, plane_half, twist_min, twist_max;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &tx) || !GetDoubleArg(env, args[7], &ty) || !GetDoubleArg(env, args[8], &tz) ||
      !GetDoubleArg(env, args[9], &plx) || !GetDoubleArg(env, args[10], &ply) || !GetDoubleArg(env, args[11], &plz) ||
      !GetDoubleArg(env, args[12], &normal_half) || !GetDoubleArg(env, args[13], &plane_half) || !GetDoubleArg(env, args[14], &twist_min) ||
      !GetDoubleArg(env, args[15], &twist_max)) {
    ThrowTypeError(env, "createSwingTwistConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateSwingTwistConstraint(
      a,
      b,
      RVec3(px, py, pz),
      Vec3(static_cast<float>(tx), static_cast<float>(ty), static_cast<float>(tz)),
      Vec3(static_cast<float>(plx), static_cast<float>(ply), static_cast<float>(plz)),
      static_cast<float>(normal_half),
      static_cast<float>(plane_half),
      static_cast<float>(twist_min),
      static_cast<float>(twist_max));
  if (id == 0) {
    ThrowError(env, "Failed to create swing twist constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateSixDOFConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(12)
  uint32_t a, b;
  double px, py, pz, axx, axy, axz, ayx, ayy, ayz;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &axx) || !GetDoubleArg(env, args[7], &axy) || !GetDoubleArg(env, args[8], &axz) ||
      !GetDoubleArg(env, args[9], &ayx) || !GetDoubleArg(env, args[10], &ayy) || !GetDoubleArg(env, args[11], &ayz)) {
    ThrowTypeError(env, "createSixDOFConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateSixDOFConstraint(
      a,
      b,
      RVec3(px, py, pz),
      Vec3(static_cast<float>(axx), static_cast<float>(axy), static_cast<float>(axz)),
      Vec3(static_cast<float>(ayx), static_cast<float>(ayy), static_cast<float>(ayz)));
  if (id == 0) {
    ThrowError(env, "Failed to create six dof constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value RemoveConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "removeConstraint: id must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->RemoveConstraintById(id), &out);
  return out;
}

napi_value SetHingeLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  double min_angle;
  double max_angle;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &min_angle) || !GetDoubleArg(env, args[3], &max_angle)) {
    ThrowTypeError(env, "setHingeLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetHingeLimits(id, static_cast<float>(min_angle), static_cast<float>(max_angle)), &out);
  return out;
}

napi_value SetSliderLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  double min_limit;
  double max_limit;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &min_limit) || !GetDoubleArg(env, args[3], &max_limit)) {
    ThrowTypeError(env, "setSliderLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetSliderLimits(id, static_cast<float>(min_limit), static_cast<float>(max_limit)), &out);
  return out;
}

napi_value SetHingeMotor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t id;
  int32_t state;
  double target_velocity;
  double target_angle;
  double max_torque;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &state) || !GetDoubleArg(env, args[3], &target_velocity) ||
      !GetDoubleArg(env, args[4], &target_angle) || !GetDoubleArg(env, args[5], &max_torque)) {
    ThrowTypeError(env, "setHingeMotor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetHingeMotor(id, state, static_cast<float>(target_velocity), static_cast<float>(target_angle), static_cast<float>(max_torque)),
      &out);
  return out;
}

napi_value SetSliderMotor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t id;
  int32_t state;
  double target_velocity;
  double target_position;
  double max_force;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &state) || !GetDoubleArg(env, args[3], &target_velocity) ||
      !GetDoubleArg(env, args[4], &target_position) || !GetDoubleArg(env, args[5], &max_force)) {
    ThrowTypeError(env, "setSliderMotor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSliderMotor(id, state, static_cast<float>(target_velocity), static_cast<float>(target_position), static_cast<float>(max_force)),
      &out);
  return out;
}

napi_value SetConeHalfAngle(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  double half_angle;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &half_angle)) {
    ThrowTypeError(env, "setConeHalfAngle: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetConeHalfAngle(id, static_cast<float>(half_angle)), &out);
  return out;
}

napi_value SetSwingTwistLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t id;
  double normal_half;
  double plane_half;
  double twist_min;
  double twist_max;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &normal_half) || !GetDoubleArg(env, args[3], &plane_half) ||
      !GetDoubleArg(env, args[4], &twist_min) || !GetDoubleArg(env, args[5], &twist_max)) {
    ThrowTypeError(env, "setSwingTwistLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSwingTwistLimits(
          id, static_cast<float>(normal_half), static_cast<float>(plane_half), static_cast<float>(twist_min), static_cast<float>(twist_max)),
      &out);
  return out;
}

napi_value SetSwingTwistMotor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(12)
  uint32_t id;
  int32_t swing_state;
  int32_t twist_state;
  double vx, vy, vz, qx, qy, qz, qw, max_torque;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &swing_state) || !GetInt32Arg(env, args[3], &twist_state) ||
      !GetDoubleArg(env, args[4], &vx) || !GetDoubleArg(env, args[5], &vy) || !GetDoubleArg(env, args[6], &vz) || !GetDoubleArg(env, args[7], &qx) ||
      !GetDoubleArg(env, args[8], &qy) || !GetDoubleArg(env, args[9], &qz) || !GetDoubleArg(env, args[10], &qw) || !GetDoubleArg(env, args[11], &max_torque)) {
    ThrowTypeError(env, "setSwingTwistMotor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSwingTwistMotor(
          id,
          swing_state,
          twist_state,
          Vec3(static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)),
          Quat(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz), static_cast<float>(qw)),
          static_cast<float>(max_torque)),
      &out);
  return out;
}

napi_value SetSixDOFLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(14)
  uint32_t id;
  double tminx, tminy, tminz, tmaxx, tmaxy, tmaxz, rminx, rminy, rminz, rmaxx, rmaxy, rmaxz;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &tminx) || !GetDoubleArg(env, args[3], &tminy) || !GetDoubleArg(env, args[4], &tminz) ||
      !GetDoubleArg(env, args[5], &tmaxx) || !GetDoubleArg(env, args[6], &tmaxy) || !GetDoubleArg(env, args[7], &tmaxz) || !GetDoubleArg(env, args[8], &rminx) ||
      !GetDoubleArg(env, args[9], &rminy) || !GetDoubleArg(env, args[10], &rminz) || !GetDoubleArg(env, args[11], &rmaxx) || !GetDoubleArg(env, args[12], &rmaxy) ||
      !GetDoubleArg(env, args[13], &rmaxz)) {
    ThrowTypeError(env, "setSixDOFLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSixDOFLimits(
          id,
          Vec3(static_cast<float>(tminx), static_cast<float>(tminy), static_cast<float>(tminz)),
          Vec3(static_cast<float>(tmaxx), static_cast<float>(tmaxy), static_cast<float>(tmaxz)),
          Vec3(static_cast<float>(rminx), static_cast<float>(rminy), static_cast<float>(rminz)),
          Vec3(static_cast<float>(rmaxx), static_cast<float>(rmaxy), static_cast<float>(rmaxz))),
      &out);
  return out;
}

napi_value SetSixDOFMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  int32_t axis;
  int32_t state;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &axis) || !GetInt32Arg(env, args[3], &state)) {
    ThrowTypeError(env, "setSixDOFMotorState: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetSixDOFMotorState(id, axis, state), &out);
  return out;
}

napi_value SetSixDOFTargetVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(8)
  uint32_t id;
  double lx, ly, lz, ax, ay, az;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &lx) || !GetDoubleArg(env, args[3], &ly) || !GetDoubleArg(env, args[4], &lz) ||
      !GetDoubleArg(env, args[5], &ax) || !GetDoubleArg(env, args[6], &ay) || !GetDoubleArg(env, args[7], &az)) {
    ThrowTypeError(env, "setSixDOFTargetVelocity: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSixDOFTargetVelocity(
          id, Vec3(static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz)), Vec3(static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az))),
      &out);
  return out;
}

napi_value SetSixDOFTargetPose(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(9)
  uint32_t id;
  double px, py, pz, qx, qy, qz, qw;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &px) || !GetDoubleArg(env, args[3], &py) || !GetDoubleArg(env, args[4], &pz) ||
      !GetDoubleArg(env, args[5], &qx) || !GetDoubleArg(env, args[6], &qy) || !GetDoubleArg(env, args[7], &qz) || !GetDoubleArg(env, args[8], &qw)) {
    ThrowTypeError(env, "setSixDOFTargetPose: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSixDOFTargetPose(
          id,
          Vec3(static_cast<float>(px), static_cast<float>(py), static_cast<float>(pz)),
          Quat(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz), static_cast<float>(qw))),
      &out);
  return out;
}

// ─── Motor spring setters ──────────────────────────────────────────────────
napi_value SetHingeMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "setHingeMotorSpring: need (id, opts)"); return nullptr; }
  MotorSpringParams p; ParseMotorSpringParams(env, args[2], p);
  napi_value out; napi_get_boolean(env, handle->world->SetHingeMotorSpring(id, p), &out); return out;
}

napi_value SetSliderMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "setSliderMotorSpring: need (id, opts)"); return nullptr; }
  MotorSpringParams p; ParseMotorSpringParams(env, args[2], p);
  napi_value out; napi_get_boolean(env, handle->world->SetSliderMotorSpring(id, p), &out); return out;
}

napi_value SetSwingMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "setSwingMotorSpring: need (id, opts)"); return nullptr; }
  MotorSpringParams p; ParseMotorSpringParams(env, args[2], p);
  napi_value out; napi_get_boolean(env, handle->world->SetSwingMotorSpring(id, p), &out); return out;
}

napi_value SetTwistMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "setTwistMotorSpring: need (id, opts)"); return nullptr; }
  MotorSpringParams p; ParseMotorSpringParams(env, args[2], p);
  napi_value out; napi_get_boolean(env, handle->world->SetTwistMotorSpring(id, p), &out); return out;
}

napi_value SetSixDOFMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id; int32_t axis;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &axis)) {
    ThrowTypeError(env, "setSixDOFMotorSpring: need (id, axis, opts)"); return nullptr;
  }
  MotorSpringParams p; ParseMotorSpringParams(env, args[3], p);
  napi_value out; napi_get_boolean(env, handle->world->SetSixDOFMotorSpring(id, axis, p), &out); return out;
}

// ─── Motor spring getters ──────────────────────────────────────────────────
napi_value GetHingeMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getHingeMotorSpring: need (id)"); return nullptr; }
  MotorSettings ms; if (!handle->world->GetHingeMotorSpring(id, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetSliderMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSliderMotorSpring: need (id)"); return nullptr; }
  MotorSettings ms; if (!handle->world->GetSliderMotorSpring(id, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetSwingMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSwingMotorSpring: need (id)"); return nullptr; }
  MotorSettings ms; if (!handle->world->GetSwingMotorSpring(id, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetTwistMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getTwistMotorSpring: need (id)"); return nullptr; }
  MotorSettings ms; if (!handle->world->GetTwistMotorSpring(id, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetSixDOFMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id; int32_t axis;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &axis)) {
    ThrowTypeError(env, "getSixDOFMotorSpring: need (id, axis)"); return nullptr;
  }
  MotorSettings ms; if (!handle->world->GetSixDOFMotorSpring(id, axis, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetHingeAngle(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getHingeAngle: invalid args");
    return nullptr;
  }
  float angle = 0.0f;
  if (!handle->world->GetHingeAngle(id, angle)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(angle), &out);
  return out;
}

napi_value GetHingeMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getHingeMotorState: invalid args");
    return nullptr;
  }
  int32_t state = 0;
  if (!handle->world->GetHingeMotorState(id, state)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_int32(env, state, &out);
  return out;
}

napi_value GetSliderPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSliderPosition: invalid args");
    return nullptr;
  }
  float position = 0.0f;
  if (!handle->world->GetSliderPosition(id, position)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(position), &out);
  return out;
}

napi_value GetSliderMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSliderMotorState: invalid args");
    return nullptr;
  }
  int32_t state = 0;
  if (!handle->world->GetSliderMotorState(id, state)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_int32(env, state, &out);
  return out;
}

napi_value GetSixDOFRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSixDOFRotation: invalid args");
    return nullptr;
  }
  Quat rotation;
  if (!handle->world->GetSixDOFRotation(id, rotation)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  return MakeQuatObject(env, rotation);
}

napi_value GetSixDOFLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSixDOFLimits: invalid args");
    return nullptr;
  }
  Vec3 tmin, tmax, rmin, rmax;
  if (!handle->world->GetSixDOFLimits(id, tmin, tmax, rmin, rmax)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_object(env, &out);
  napi_set_named_property(env, out, "translationMin", MakeVec3Object(env, RVec3(tmin)));
  napi_set_named_property(env, out, "translationMax", MakeVec3Object(env, RVec3(tmax)));
  napi_set_named_property(env, out, "rotationMin", MakeVec3Object(env, RVec3(rmin)));
  napi_set_named_property(env, out, "rotationMax", MakeVec3Object(env, RVec3(rmax)));
  return out;
}

napi_value GetSixDOFMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  int32_t axis;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &axis)) {
    ThrowTypeError(env, "getSixDOFMotorState: invalid args");
    return nullptr;
  }
  int32_t state = 0;
  if (!handle->world->GetSixDOFMotorState(id, axis, state)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_int32(env, state, &out);
  return out;
}

napi_value GetSwingTwistRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSwingTwistRotation: invalid args");
    return nullptr;
  }
  Quat rotation;
  if (!handle->world->GetSwingTwistRotation(id, rotation)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  return MakeQuatObject(env, rotation);
}

napi_value GetSwingTwistMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSwingTwistMotorState: invalid args");
    return nullptr;
  }
  int32_t swing_state = 0;
  int32_t twist_state = 0;
  if (!handle->world->GetSwingTwistMotorState(id, swing_state, twist_state)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_object(env, &out);
  napi_value swing, twist;
  napi_create_int32(env, swing_state, &swing);
  napi_create_int32(env, twist_state, &twist);
  napi_set_named_property(env, out, "swingState", swing);
  napi_set_named_property(env, out, "twistState", twist);
  return out;
}

napi_value GetHingeLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getHingeLambdas: invalid args"); return nullptr; }
  Vec3 pos; float rx, ry, rlim, motor;
  if (!handle->world->GetHingeLambdas(id, pos, rx, ry, rlim, motor)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  napi_set_named_property(env, out, "rotation", MakeVec2Object(env, rx, ry));
  SetF64Prop(env, out, "rotationLimits", rlim);
  SetF64Prop(env, out, "motor", motor);
  return out;
}

napi_value GetSliderLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSliderLambdas: invalid args"); return nullptr; }
  float px, py, plim, motor; Vec3 rot;
  if (!handle->world->GetSliderLambdas(id, px, py, plim, rot, motor)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec2Object(env, px, py));
  SetF64Prop(env, out, "positionLimits", plim);
  napi_set_named_property(env, out, "rotation", MakeVec3Object(env, rot));
  SetF64Prop(env, out, "motor", motor);
  return out;
}

napi_value GetSwingTwistLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSwingTwistLambdas: invalid args"); return nullptr; }
  Vec3 pos, motor; float twist, swy, swz;
  if (!handle->world->GetSwingTwistLambdas(id, pos, twist, swy, swz, motor)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  SetF64Prop(env, out, "twist", twist);
  SetF64Prop(env, out, "swingY", swy);
  SetF64Prop(env, out, "swingZ", swz);
  napi_set_named_property(env, out, "motor", MakeVec3Object(env, motor));
  return out;
}

napi_value GetSixDOFLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSixDOFLambdas: invalid args"); return nullptr; }
  Vec3 pos, rot, mtrans, mrot;
  if (!handle->world->GetSixDOFLambdas(id, pos, rot, mtrans, mrot)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  napi_set_named_property(env, out, "rotation", MakeVec3Object(env, rot));
  napi_set_named_property(env, out, "motorTranslation", MakeVec3Object(env, mtrans));
  napi_set_named_property(env, out, "motorRotation", MakeVec3Object(env, mrot));
  return out;
}

napi_value GetConeLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getConeLambdas: invalid args"); return nullptr; }
  Vec3 pos; float rot;
  if (!handle->world->GetConeLambdas(id, pos, rot)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  SetF64Prop(env, out, "rotation", rot);
  return out;
}

napi_value GetPointLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getPointLambdas: invalid args"); return nullptr; }
  Vec3 pos;
  if (!handle->world->GetPointLambdas(id, pos)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  return out;
}

napi_value GetFixedLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getFixedLambdas: invalid args"); return nullptr; }
  Vec3 pos, rot;
  if (!handle->world->GetFixedLambdas(id, pos, rot)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  napi_set_named_property(env, out, "rotation", MakeVec3Object(env, rot));
  return out;
}

napi_value GetDistanceLambda(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getDistanceLambda: invalid args"); return nullptr; }
  float out_val;
  if (!handle->world->GetDistanceLambda(id, out_val)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_double(env, static_cast<double>(out_val), &out);
  return out;
}

napi_value GetPulleyLambda(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getPulleyLambda: invalid args"); return nullptr; }
  float out_val;
  if (!handle->world->GetPulleyLambda(id, out_val)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_double(env, static_cast<double>(out_val), &out);
  return out;
}

napi_value GetGearLambda(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getGearLambda: invalid args"); return nullptr; }
  float out_val;
  if (!handle->world->GetGearLambda(id, out_val)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_double(env, static_cast<double>(out_val), &out);
  return out;
}

napi_value GetPathLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getPathLambdas: invalid args"); return nullptr; }
  float px, py, plim, motor, rhx, rhy; Vec3 rot;
  if (!handle->world->GetPathLambdas(id, px, py, plim, motor, rhx, rhy, rot)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec2Object(env, px, py));
  SetF64Prop(env, out, "positionLimits", plim);
  SetF64Prop(env, out, "motor", motor);
  napi_set_named_property(env, out, "rotationHinge", MakeVec2Object(env, rhx, rhy));
  napi_set_named_property(env, out, "rotation", MakeVec3Object(env, rot));
  return out;
}

napi_value GetHingeLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getHingeLimits: invalid args");
    return nullptr;
  }
  float vmin = 0.0f, vmax = 0.0f;
  if (!handle->world->GetHingeLimits(id, vmin, vmax)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, nmin, nmax;
  napi_create_object(env, &out);
  napi_create_double(env, static_cast<double>(vmin), &nmin);
  napi_create_double(env, static_cast<double>(vmax), &nmax);
  napi_set_named_property(env, out, "min", nmin);
  napi_set_named_property(env, out, "max", nmax);
  return out;
}

napi_value GetSliderLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSliderLimits: invalid args");
    return nullptr;
  }
  float vmin = 0.0f, vmax = 0.0f;
  if (!handle->world->GetSliderLimits(id, vmin, vmax)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, nmin, nmax;
  napi_create_object(env, &out);
  napi_create_double(env, static_cast<double>(vmin), &nmin);
  napi_create_double(env, static_cast<double>(vmax), &nmax);
  napi_set_named_property(env, out, "min", nmin);
  napi_set_named_property(env, out, "max", nmax);
  return out;
}

napi_value GetSwingTwistLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSwingTwistLimits: invalid args");
    return nullptr;
  }
  float n_half = 0.0f, p_half = 0.0f, t_min = 0.0f, t_max = 0.0f;
  if (!handle->world->GetSwingTwistLimits(id, n_half, p_half, t_min, t_max)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, vnh, vph, vtmin, vtmax;
  napi_create_object(env, &out);
  napi_create_double(env, static_cast<double>(n_half), &vnh);
  napi_create_double(env, static_cast<double>(p_half), &vph);
  napi_create_double(env, static_cast<double>(t_min), &vtmin);
  napi_create_double(env, static_cast<double>(t_max), &vtmax);
  napi_set_named_property(env, out, "normalHalfCone", vnh);
  napi_set_named_property(env, out, "planeHalfCone", vph);
  napi_set_named_property(env, out, "twistMin", vtmin);
  napi_set_named_property(env, out, "twistMax", vtmax);
  return out;
}

napi_value CreateGearConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(10, 12)
  uint32_t a, b;
  double h1x, h1y, h1z, h2x, h2y, h2z, ratio;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) ||
      napi_get_value_double(env, args[3], &h1x) != napi_ok ||
      napi_get_value_double(env, args[4], &h1y) != napi_ok ||
      napi_get_value_double(env, args[5], &h1z) != napi_ok ||
      napi_get_value_double(env, args[6], &h2x) != napi_ok ||
      napi_get_value_double(env, args[7], &h2y) != napi_ok ||
      napi_get_value_double(env, args[8], &h2z) != napi_ok ||
      napi_get_value_double(env, args[9], &ratio) != napi_ok) {
    ThrowTypeError(env, "createGearConstraint: invalid args");
    return nullptr;
  }
  uint32_t c1 = 0, c2 = 0;
  if (argc > 10) GetUInt32Arg(env, args[10], &c1);
  if (argc > 11) GetUInt32Arg(env, args[11], &c2);
  const uint32_t id = handle->world->CreateGearConstraint(
    a, b,
    Vec3(static_cast<float>(h1x), static_cast<float>(h1y), static_cast<float>(h1z)),
    Vec3(static_cast<float>(h2x), static_cast<float>(h2y), static_cast<float>(h2z)),
    static_cast<float>(ratio), c1, c2);
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreatePulleyConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(16, 18)
  uint32_t a, b;
  double bp1x, bp1y, bp1z, fp1x, fp1y, fp1z, bp2x, bp2y, bp2z, fp2x, fp2y, fp2z, ratio;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) ||
      napi_get_value_double(env, args[3], &bp1x) != napi_ok ||
      napi_get_value_double(env, args[4], &bp1y) != napi_ok ||
      napi_get_value_double(env, args[5], &bp1z) != napi_ok ||
      napi_get_value_double(env, args[6], &fp1x) != napi_ok ||
      napi_get_value_double(env, args[7], &fp1y) != napi_ok ||
      napi_get_value_double(env, args[8], &fp1z) != napi_ok ||
      napi_get_value_double(env, args[9], &bp2x) != napi_ok ||
      napi_get_value_double(env, args[10], &bp2y) != napi_ok ||
      napi_get_value_double(env, args[11], &bp2z) != napi_ok ||
      napi_get_value_double(env, args[12], &fp2x) != napi_ok ||
      napi_get_value_double(env, args[13], &fp2y) != napi_ok ||
      napi_get_value_double(env, args[14], &fp2z) != napi_ok ||
      napi_get_value_double(env, args[15], &ratio) != napi_ok) {
    ThrowTypeError(env, "createPulleyConstraint: invalid args");
    return nullptr;
  }
  double min_len = 0.0, max_len = -1.0;
  if (argc > 16) napi_get_value_double(env, args[16], &min_len);
  if (argc > 17) napi_get_value_double(env, args[17], &max_len);
  const uint32_t id = handle->world->CreatePulleyConstraint(
    a, b,
    RVec3(static_cast<float>(bp1x), static_cast<float>(bp1y), static_cast<float>(bp1z)),
    RVec3(static_cast<float>(fp1x), static_cast<float>(fp1y), static_cast<float>(fp1z)),
    RVec3(static_cast<float>(bp2x), static_cast<float>(bp2y), static_cast<float>(bp2z)),
    RVec3(static_cast<float>(fp2x), static_cast<float>(fp2y), static_cast<float>(fp2z)),
    static_cast<float>(ratio), static_cast<float>(min_len), static_cast<float>(max_len));
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value GetPulleyLength(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPulleyLength: invalid args");
    return nullptr;
  }
  float length = 0.0f;
  if (!handle->world->GetPulleyLength(id, length)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(length), &out);
  return out;
}

napi_value SetPulleyLength(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  double min_len, max_len;
  if (!GetUInt32Arg(env, args[1], &id) ||
      napi_get_value_double(env, args[2], &min_len) != napi_ok ||
      napi_get_value_double(env, args[3], &max_len) != napi_ok) {
    ThrowTypeError(env, "setPulleyLength: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetPulleyLengthLimits(id, static_cast<float>(min_len), static_cast<float>(max_len)), &out);
  return out;
}

napi_value GetPulleyLengthLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPulleyLengthLimits: invalid args");
    return nullptr;
  }
  float vmin = 0.0f, vmax = 0.0f;
  if (!handle->world->GetPulleyLengthLimits(id, vmin, vmax)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, nmin, nmax;
  napi_create_object(env, &out);
  napi_create_double(env, static_cast<double>(vmin), &nmin);
  napi_create_double(env, static_cast<double>(vmax), &nmax);
  napi_set_named_property(env, out, "min", nmin);
  napi_set_named_property(env, out, "max", nmax);
  return out;
}

napi_value CreatePathConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(15)
  uint32_t a, b;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b)) {
    ThrowTypeError(env, "createPathConstraint: invalid args");
    return nullptr;
  }
  bool is_arr = false;
  napi_is_array(env, args[3], &is_arr);
  if (!is_arr) {
    ThrowTypeError(env, "createPathConstraint: points must be an array");
    return nullptr;
  }
  uint32_t pts_len = 0;
  napi_get_array_length(env, args[3], &pts_len);
  if (pts_len % 9 != 0) {
    ThrowTypeError(env, "createPathConstraint: points length must be multiple of 9");
    return nullptr;
  }
  std::vector<float> flat_points(pts_len);
  for (uint32_t i = 0; i < pts_len; i++) {
    napi_value elem;
    napi_get_element(env, args[3], i, &elem);
    double val = 0.0;
    napi_get_value_double(env, elem, &val);
    flat_points[i] = static_cast<float>(val);
  }
  bool closed = false;
  napi_get_value_bool(env, args[4], &closed);
  double ppx, ppy, ppz, prx, pry, prz, prw, pf, mf;
  int32_t rot_type;
  if (napi_get_value_double(env, args[5], &ppx) != napi_ok ||
      napi_get_value_double(env, args[6], &ppy) != napi_ok ||
      napi_get_value_double(env, args[7], &ppz) != napi_ok ||
      napi_get_value_double(env, args[8], &prx) != napi_ok ||
      napi_get_value_double(env, args[9], &pry) != napi_ok ||
      napi_get_value_double(env, args[10], &prz) != napi_ok ||
      napi_get_value_double(env, args[11], &prw) != napi_ok ||
      napi_get_value_double(env, args[12], &pf) != napi_ok ||
      napi_get_value_double(env, args[13], &mf) != napi_ok ||
      !GetInt32Arg(env, args[14], &rot_type)) {
    ThrowTypeError(env, "createPathConstraint: invalid numeric args");
    return nullptr;
  }
  const uint32_t id = handle->world->CreatePathConstraint(
    a, b, flat_points, closed,
    Vec3(static_cast<float>(ppx), static_cast<float>(ppy), static_cast<float>(ppz)),
    Quat(static_cast<float>(prx), static_cast<float>(pry), static_cast<float>(prz), static_cast<float>(prw)),
    static_cast<float>(pf), static_cast<float>(mf), static_cast<int>(rot_type));
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value GetPathFraction(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPathFraction: invalid args");
    return nullptr;
  }
  float frac = 0.0f;
  if (!handle->world->GetPathFraction(id, frac)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(frac), &out);
  return out;
}

napi_value GetPathMaxFraction(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPathMaxFraction: invalid args");
    return nullptr;
  }
  float max_frac = 0.0f;
  if (!handle->world->GetPathMaxFraction(id, max_frac)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(max_frac), &out);
  return out;
}

napi_value SetPathMotor(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(3, 5)
  uint32_t id;
  int32_t state;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &state)) {
    ThrowTypeError(env, "setPathMotor: invalid args");
    return nullptr;
  }
  bool ok_res = handle->world->SetPathMotorState(id, state);
  if (ok_res && argc > 3) {
    double vel = 0.0;
    napi_get_value_double(env, args[3], &vel);
    handle->world->SetPathTargetVelocity(id, static_cast<float>(vel));
  }
  if (ok_res && argc > 4) {
    double frac = 0.0;
    napi_get_value_double(env, args[4], &frac);
    handle->world->SetPathTargetFraction(id, static_cast<float>(frac));
  }
  napi_value out;
  napi_get_boolean(env, ok_res, &out);
  return out;
}

napi_value GetPathMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPathMotorState: invalid args");
    return nullptr;
  }
  int32_t state = 0;
  float vel = 0.0f, frac = 0.0f;
  if (!handle->world->GetPathMotorState(id, state, vel, frac)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, ns, nv, nf;
  napi_create_object(env, &out);
  napi_create_int32(env, state, &ns);
  napi_create_double(env, static_cast<double>(vel), &nv);
  napi_create_double(env, static_cast<double>(frac), &nf);
  napi_set_named_property(env, out, "state", ns);
  napi_set_named_property(env, out, "targetVelocity", nv);
  napi_set_named_property(env, out, "targetFraction", nf);
  return out;
}

napi_value SetPathMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "setPathMotorSpring: invalid args");
    return nullptr;
  }
  MotorSpringParams p;
  ParseMotorSpringParams(env, args[2], p);
  napi_value out;
  napi_get_boolean(env, handle->world->SetPathMotorSpring(id, p), &out);
  return out;
}

napi_value GetPathMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPathMotorSpring: invalid args");
    return nullptr;
  }
  MotorSettings ms;
  if (!handle->world->GetPathMotorSpring(id, ms)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  return MakeMotorSpringObject(env, ms);
}

// ─── Distance constraint limits & spring ─────────────────────────────────────

napi_value SetDistanceLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id; double mn, mx;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &mn) || !GetDoubleArg(env, args[3], &mx)) {
    ThrowTypeError(env, "setDistanceLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetDistanceLimits(id, static_cast<float>(mn), static_cast<float>(mx)), &out);
  return out;
}

napi_value GetDistanceLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getDistanceLimits: invalid args");
    return nullptr;
  }
  float mn = 0.0f, mx = 0.0f;
  if (!handle->world->GetDistanceLimits(id, mn, mx)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value obj, mnv, mxv;
  napi_create_object(env, &obj);
  napi_create_double(env, static_cast<double>(mn), &mnv);
  napi_create_double(env, static_cast<double>(mx), &mxv);
  napi_set_named_property(env, obj, "min", mnv);
  napi_set_named_property(env, obj, "max", mxv);
  return obj;
}

napi_value SetDistanceLimitsSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "setDistanceLimitsSpring: invalid args");
    return nullptr;
  }
  SpringSettings ss;
  ParseSpringSettings(env, args[2], ss);
  napi_value out;
  napi_get_boolean(env, handle->world->SetDistanceLimitsSpring(id, ss), &out);
  return out;
}

napi_value GetDistanceLimitsSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getDistanceLimitsSpring: invalid args");
    return nullptr;
  }
  SpringSettings ss;
  if (!handle->world->GetDistanceLimitsSpring(id, ss)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  return MakeSpringSettingsObject(env, ss);
}

// ─── RackAndPinion constraint ────────────────────────────────────────────────

napi_value CreateRackAndPinionConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t a, b;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b)) {
    ThrowTypeError(env, "createRackAndPinionConstraint: invalid body IDs");
    return nullptr;
  }
  napi_value opts = args[3];
  auto getVec = [&](const char *key, Vec3 def) -> Vec3 {
    Vec3 v = def;
    GetVec3ObjProp(env, opts, key, v);
    return v;
  };
  auto getF = [&](const char *key, float def) -> float {
    napi_value v;
    if (napi_get_named_property(env, opts, key, &v) == napi_ok) {
      double d = 0.0;
      if (GetDoubleArg(env, v, &d)) return static_cast<float>(d);
    }
    return def;
  };
  auto getU = [&](const char *key, uint32_t def) -> uint32_t {
    napi_value v;
    if (napi_get_named_property(env, opts, key, &v) == napi_ok) {
      uint32_t u = 0;
      if (GetUInt32Arg(env, v, &u)) return u;
    }
    return def;
  };
  Vec3 hinge_axis = getVec("hingeAxis", Vec3(0, 1, 0));
  Vec3 slider_axis = getVec("sliderAxis", Vec3(1, 0, 0));
  float ratio = getF("ratio", 1.0f);
  uint32_t pinion_id = getU("pinionConstraintId", 0);
  uint32_t rack_id   = getU("rackConstraintId",   0);
  const uint32_t id = handle->world->CreateRackAndPinionConstraint(a, b, hinge_axis, slider_axis, ratio, pinion_id, rack_id);
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value GetRackAndPinionLambda(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getRackAndPinionLambda: invalid args");
    return nullptr;
  }
  float val = 0.0f;
  if (!handle->world->GetRackAndPinionLambda(id, val)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(val), &out);
  return out;
}

// ─── HeightField shape ────────────────────────────────────────────────────────

napi_value CreateHeightField(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  // args[1] = opts { samples: Float32Array|Array, sampleCount, offset, scale, position, friction, restitution }
  napi_value opts = args[1];

  // sampleCount
  uint32_t sample_count = 0;
  {
    napi_value v;
    if (napi_get_named_property(env, opts, "sampleCount", &v) != napi_ok || !GetUInt32Arg(env, v, &sample_count) || sample_count < 2) {
      ThrowTypeError(env, "createHeightField: sampleCount must be >= 2");
      return nullptr;
    }
  }

  // samples — accept Float32Array or regular Array
  std::vector<float> samples;
  {
    napi_value sv;
    if (napi_get_named_property(env, opts, "samples", &sv) != napi_ok) {
      ThrowTypeError(env, "createHeightField: missing samples");
      return nullptr;
    }
    bool is_typed = false;
    napi_is_typedarray(env, sv, &is_typed);
    if (is_typed) {
      napi_typedarray_type ta_type;
      size_t byte_offset = 0, length = 0;
      void *data = nullptr;
      napi_value buf;
      if (napi_get_typedarray_info(env, sv, &ta_type, &length, &data, &buf, &byte_offset) != napi_ok || ta_type != napi_float32_array) {
        ThrowTypeError(env, "createHeightField: samples must be Float32Array or Array");
        return nullptr;
      }
      samples.assign(static_cast<float *>(data), static_cast<float *>(data) + length);
    } else {
      bool is_array = false;
      napi_is_array(env, sv, &is_array);
      if (!is_array) {
        ThrowTypeError(env, "createHeightField: samples must be Float32Array or Array");
        return nullptr;
      }
      uint32_t len = 0;
      napi_get_array_length(env, sv, &len);
      samples.reserve(len);
      for (uint32_t i = 0; i < len; ++i) {
        napi_value elem;
        napi_get_element(env, sv, i, &elem);
        double d = 0.0;
        GetDoubleArg(env, elem, &d);
        samples.push_back(static_cast<float>(d));
      }
    }
  }

  Vec3 offset = Vec3::sZero();
  Vec3 scale(1.0f, 1.0f, 1.0f);
  GetVec3ObjProp(env, opts, "offset", offset);
  GetVec3ObjProp(env, opts, "scale", scale);

  double x = 0, y = 0, z = 0;
  {
    napi_value posv;
    if (napi_get_named_property(env, opts, "position", &posv) == napi_ok) {
      napi_value xv, yv, zv;
      if (napi_get_named_property(env, posv, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &x);
      if (napi_get_named_property(env, posv, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &y);
      if (napi_get_named_property(env, posv, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &z);
    }
  }

  double friction = 0.5, restitution = 0.0;
  {
    napi_value v;
    if (napi_get_named_property(env, opts, "friction", &v) == napi_ok) GetDoubleArg(env, v, &friction);
    if (napi_get_named_property(env, opts, "restitution", &v) == napi_ok) GetDoubleArg(env, v, &restitution);
  }

  // materialIndices — Uint8Array or Array, one per sample
  std::vector<uint8_t> mat_indices;
  {
    napi_value mi_v;
    if (napi_get_named_property(env, opts, "materialIndices", &mi_v) == napi_ok) {
      bool is_typed = false;
      bool is_arr = false;
      napi_is_typedarray(env, mi_v, &is_typed);
      napi_is_array(env, mi_v, &is_arr);
      if (is_typed) {
        napi_typedarray_type ta_type;
        size_t byte_offset = 0, length = 0;
        void *data = nullptr;
        napi_value buf;
        napi_get_typedarray_info(env, mi_v, &ta_type, &length, &data, &buf, &byte_offset);
        if (ta_type == napi_uint8_array) {
          mat_indices.assign(static_cast<uint8_t *>(data), static_cast<uint8_t *>(data) + length);
        }
      } else if (is_arr) {
        uint32_t len = 0;
        napi_get_array_length(env, mi_v, &len);
        mat_indices.reserve(len);
        for (uint32_t k = 0; k < len; ++k) {
          napi_value elem;
          napi_get_element(env, mi_v, k, &elem);
          uint32_t idx = 0;
          GetUInt32Arg(env, elem, &idx);
          mat_indices.push_back(static_cast<uint8_t>(idx));
        }
      }
    }
  }

  // materials — array of { friction, restitution }
  PhysicsMaterialList mat_list;
  {
    napi_value mats_v;
    if (napi_get_named_property(env, opts, "materials", &mats_v) == napi_ok) {
      bool is_arr = false;
      napi_is_array(env, mats_v, &is_arr);
      if (is_arr) {
        uint32_t mats_len = 0;
        napi_get_array_length(env, mats_v, &mats_len);
        for (uint32_t k = 0; k < mats_len; ++k) {
          napi_value mobj;
          napi_get_element(env, mats_v, k, &mobj);
          double mf = 0.5, mr = 0.0;
          napi_value fv, rv2;
          if (napi_get_named_property(env, mobj, "friction", &fv) == napi_ok) GetDoubleArg(env, fv, &mf);
          if (napi_get_named_property(env, mobj, "restitution", &rv2) == napi_ok) GetDoubleArg(env, rv2, &mr);
          mat_list.push_back(new IndexedMaterial(k, static_cast<float>(mf), static_cast<float>(mr)));
        }
      }
    }
  }

  const uint32_t id = handle->world->CreateHeightField(
      samples, sample_count, offset, scale, mat_indices, mat_list, x, y, z,
      static_cast<float>(friction), static_cast<float>(restitution));
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create height field");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

// ─── Compound shapes ─────────────────────────────────────────────────────────

static bool ParseSubShapeSpecArray(napi_env env, napi_value arr, std::vector<SubShapeSpec> &out) {
  bool is_array = false;
  napi_is_array(env, arr, &is_array);
  if (!is_array) return false;
  uint32_t len = 0;
  napi_get_array_length(env, arr, &len);
  out.reserve(len);
  for (uint32_t i = 0; i < len; ++i) {
    napi_value elem;
    if (napi_get_element(env, arr, i, &elem) != napi_ok) return false;
    SubShapeSpec spec;
    if (!ParseSubShapeSpec(env, elem, spec)) return false;
    out.push_back(std::move(spec));
  }
  return true;
}

static uint32_t ParseCompoundBodyOpts(napi_env env, napi_value opts,
                                       std::vector<SubShapeSpec> &subs,
                                       double &x, double &y, double &z,
                                       bool &dynamic, float &friction, float &restitution) {
  napi_value subv;
  if (napi_get_named_property(env, opts, "shapes", &subv) != napi_ok || !ParseSubShapeSpecArray(env, subv, subs)) return 0;
  napi_value posv;
  if (napi_get_named_property(env, opts, "position", &posv) == napi_ok) {
    napi_value xv, yv, zv;
    if (napi_get_named_property(env, posv, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &x);
    if (napi_get_named_property(env, posv, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &y);
    if (napi_get_named_property(env, posv, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &z);
  }
  napi_value dv;
  if (napi_get_named_property(env, opts, "dynamic", &dv) == napi_ok) GetBoolArg(env, dv, &dynamic);
  {
    napi_value v;
    if (napi_get_named_property(env, opts, "friction", &v) == napi_ok) { double d; if (GetDoubleArg(env, v, &d)) friction = static_cast<float>(d); }
    if (napi_get_named_property(env, opts, "restitution", &v) == napi_ok) { double d; if (GetDoubleArg(env, v, &d)) restitution = static_cast<float>(d); }
  }
  return 1;
}

napi_value CreateStaticCompound(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  std::vector<SubShapeSpec> subs;
  double x = 0, y = 0, z = 0; bool dynamic = false; float friction = 0.5f, restitution = 0.0f;
  if (!ParseCompoundBodyOpts(env, args[1], subs, x, y, z, dynamic, friction, restitution)) {
    ThrowTypeError(env, "createStaticCompound: invalid args");
    return nullptr;
  }
  const uint32_t id = handle->world->CreateStaticCompound(subs, x, y, z, dynamic, friction, restitution);
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create static compound");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateMutableCompound(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  std::vector<SubShapeSpec> subs;
  double x = 0, y = 0, z = 0; bool dynamic = false; float friction = 0.5f, restitution = 0.0f;
  if (!ParseCompoundBodyOpts(env, args[1], subs, x, y, z, dynamic, friction, restitution)) {
    ThrowTypeError(env, "createMutableCompound: invalid args");
    return nullptr;
  }
  const uint32_t id = handle->world->CreateMutableCompound(subs, x, y, z, dynamic, friction, restitution);
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create mutable compound");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value AddMutableSubShape(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t body_id;
  if (!GetUInt32Arg(env, args[1], &body_id)) {
    ThrowTypeError(env, "addMutableSubShape: invalid body_id");
    return nullptr;
  }
  SubShapeSpec spec;
  if (!ParseSubShapeSpec(env, args[2], spec)) {
    ThrowTypeError(env, "addMutableSubShape: invalid sub-shape spec");
    return nullptr;
  }
  const int32_t idx = handle->world->AddMutableSubShape(body_id, spec);
  napi_value out;
  napi_create_int32(env, idx, &out);
  return out;
}

napi_value RemoveMutableSubShape(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t body_id, index;
  if (!GetUInt32Arg(env, args[1], &body_id) || !GetUInt32Arg(env, args[2], &index)) {
    ThrowTypeError(env, "removeMutableSubShape: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->RemoveMutableSubShape(body_id, index), &out);
  return out;
}

napi_value ModifyMutableSubShape(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t body_id, index;
  if (!GetUInt32Arg(env, args[1], &body_id) || !GetUInt32Arg(env, args[2], &index)) {
    ThrowTypeError(env, "modifyMutableSubShape: invalid args");
    return nullptr;
  }
  Vec3 pos = Vec3::sZero();
  Quat rot = Quat::sIdentity();
  {
    napi_value posv = args[3];
    napi_valuetype vt = napi_undefined;
    napi_typeof(env, posv, &vt);
    if (vt == napi_object) {
      napi_value xv, yv, zv; double px = 0, py = 0, pz = 0;
      if (napi_get_named_property(env, posv, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &px);
      if (napi_get_named_property(env, posv, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &py);
      if (napi_get_named_property(env, posv, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &pz);
      pos = Vec3(static_cast<float>(px), static_cast<float>(py), static_cast<float>(pz));
    }
  }
  {
    napi_value rotv = args[4];
    napi_valuetype vt = napi_undefined;
    napi_typeof(env, rotv, &vt);
    if (vt == napi_object) {
      napi_value xv, yv, zv, wv; double rx = 0, ry = 0, rz = 0, rw = 1;
      if (napi_get_named_property(env, rotv, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &rx);
      if (napi_get_named_property(env, rotv, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &ry);
      if (napi_get_named_property(env, rotv, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &rz);
      if (napi_get_named_property(env, rotv, "w", &wv) == napi_ok) GetDoubleArg(env, wv, &rw);
      rot = Quat(static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(rz), static_cast<float>(rw));
    }
  }
  napi_value out;
  napi_get_boolean(env, handle->world->ModifyMutableSubShape(body_id, index, pos, rot), &out);
  return out;
}

napi_value AdjustMutableCenterOfMass(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t body_id;
  if (!GetUInt32Arg(env, args[1], &body_id)) {
    ThrowTypeError(env, "adjustMutableCenterOfMass: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AdjustMutableCenterOfMass(body_id), &out);
  return out;
}

// ─────────────────────────────────────────────────────────────────────────────

napi_value CreateSkeleton(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(1)
  uint32_t id = handle->world->CreateSkeleton();
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value AddSkeletonJoint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t skeleton_id;
  std::string name;
  int32_t parent_idx;
  if (!GetUInt32Arg(env, args[1], &skeleton_id) || !GetStringArg(env, args[2], &name) || !GetInt32Arg(env, args[3], &parent_idx)) {
    ThrowTypeError(env, "addSkeletonJoint: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AddSkeletonJoint(skeleton_id, name, parent_idx), &out);
  return out;
}

napi_value FinalizeSkeleton(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t skeleton_id;
  if (!GetUInt32Arg(env, args[1], &skeleton_id)) {
    ThrowTypeError(env, "finalizeSkeleton: skeletonId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->FinalizeSkeleton(skeleton_id), &out);
  return out;
}

napi_value GetSkeletonJointCount(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t skeleton_id;
  if (!GetUInt32Arg(env, args[1], &skeleton_id)) {
    ThrowTypeError(env, "getSkeletonJointCount: skeletonId must be uint32");
    return nullptr;
  }
  int count = handle->world->GetSkeletonJointCount(skeleton_id);
  if (count < 0) {
    ThrowError(env, "Skeleton not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, count, &out);
  return out;
}

napi_value GetSkeletonJointInfo(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t skeleton_id;
  int32_t joint_index;
  if (!GetUInt32Arg(env, args[1], &skeleton_id) || !GetInt32Arg(env, args[2], &joint_index)) {
    ThrowTypeError(env, "getSkeletonJointInfo: invalid args");
    return nullptr;
  }

  std::string name;
  int32_t parent_index = -1;
  if (!handle->world->GetSkeletonJointInfo(skeleton_id, joint_index, name, parent_index)) {
    ThrowError(env, "Skeleton or joint not found");
    return nullptr;
  }

  napi_value out;
  napi_create_object(env, &out);
  napi_value name_v;
  napi_value parent_v;
  napi_create_string_utf8(env, name.c_str(), name.size(), &name_v);
  napi_create_int32(env, parent_index, &parent_v);
  napi_set_named_property(env, out, "name", name_v);
  napi_set_named_property(env, out, "parentIndex", parent_v);
  return out;
}

napi_value GetSkeletonJointIndex(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t skeleton_id;
  std::string name;
  if (!GetUInt32Arg(env, args[1], &skeleton_id) || !GetStringArg(env, args[2], &name)) {
    ThrowTypeError(env, "getSkeletonJointIndex: invalid args");
    return nullptr;
  }

  int idx = handle->world->GetSkeletonJointIndex(skeleton_id, name);
  if (idx < 0) {
    ThrowError(env, "Skeleton or joint not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, idx, &out);
  return out;
}

napi_value CreateRagdollSettings(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t skeleton_id;
  double half_h, radius, spacing;
  if (!GetUInt32Arg(env, args[1], &skeleton_id) || !GetDoubleArg(env, args[2], &half_h) || !GetDoubleArg(env, args[3], &radius) ||
      !GetDoubleArg(env, args[4], &spacing) || half_h < 0 || radius <= 0 || spacing < 0) {
    ThrowTypeError(env, "createRagdollSettings: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateRagdollSettings(skeleton_id, static_cast<float>(half_h), static_cast<float>(radius), static_cast<float>(spacing));
  if (id == 0) {
    ThrowError(env, "Failed to create ragdoll settings");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateRagdoll(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t settings_id;
  uint32_t group_id;
  uint32_t user_lo;
  bool activate;
  if (!GetUInt32Arg(env, args[1], &settings_id) || !GetUInt32Arg(env, args[2], &group_id) || !GetUInt32Arg(env, args[3], &user_lo) ||
      !GetBoolArg(env, args[4], &activate)) {
    ThrowTypeError(env, "createRagdoll: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateRagdoll(settings_id, group_id, static_cast<uint64_t>(user_lo), activate);
  if (id == 0) {
    ThrowError(env, "Failed to create ragdoll");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value DestroyRagdoll(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "destroyRagdoll: ragdollId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->DestroyRagdoll(ragdoll_id), &out);
  return out;
}

napi_value GetRagdollBodyCount(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "getRagdollBodyCount: ragdollId must be uint32");
    return nullptr;
  }
  int count = handle->world->GetRagdollBodyCount(ragdoll_id);
  if (count < 0) {
    ThrowError(env, "Ragdoll not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, count, &out);
  return out;
}

napi_value GetRagdollBoneBodyId(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t ragdoll_id;
  int32_t index;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetInt32Arg(env, args[2], &index)) {
    ThrowTypeError(env, "getRagdollBoneBodyId: invalid args");
    return nullptr;
  }
  uint32_t body_id = 0;
  if (!handle->world->GetRagdollBoneBodyId(ragdoll_id, index, body_id)) {
    ThrowError(env, "Ragdoll or bone not found");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, body_id, &out);
  return out;
}

napi_value GetRagdollBoneTransform(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t ragdoll_id;
  int32_t index;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetInt32Arg(env, args[2], &index)) {
    ThrowTypeError(env, "getRagdollBoneTransform: invalid args");
    return nullptr;
  }

  RVec3 pos;
  Quat rot;
  if (!handle->world->GetRagdollBoneTransform(ragdoll_id, index, pos, rot)) {
    ThrowError(env, "Failed to get ragdoll bone transform");
    return nullptr;
  }

  napi_value out;
  napi_create_object(env, &out);
  napi_value p = MakeVec3Object(env, pos);
  napi_value q = MakeQuatObject(env, rot);
  napi_set_named_property(env, out, "position", p);
  napi_set_named_property(env, out, "rotation", q);
  return out;
}

napi_value SetRagdollBoneTransform(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  uint32_t ragdoll_id;
  int32_t index;
  double px, py, pz, qx, qy, qz, qw;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetInt32Arg(env, args[2], &index) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &qx) || !GetDoubleArg(env, args[7], &qy) || !GetDoubleArg(env, args[8], &qz) ||
      !GetDoubleArg(env, args[9], &qw)) {
    ThrowTypeError(env, "setRagdollBoneTransform: invalid args");
    return nullptr;
  }

  bool ok = handle->world->SetRagdollBoneTransform(
      ragdoll_id,
      index,
      RVec3(px, py, pz),
      Quat(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz), static_cast<float>(qw)),
      true);

  napi_value out;
  napi_get_boolean(env, ok, &out);
  return out;
}

napi_value SetRagdollJointShape(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t settings_id;
  int32_t joint_index;
  if (!GetUInt32Arg(env, args[1], &settings_id) || !GetInt32Arg(env, args[2], &joint_index)) {
    ThrowTypeError(env, "setRagdollJointShape: invalid args");
    return nullptr;
  }
  napi_valuetype vt = napi_undefined;
  if (napi_typeof(env, args[3], &vt) != napi_ok || vt != napi_object) {
    ThrowTypeError(env, "setRagdollJointShape: config must be object");
    return nullptr;
  }
  int kind = 0;
  float half_h = 0.2f, radius = 0.1f, hx = 0.1f, hy = 0.2f, hz = 0.1f;
  napi_value kind_v;
  if (napi_get_named_property(env, args[3], "kind", &kind_v) == napi_ok) {
    std::string ks;
    if (GetStringArg(env, kind_v, &ks)) {
      if (ks == "box") kind = 1;
      else if (ks == "sphere") kind = 2;
    }
  }
  auto getF3 = [&](const char *key, float &dest) {
    napi_value v;
    if (napi_get_named_property(env, args[3], key, &v) == napi_ok) {
      double d = 0.0;
      if (GetDoubleArg(env, v, &d)) dest = static_cast<float>(d);
    }
  };
  getF3("halfHeight", half_h);
  getF3("radius", radius);
  napi_value he_v;
  if (napi_get_named_property(env, args[3], "halfExtents", &he_v) == napi_ok) {
    napi_valuetype hevt = napi_undefined;
    if (napi_typeof(env, he_v, &hevt) == napi_ok && hevt == napi_object) {
      napi_value xv, yv, zv;
      double dx = 0, dy = 0, dz = 0;
      if (napi_get_named_property(env, he_v, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &dx);
      if (napi_get_named_property(env, he_v, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &dy);
      if (napi_get_named_property(env, he_v, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &dz);
      hx = static_cast<float>(dx); hy = static_cast<float>(dy); hz = static_cast<float>(dz);
    }
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetRagdollJointShape(settings_id, joint_index, kind, half_h, radius, hx, hy, hz), &out);
  return out;
}

napi_value SetRagdollJointTransform(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  uint32_t settings_id;
  int32_t joint_index;
  double px, py, pz, qx, qy, qz, qw;
  if (!GetUInt32Arg(env, args[1], &settings_id) || !GetInt32Arg(env, args[2], &joint_index) ||
      !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &qx) ||
      !GetDoubleArg(env, args[7], &qy) || !GetDoubleArg(env, args[8], &qz) ||
      !GetDoubleArg(env, args[9], &qw)) {
    ThrowTypeError(env, "setRagdollJointTransform: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetRagdollJointTransform(
      settings_id, joint_index,
      RVec3(px, py, pz),
      Quat(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz), static_cast<float>(qw))), &out);
  return out;
}

napi_value SetRagdollJointConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t settings_id;
  int32_t joint_index;
  if (!GetUInt32Arg(env, args[1], &settings_id) || !GetInt32Arg(env, args[2], &joint_index)) {
    ThrowTypeError(env, "setRagdollJointConstraint: invalid args");
    return nullptr;
  }
  RagdollJointConstraintConfig cfg;
  ParseRagdollJointConstraintConfig(env, args[3], cfg);
  napi_value out;
  napi_get_boolean(env, handle->world->SetRagdollJointConstraint(settings_id, joint_index, cfg), &out);
  return out;
}

napi_value GetRagdollConstraintIds(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "getRagdollConstraintIds: invalid args");
    return nullptr;
  }
  std::vector<uint32_t> ids;
  if (!handle->world->GetRagdollConstraintIds(ragdoll_id, ids)) {
    ThrowError(env, "Ragdoll not found");
    return nullptr;
  }
  napi_value arr;
  napi_create_array_with_length(env, ids.size(), &arr);
  for (size_t i = 0; i < ids.size(); ++i) {
    napi_value v;
    napi_create_uint32(env, ids[i], &v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), v);
  }
  return arr;
}

// --- SkeletonPose NAPI ---

napi_value CreateSkeletonPose(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "createSkeletonPose: expected ragdoll id"); return nullptr;
  }
  uint32_t id = handle->world->CreateSkeletonPose(ragdoll_id);
  napi_value result;
  napi_create_uint32(env, id, &result);
  return result;
}

napi_value DestroySkeletonPose(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t pose_id;
  if (!GetUInt32Arg(env, args[1], &pose_id)) {
    ThrowTypeError(env, "destroySkeletonPose: expected pose id"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->DestroySkeletonPose(pose_id), &result);
  return result;
}

napi_value SetPoseJoint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  uint32_t pose_id; int32_t ji;
  double tx, ty, tz, rx, ry, rz, rw;
  if (!GetUInt32Arg(env, args[1], &pose_id) || !GetInt32Arg(env, args[2], &ji) ||
      !GetDoubleArg(env, args[3], &tx) || !GetDoubleArg(env, args[4], &ty) ||
      !GetDoubleArg(env, args[5], &tz) || !GetDoubleArg(env, args[6], &rx) ||
      !GetDoubleArg(env, args[7], &ry) || !GetDoubleArg(env, args[8], &rz) ||
      !GetDoubleArg(env, args[9], &rw)) {
    ThrowTypeError(env, "setPoseJoint: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->SetPoseJoint(pose_id, ji,
    (float)tx, (float)ty, (float)tz, (float)rx, (float)ry, (float)rz, (float)rw), &result);
  return result;
}

napi_value GetPoseJoint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t pose_id; int32_t ji;
  if (!GetUInt32Arg(env, args[1], &pose_id) || !GetInt32Arg(env, args[2], &ji)) {
    ThrowTypeError(env, "getPoseJoint: invalid args"); return nullptr;
  }
  Vec3 t; Quat r;
  if (!handle->world->GetPoseJoint(pose_id, ji, t, r)) return nullptr;
  napi_value obj, trans, rot;
  napi_create_object(env, &obj);
  napi_create_object(env, &trans);
  napi_create_object(env, &rot);
  SetF64Prop(env, trans, "x", t.GetX()); SetF64Prop(env, trans, "y", t.GetY()); SetF64Prop(env, trans, "z", t.GetZ());
  SetF64Prop(env, rot, "x", r.GetX()); SetF64Prop(env, rot, "y", r.GetY()); SetF64Prop(env, rot, "z", r.GetZ()); SetF64Prop(env, rot, "w", r.GetW());
  napi_set_named_property(env, obj, "translation", trans);
  napi_set_named_property(env, obj, "rotation", rot);
  return obj;
}

napi_value SetPoseRootOffset(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t pose_id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &pose_id) ||
      !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) ||
      !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "setPoseRootOffset: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->SetPoseRootOffset(pose_id, x, y, z), &result);
  return result;
}

napi_value GetPoseRootOffset(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t pose_id;
  if (!GetUInt32Arg(env, args[1], &pose_id)) {
    ThrowTypeError(env, "getPoseRootOffset: invalid args"); return nullptr;
  }
  RVec3 out;
  if (!handle->world->GetPoseRootOffset(pose_id, out)) return nullptr;
  napi_value obj;
  napi_create_object(env, &obj);
  SetF64Prop(env, obj, "x", (double)out.GetX());
  SetF64Prop(env, obj, "y", (double)out.GetY());
  SetF64Prop(env, obj, "z", (double)out.GetZ());
  return obj;
}

napi_value CalculatePoseJointMatrices(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t pose_id;
  if (!GetUInt32Arg(env, args[1], &pose_id)) {
    ThrowTypeError(env, "calculatePoseJointMatrices: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->CalculatePoseJointMatrices(pose_id), &result);
  return result;
}

napi_value GetPoseJointCount(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t pose_id;
  if (!GetUInt32Arg(env, args[1], &pose_id)) {
    ThrowTypeError(env, "getPoseJointCount: invalid args"); return nullptr;
  }
  napi_value result;
  napi_create_int32(env, handle->world->GetPoseJointCount(pose_id), &result);
  return result;
}

// --- Extended Ragdoll NAPI ---

napi_value RagdollSetPose(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(3, 4)
  uint32_t ragdoll_id, pose_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &pose_id)) {
    ThrowTypeError(env, "ragdollSetPose: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 4) { bool b; if (GetBoolArg(env, args[3], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollSetPose(ragdoll_id, pose_id, lock), &result);
  return result;
}

napi_value RagdollGetPose(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(3, 4)
  uint32_t ragdoll_id, pose_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &pose_id)) {
    ThrowTypeError(env, "ragdollGetPose: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 4) { bool b; if (GetBoolArg(env, args[3], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollGetPose(ragdoll_id, pose_id, lock), &result);
  return result;
}

napi_value RagdollDriveToPoseKinematics(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(4, 5)
  uint32_t ragdoll_id, pose_id;
  double dt;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &pose_id) ||
      !GetDoubleArg(env, args[3], &dt)) {
    ThrowTypeError(env, "ragdollDriveToPoseKinematics: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 5) { bool b; if (GetBoolArg(env, args[4], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollDriveToPoseKinematics(ragdoll_id, pose_id, (float)dt, lock), &result);
  return result;
}

napi_value RagdollDriveToPoseMotors(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t ragdoll_id, pose_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &pose_id)) {
    ThrowTypeError(env, "ragdollDriveToPoseMotors: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollDriveToPoseMotors(ragdoll_id, pose_id), &result);
  return result;
}

napi_value RagdollActivate(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(2, 3)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollActivate: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 3) { bool b; if (GetBoolArg(env, args[2], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollActivate(ragdoll_id, lock), &result);
  return result;
}

napi_value RagdollIsActive(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollIsActive: invalid args"); return nullptr;
  }
  napi_value result;
  napi_create_int32(env, handle->world->RagdollIsActive(ragdoll_id), &result);
  return result;
}

napi_value RagdollGetRootTransform(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollGetRootTransform: invalid args"); return nullptr;
  }
  RVec3 pos; Quat rot;
  if (!handle->world->RagdollGetRootTransform(ragdoll_id, pos, rot)) return nullptr;
  napi_value obj, pobj, robj;
  napi_create_object(env, &obj);
  napi_create_object(env, &pobj);
  napi_create_object(env, &robj);
  SetF64Prop(env, pobj, "x", (double)pos.GetX()); SetF64Prop(env, pobj, "y", (double)pos.GetY()); SetF64Prop(env, pobj, "z", (double)pos.GetZ());
  SetF64Prop(env, robj, "x", rot.GetX()); SetF64Prop(env, robj, "y", rot.GetY()); SetF64Prop(env, robj, "z", rot.GetZ()); SetF64Prop(env, robj, "w", rot.GetW());
  napi_set_named_property(env, obj, "position", pobj);
  napi_set_named_property(env, obj, "rotation", robj);
  return obj;
}

napi_value RagdollGetWorldSpaceBounds(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollGetWorldSpaceBounds: invalid args"); return nullptr;
  }
  Vec3 bmin, bmax;
  if (!handle->world->RagdollGetWorldSpaceBounds(ragdoll_id, bmin, bmax)) return nullptr;
  napi_value obj, minobj, maxobj;
  napi_create_object(env, &obj);
  napi_create_object(env, &minobj);
  napi_create_object(env, &maxobj);
  SetF64Prop(env, minobj, "x", bmin.GetX()); SetF64Prop(env, minobj, "y", bmin.GetY()); SetF64Prop(env, minobj, "z", bmin.GetZ());
  SetF64Prop(env, maxobj, "x", bmax.GetX()); SetF64Prop(env, maxobj, "y", bmax.GetY()); SetF64Prop(env, maxobj, "z", bmax.GetZ());
  napi_set_named_property(env, obj, "min", minobj);
  napi_set_named_property(env, obj, "max", maxobj);
  return obj;
}

napi_value RagdollSetGroupID(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(3, 4)
  uint32_t ragdoll_id, group_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &group_id)) {
    ThrowTypeError(env, "ragdollSetGroupID: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 4) { bool b; if (GetBoolArg(env, args[3], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollSetGroupID(ragdoll_id, group_id, lock), &result);
  return result;
}

napi_value RagdollResetWarmStart(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollResetWarmStart: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollResetWarmStart(ragdoll_id), &result);
  return result;
}

napi_value RagdollSetLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(5, 6)
  uint32_t ragdoll_id;
  double vx, vy, vz;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) ||
      !GetDoubleArg(env, args[2], &vx) || !GetDoubleArg(env, args[3], &vy) ||
      !GetDoubleArg(env, args[4], &vz)) {
    ThrowTypeError(env, "ragdollSetLinearVelocity: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 6) { bool b; if (GetBoolArg(env, args[5], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollSetLinearVelocity(ragdoll_id, (float)vx, (float)vy, (float)vz, lock), &result);
  return result;
}

napi_value RagdollAddLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(5, 6)
  uint32_t ragdoll_id;
  double vx, vy, vz;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) ||
      !GetDoubleArg(env, args[2], &vx) || !GetDoubleArg(env, args[3], &vy) ||
      !GetDoubleArg(env, args[4], &vz)) {
    ThrowTypeError(env, "ragdollAddLinearVelocity: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 6) { bool b; if (GetBoolArg(env, args[5], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollAddLinearVelocity(ragdoll_id, (float)vx, (float)vy, (float)vz, lock), &result);
  return result;
}

napi_value RagdollSetLinearAndAngularVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(8, 9)
  uint32_t ragdoll_id;
  double lvx, lvy, lvz, avx, avy, avz;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) ||
      !GetDoubleArg(env, args[2], &lvx) || !GetDoubleArg(env, args[3], &lvy) ||
      !GetDoubleArg(env, args[4], &lvz) || !GetDoubleArg(env, args[5], &avx) ||
      !GetDoubleArg(env, args[6], &avy) || !GetDoubleArg(env, args[7], &avz)) {
    ThrowTypeError(env, "ragdollSetLinearAndAngularVelocity: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 9) { bool b; if (GetBoolArg(env, args[8], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollSetLinearAndAngularVelocity(ragdoll_id,
    (float)lvx, (float)lvy, (float)lvz, (float)avx, (float)avy, (float)avz, lock), &result);
  return result;
}

napi_value RagdollAddImpulse(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(5, 6)
  uint32_t ragdoll_id;
  double ix, iy, iz;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) ||
      !GetDoubleArg(env, args[2], &ix) || !GetDoubleArg(env, args[3], &iy) ||
      !GetDoubleArg(env, args[4], &iz)) {
    ThrowTypeError(env, "ragdollAddImpulse: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 6) { bool b; if (GetBoolArg(env, args[5], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollAddImpulse(ragdoll_id, (float)ix, (float)iy, (float)iz, lock), &result);
  return result;
}

napi_value RagdollAddToPhysicsSystem(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(2, 3)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollAddToPhysicsSystem: invalid args"); return nullptr;
  }
  bool activate = true;
  if (argc >= 3) { bool b; if (GetBoolArg(env, args[2], &b)) activate = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollAddToPhysicsSystem(ragdoll_id, activate), &result);
  return result;
}

napi_value RagdollRemoveFromPhysicsSystem(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollRemoveFromPhysicsSystem: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollRemoveFromPhysicsSystem(ragdoll_id), &result);
  return result;
}

napi_value RagdollStabilize(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollStabilize: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollStabilize(ragdoll_id), &result);
  return result;
}

napi_value SnapshotState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(1)
  std::vector<uint8_t> data = handle->world->SnapshotState();
  napi_value buf = nullptr;
  void *ptr = nullptr;
  napi_create_buffer(env, data.size(), &ptr, &buf);
  if (!data.empty() && ptr) memcpy(ptr, data.data(), data.size());
  return buf;
}

napi_value ApplySnapshot(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  bool is_buf = false;
  napi_is_buffer(env, args[1], &is_buf);
  if (!is_buf) { ThrowTypeError(env, "applySnapshot: expected Buffer"); return nullptr; }
  void *data = nullptr; size_t len = 0;
  napi_get_buffer_info(env, args[1], &data, &len);
  napi_value result;
  napi_get_boolean(env, handle->world->ApplySnapshot(static_cast<const uint8_t *>(data), len), &result);
  return result;
}

napi_value SaveScene(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(1)
  std::vector<uint8_t> data = handle->world->SaveScene();
  napi_value buf = nullptr;
  void *ptr = nullptr;
  napi_create_buffer(env, data.size(), &ptr, &buf);
  if (!data.empty() && ptr) memcpy(ptr, data.data(), data.size());
  return buf;
}

napi_value LoadScene(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  bool is_buf = false;
  napi_is_buffer(env, args[1], &is_buf);
  if (!is_buf) { ThrowTypeError(env, "loadScene: expected Buffer"); return nullptr; }
  void *data = nullptr; size_t len = 0;
  napi_get_buffer_info(env, args[1], &data, &len);
  int count = handle->world->LoadScene(static_cast<const uint8_t *>(data), len);
  if (count < 0) { ThrowError(env, "loadScene: failed to restore scene"); return nullptr; }
  napi_value result;
  napi_create_int32(env, count, &result);
  return result;
}

// ── CharacterVirtual NAPI wrappers ────────────────────────────────────────

napi_value CreateCharacter(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(9)
  double half_height, radius, x, y, z, mass, max_strength, max_slope_angle;
  if (!GetDoubleArg(env, args[1], &half_height) || !GetDoubleArg(env, args[2], &radius) ||
      !GetDoubleArg(env, args[3], &x) || !GetDoubleArg(env, args[4], &y) || !GetDoubleArg(env, args[5], &z) ||
      !GetDoubleArg(env, args[6], &mass) || !GetDoubleArg(env, args[7], &max_strength) ||
      !GetDoubleArg(env, args[8], &max_slope_angle)) {
    ThrowTypeError(env, "createCharacter: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateCharacter(
      static_cast<float>(half_height), static_cast<float>(radius),
      x, y, z,
      static_cast<float>(mass), static_cast<float>(max_strength),
      static_cast<float>(max_slope_angle));
  napi_value result;
  napi_create_uint32(env, id, &result);
  return result;
}

napi_value DestroyCharacter(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id = 0;
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "destroyCharacter: invalid id"); return nullptr; }
  id = static_cast<uint32_t>(d);
  bool ok = handle->world->DestroyCharacter(id);
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value CharacterUpdate(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  double d, dt;
  if (!GetDoubleArg(env, args[1], &d) || !GetDoubleArg(env, args[2], &dt)) {
    ThrowTypeError(env, "characterUpdate: invalid args"); return nullptr;
  }
  bool ok = handle->world->CharacterUpdate(static_cast<uint32_t>(d), static_cast<float>(dt));
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value SetCharacterLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  double d, vx, vy, vz;
  if (!GetDoubleArg(env, args[1], &d) || !GetDoubleArg(env, args[2], &vx) ||
      !GetDoubleArg(env, args[3], &vy) || !GetDoubleArg(env, args[4], &vz)) {
    ThrowTypeError(env, "setCharacterLinearVelocity: invalid args"); return nullptr;
  }
  bool ok = handle->world->SetCharacterLinearVelocity(
      static_cast<uint32_t>(d), static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz));
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value GetCharacterLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterLinearVelocity: invalid id"); return nullptr; }
  Vec3 vel;
  if (!handle->world->GetCharacterLinearVelocity(static_cast<uint32_t>(d), vel)) return nullptr;
  return MakeVec3Object(env, RVec3(vel.GetX(), vel.GetY(), vel.GetZ()));
}

napi_value SetCharacterPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  double d, x, y, z;
  if (!GetDoubleArg(env, args[1], &d) || !GetDoubleArg(env, args[2], &x) ||
      !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "setCharacterPosition: invalid args"); return nullptr;
  }
  bool ok = handle->world->SetCharacterPosition(static_cast<uint32_t>(d), x, y, z);
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value GetCharacterPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterPosition: invalid id"); return nullptr; }
  RVec3 pos;
  if (!handle->world->GetCharacterPosition(static_cast<uint32_t>(d), pos)) return nullptr;
  return MakeVec3Object(env, pos);
}

napi_value SetCharacterRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  double d, rx, ry, rz, rw;
  if (!GetDoubleArg(env, args[1], &d) || !GetDoubleArg(env, args[2], &rx) ||
      !GetDoubleArg(env, args[3], &ry) || !GetDoubleArg(env, args[4], &rz) ||
      !GetDoubleArg(env, args[5], &rw)) {
    ThrowTypeError(env, "setCharacterRotation: invalid args"); return nullptr;
  }
  bool ok = handle->world->SetCharacterRotation(
      static_cast<uint32_t>(d), static_cast<float>(rx), static_cast<float>(ry),
      static_cast<float>(rz), static_cast<float>(rw));
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value GetCharacterRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterRotation: invalid id"); return nullptr; }
  Quat rot;
  if (!handle->world->GetCharacterRotation(static_cast<uint32_t>(d), rot)) return nullptr;
  return MakeQuatObject(env, rot);
}

napi_value GetCharacterGroundState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterGroundState: invalid id"); return nullptr; }
  int state = handle->world->GetCharacterGroundState(static_cast<uint32_t>(d));
  napi_value result; napi_create_int32(env, state, &result);
  return result;
}

napi_value GetCharacterGroundNormal(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterGroundNormal: invalid id"); return nullptr; }
  Vec3 normal;
  if (!handle->world->GetCharacterGroundNormal(static_cast<uint32_t>(d), normal)) return nullptr;
  return MakeVec3Object(env, RVec3(normal.GetX(), normal.GetY(), normal.GetZ()));
}

napi_value GetCharacterGroundBodyId(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterGroundBodyId: invalid id"); return nullptr; }
  uint32_t body_id = handle->world->GetCharacterGroundBodyId(static_cast<uint32_t>(d));
  napi_value result; napi_create_uint32(env, body_id, &result);
  return result;
}

#ifdef JPH_DEBUG_RENDERER
napi_value GetDebugGeometry(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  bool draw_bodies, draw_constraints, draw_constraint_limits, wireframe;
  if (!GetBoolArg(env, args[1], &draw_bodies) || !GetBoolArg(env, args[2], &draw_constraints) ||
      !GetBoolArg(env, args[3], &draw_constraint_limits) || !GetBoolArg(env, args[4], &wireframe)) {
    ThrowTypeError(env, "getDebugGeometry: invalid args"); return nullptr;
  }

  PhysicsWorld::DebugGeoResult geo = handle->world->GetDebugGeometry(
      draw_bodies, draw_constraints, draw_constraint_limits, wireframe);

  // Build Float32Array for line positions
  napi_value line_pos_buf, tri_pos_buf;
  napi_value line_col_buf, tri_col_buf;
  void *data_ptr;
  size_t byte_len;

  // Lines: positions (Float32Array)
  byte_len = geo.linePos.size() * sizeof(float);
  napi_create_arraybuffer(env, byte_len, &data_ptr, &line_pos_buf);
  if (byte_len > 0) std::memcpy(data_ptr, geo.linePos.data(), byte_len);
  napi_value line_pos_arr;
  napi_create_typedarray(env, napi_float32_array, geo.linePos.size(), line_pos_buf, 0, &line_pos_arr);

  // Lines: colors (Uint32Array)
  byte_len = geo.lineCol.size() * sizeof(uint32_t);
  napi_create_arraybuffer(env, byte_len, &data_ptr, &line_col_buf);
  if (byte_len > 0) std::memcpy(data_ptr, geo.lineCol.data(), byte_len);
  napi_value line_col_arr;
  napi_create_typedarray(env, napi_uint32_array, geo.lineCol.size(), line_col_buf, 0, &line_col_arr);

  // Triangles: positions (Float32Array)
  byte_len = geo.triPos.size() * sizeof(float);
  napi_create_arraybuffer(env, byte_len, &data_ptr, &tri_pos_buf);
  if (byte_len > 0) std::memcpy(data_ptr, geo.triPos.data(), byte_len);
  napi_value tri_pos_arr;
  napi_create_typedarray(env, napi_float32_array, geo.triPos.size(), tri_pos_buf, 0, &tri_pos_arr);

  // Triangles: colors (Uint32Array)
  byte_len = geo.triCol.size() * sizeof(uint32_t);
  napi_create_arraybuffer(env, byte_len, &data_ptr, &tri_col_buf);
  if (byte_len > 0) std::memcpy(data_ptr, geo.triCol.data(), byte_len);
  napi_value tri_col_arr;
  napi_create_typedarray(env, napi_uint32_array, geo.triCol.size(), tri_col_buf, 0, &tri_col_arr);

  napi_value result;
  napi_create_object(env, &result);
  napi_set_named_property(env, result, "lines", line_pos_arr);
  napi_set_named_property(env, result, "lineColors", line_col_arr);
  napi_set_named_property(env, result, "triangles", tri_pos_arr);
  napi_set_named_property(env, result, "triangleColors", tri_col_arr);
  return result;
}
#endif

#undef WORLD_FN_BEGIN
*/

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
      {"createWorld", nullptr, CreateWorld, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"destroyWorld", nullptr, DestroyWorld, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"step", nullptr, StepWorld, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setGravity", nullptr, SetGravity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setBodyActivationCallback", nullptr, SetBodyActivationCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setContactCallback", nullptr, SetContactCallback, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"createSphere", nullptr, CreateSphere, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createBox", nullptr, CreateBox, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createCapsule", nullptr, CreateCapsule, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createCylinder", nullptr, CreateCylinder, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createTaperedCapsule", nullptr, CreateTaperedCapsule, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createTaperedCylinder", nullptr, CreateTaperedCylinder, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createConvexHull", nullptr, CreateConvexHull, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createMesh", nullptr, CreateMesh, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createHeightField", nullptr, CreateHeightField, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createStaticCompound", nullptr, CreateStaticCompound, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createMutableCompound", nullptr, CreateMutableCompound, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"addMutableSubShape", nullptr, AddMutableSubShape, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"removeMutableSubShape", nullptr, RemoveMutableSubShape, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"modifyMutableSubShape", nullptr, ModifyMutableSubShape, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"adjustMutableCenterOfMass", nullptr, AdjustMutableCenterOfMass, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"getBodyPosition", nullptr, GetBodyPosition, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getBodyRotation", nullptr, GetBodyRotation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setBodyPosition", nullptr, SetBodyPosition, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setBodyRotation", nullptr, SetBodyRotation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getLinearVelocity", nullptr, GetLinearVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setLinearVelocity", nullptr, SetLinearVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getAngularVelocity", nullptr, GetAngularVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setAngularVelocity", nullptr, SetAngularVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"applyImpulse", nullptr, ApplyImpulse, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"addAngularImpulse", nullptr, AddAngularImpulse, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"addForce", nullptr, AddForce, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"addTorque", nullptr, AddTorque, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setFriction", nullptr, SetFrictionValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getFriction", nullptr, GetFrictionValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setRestitution", nullptr, SetRestitutionValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getRestitution", nullptr, GetRestitutionValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setGravityFactor", nullptr, SetGravityFactorValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getGravityFactor", nullptr, GetGravityFactorValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setMotionType", nullptr, SetMotionTypeValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getMotionType", nullptr, GetMotionTypeValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setMotionQuality", nullptr, SetMotionQualityValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getMotionQuality", nullptr, GetMotionQualityValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setObjectLayer", nullptr, SetObjectLayerValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getObjectLayer", nullptr, GetObjectLayerValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setDamping", nullptr, SetDampingValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getDamping", nullptr, GetDampingValue, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"activateBody", nullptr, ActivateBody, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deactivateBody", nullptr, DeactivateBody, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"removeBody", nullptr, RemoveBody, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"hasBody", nullptr, HasBody, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"isBodyActive", nullptr, IsBodyActive, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setBodySensor", nullptr, SetBodySensor, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"isBodySensor", nullptr, IsBodySensor, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCenterOfMassPosition", nullptr, GetCenterOfMassPosition, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"rayCastClosest", nullptr, RayCastClosest, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"rayCastAll", nullptr, RayCastAll, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"collideSphereAll", nullptr, CollideSphereAll, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"castSphereAll", nullptr, CastSphereAll, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"castBoxAll", nullptr, CastBoxAll, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"castCapsuleAll", nullptr, CastCapsuleAll, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"queryAABB", nullptr, QueryAABB, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"areBodiesInContact", nullptr, AreBodiesInContact, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"createFixedConstraint", nullptr, CreateFixedConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createDistanceConstraint", nullptr, CreateDistanceConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createHingeConstraint", nullptr, CreateHingeConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createSliderConstraint", nullptr, CreateSliderConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createPointConstraint", nullptr, CreatePointConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createConeConstraint", nullptr, CreateConeConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createSwingTwistConstraint", nullptr, CreateSwingTwistConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createSixDOFConstraint", nullptr, CreateSixDOFConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"removeConstraint", nullptr, RemoveConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setHingeLimits", nullptr, SetHingeLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSliderLimits", nullptr, SetSliderLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setHingeMotor", nullptr, SetHingeMotor, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSliderMotor", nullptr, SetSliderMotor, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setConeHalfAngle", nullptr, SetConeHalfAngle, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSwingTwistLimits", nullptr, SetSwingTwistLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSwingTwistMotor", nullptr, SetSwingTwistMotor, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSixDOFLimits", nullptr, SetSixDOFLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSixDOFMotorState", nullptr, SetSixDOFMotorState, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSixDOFTargetVelocity", nullptr, SetSixDOFTargetVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSixDOFTargetPose", nullptr, SetSixDOFTargetPose, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setHingeMotorSpring", nullptr, SetHingeMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSliderMotorSpring", nullptr, SetSliderMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSwingMotorSpring", nullptr, SetSwingMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setTwistMotorSpring", nullptr, SetTwistMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setSixDOFMotorSpring", nullptr, SetSixDOFMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getHingeMotorSpring", nullptr, GetHingeMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSliderMotorSpring", nullptr, GetSliderMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSwingMotorSpring", nullptr, GetSwingMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getTwistMotorSpring", nullptr, GetTwistMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSixDOFMotorSpring", nullptr, GetSixDOFMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"getHingeAngle", nullptr, GetHingeAngle, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getHingeMotorState", nullptr, GetHingeMotorState, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSliderPosition", nullptr, GetSliderPosition, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSliderMotorState", nullptr, GetSliderMotorState, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSixDOFRotation", nullptr, GetSixDOFRotation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSixDOFLimits", nullptr, GetSixDOFLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSixDOFMotorState", nullptr, GetSixDOFMotorState, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSwingTwistRotation", nullptr, GetSwingTwistRotation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSwingTwistMotorState", nullptr, GetSwingTwistMotorState, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getHingeLambdas", nullptr, GetHingeLambdas, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSliderLambdas", nullptr, GetSliderLambdas, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSwingTwistLambdas", nullptr, GetSwingTwistLambdas, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSixDOFLambdas", nullptr, GetSixDOFLambdas, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getConeLambdas", nullptr, GetConeLambdas, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPointLambdas", nullptr, GetPointLambdas, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getFixedLambdas", nullptr, GetFixedLambdas, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getDistanceLambda", nullptr, GetDistanceLambda, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPulleyLambda", nullptr, GetPulleyLambda, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getGearLambda", nullptr, GetGearLambda, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPathLambdas", nullptr, GetPathLambdas, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getHingeLimits", nullptr, GetHingeLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSliderLimits", nullptr, GetSliderLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSwingTwistLimits", nullptr, GetSwingTwistLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createGearConstraint", nullptr, CreateGearConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createPulleyConstraint", nullptr, CreatePulleyConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPulleyLength", nullptr, GetPulleyLength, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setPulleyLength", nullptr, SetPulleyLength, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPulleyLengthLimits", nullptr, GetPulleyLengthLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createPathConstraint", nullptr, CreatePathConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPathFraction", nullptr, GetPathFraction, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPathMaxFraction", nullptr, GetPathMaxFraction, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setPathMotor", nullptr, SetPathMotor, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPathMotorState", nullptr, GetPathMotorState, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setPathMotorSpring", nullptr, SetPathMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPathMotorSpring", nullptr, GetPathMotorSpring, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"setDistanceLimits", nullptr, SetDistanceLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getDistanceLimits", nullptr, GetDistanceLimits, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setDistanceLimitsSpring", nullptr, SetDistanceLimitsSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getDistanceLimitsSpring", nullptr, GetDistanceLimitsSpring, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createRackAndPinionConstraint", nullptr, CreateRackAndPinionConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getRackAndPinionLambda", nullptr, GetRackAndPinionLambda, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"createSkeleton", nullptr, CreateSkeleton, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"addSkeletonJoint", nullptr, AddSkeletonJoint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"finalizeSkeleton", nullptr, FinalizeSkeleton, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSkeletonJointCount", nullptr, GetSkeletonJointCount, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSkeletonJointInfo", nullptr, GetSkeletonJointInfo, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSkeletonJointIndex", nullptr, GetSkeletonJointIndex, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createRagdollSettings", nullptr, CreateRagdollSettings, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"createRagdoll", nullptr, CreateRagdoll, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"destroyRagdoll", nullptr, DestroyRagdoll, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getRagdollBodyCount", nullptr, GetRagdollBodyCount, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getRagdollBoneBodyId", nullptr, GetRagdollBoneBodyId, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getRagdollBoneTransform", nullptr, GetRagdollBoneTransform, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setRagdollBoneTransform", nullptr, SetRagdollBoneTransform, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setRagdollJointShape", nullptr, SetRagdollJointShape, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setRagdollJointTransform", nullptr, SetRagdollJointTransform, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setRagdollJointConstraint", nullptr, SetRagdollJointConstraint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getRagdollConstraintIds", nullptr, GetRagdollConstraintIds, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"createSkeletonPose", nullptr, CreateSkeletonPose, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"destroySkeletonPose", nullptr, DestroySkeletonPose, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setPoseJoint", nullptr, SetPoseJoint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPoseJoint", nullptr, GetPoseJoint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setPoseRootOffset", nullptr, SetPoseRootOffset, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPoseRootOffset", nullptr, GetPoseRootOffset, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"calculatePoseJointMatrices", nullptr, CalculatePoseJointMatrices, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getPoseJointCount", nullptr, GetPoseJointCount, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"ragdollSetPose", nullptr, RagdollSetPose, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollGetPose", nullptr, RagdollGetPose, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollDriveToPoseKinematics", nullptr, RagdollDriveToPoseKinematics, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollDriveToPoseMotors", nullptr, RagdollDriveToPoseMotors, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollActivate", nullptr, RagdollActivate, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollIsActive", nullptr, RagdollIsActive, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollGetRootTransform", nullptr, RagdollGetRootTransform, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollGetWorldSpaceBounds", nullptr, RagdollGetWorldSpaceBounds, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollSetGroupID", nullptr, RagdollSetGroupID, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollResetWarmStart", nullptr, RagdollResetWarmStart, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollSetLinearVelocity", nullptr, RagdollSetLinearVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollAddLinearVelocity", nullptr, RagdollAddLinearVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollSetLinearAndAngularVelocity", nullptr, RagdollSetLinearAndAngularVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollAddImpulse", nullptr, RagdollAddImpulse, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollAddToPhysicsSystem", nullptr, RagdollAddToPhysicsSystem, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollRemoveFromPhysicsSystem", nullptr, RagdollRemoveFromPhysicsSystem, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"ragdollStabilize", nullptr, RagdollStabilize, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"snapshotState", nullptr, SnapshotState, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"applySnapshot", nullptr, ApplySnapshot, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"saveScene", nullptr, SaveScene, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"loadScene", nullptr, LoadScene, nullptr, nullptr, nullptr, napi_default, nullptr},

      {"createCharacter", nullptr, CreateCharacter, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"destroyCharacter", nullptr, DestroyCharacter, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"characterUpdate", nullptr, CharacterUpdate, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setCharacterLinearVelocity", nullptr, SetCharacterLinearVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCharacterLinearVelocity", nullptr, GetCharacterLinearVelocity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setCharacterPosition", nullptr, SetCharacterPosition, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCharacterPosition", nullptr, GetCharacterPosition, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setCharacterRotation", nullptr, SetCharacterRotation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCharacterRotation", nullptr, GetCharacterRotation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCharacterGroundState", nullptr, GetCharacterGroundState, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCharacterGroundNormal", nullptr, GetCharacterGroundNormal, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getCharacterGroundBodyId", nullptr, GetCharacterGroundBodyId, nullptr, nullptr, nullptr, napi_default, nullptr},
#ifdef JPH_DEBUG_RENDERER
      {"getDebugGeometry", nullptr, GetDebugGeometry, nullptr, nullptr, nullptr, napi_default, nullptr},
#endif
  };

  if (napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors) != napi_ok) {
    return nullptr;
  }

  // Force Jolt factory cleanup when the NAPI environment tears down (process exit,
  // Worker thread termination). At this point all GC finalizers have already run,
  // but just in case any world was leaked we forcibly reset state so that
  // UnregisterTypes / Factory deletion always happens exactly once.
  napi_add_env_cleanup_hook(env, [](void *) {
    std::lock_guard<std::mutex> guard(gInitMutex);
    if (!gJoltInitialized) return;
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    gJoltInitialized = false;
    gWorldCount.store(0);
  }, nullptr);

  return exports;
}

}  // namespace

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
