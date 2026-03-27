#include "js_convert.h"
#include "jolt_convert.h"

namespace JOLT {

napi_value JsConvert<void>::to(napi_env env) {
  napi_value v;
  napi_get_undefined(env, &v);
  return v;
}

/* bool */
bool JsConvert<bool>::from(napi_env env, napi_value v) {
  bool x;
  napi_get_value_bool(env, v, &x);
  return x;
}

napi_value JsConvert<bool>::to(napi_env env, bool v) {
  napi_value r;
  napi_get_boolean(env, v, &r);
  return r;
}

/* int32 */
int32_t JsConvert<int32_t>::from(napi_env env, napi_value v) {
  int32_t x;
  napi_get_value_int32(env, v, &x);
  return x;
}

napi_value JsConvert<int32_t>::to(napi_env env, int32_t v) {
  napi_value r;
  napi_create_int32(env, v, &r);
  return r;
}

/* uint32_t */
uint32_t JsConvert<uint32_t>::from(napi_env env, napi_value v) {
  uint32_t x;
  napi_get_value_uint32(env, v, &x);
  return x;
}

napi_value JsConvert<uint32_t>::to(napi_env env, uint32_t v) {
  napi_value r;
  napi_create_uint32(env, v, &r);
  return r;
}

/* double */
double JsConvert<double>::from(napi_env env, napi_value v) {
  double x;
  napi_get_value_double(env, v, &x);
  return x;
}

napi_value JsConvert<double>::to(napi_env env, double v) {
  napi_value r;
  napi_create_double(env, v, &r);
  return r;
}

/* float */
float JsConvert<float>::from(napi_env env, napi_value v) {
  double x;
  napi_get_value_double(env, v, &x);
  return static_cast<float>(x);
}

napi_value JsConvert<float>::to(napi_env env, float v) {
  napi_value r;
  napi_create_double(env, static_cast<double>(v), &r);
  return r;
}

/* vector<uint8_t> — Node.js Buffer */
std::vector<uint8_t> JsConvert<std::vector<uint8_t>>::from(napi_env env, napi_value v) {
  void* data;
  size_t len;
  napi_get_buffer_info(env, v, &data, &len);
  auto* p = static_cast<uint8_t*>(data);
  return std::vector<uint8_t>(p, p + len);
}

napi_value JsConvert<std::vector<uint8_t>>::to(napi_env env, const std::vector<uint8_t>& v) {
  napi_value buf;
  void* dst;
  napi_create_buffer_copy(env, v.size(), v.data(), &dst, &buf);
  return buf;
}

/* string */
std::string JsConvert<std::string>::from(napi_env env, napi_value v) {
  size_t len;
  napi_get_value_string_utf8(env, v, nullptr, 0, &len);
  std::string s;
  s.resize(len);
  napi_get_value_string_utf8(env, v, s.data(), len+1, &len);
  return s;
}

napi_value JsConvert<std::string>::to(napi_env env, const std::string& s) {
  napi_value r;
  napi_create_string_utf8(env, s.c_str(), s.size(), &r);
  return r;
}

/* BodyShapeType */
BodyShapeType JsConvert<BodyShapeType>::from(napi_env env, napi_value v) {
  size_t len;
  napi_get_value_string_utf8(env, v, nullptr, 0, &len);
  std::string s;
  s.resize(len);
  napi_get_value_string_utf8(env, v, s.data(), len+1, &len);
  if (s == "sphere") return BodyShapeType::Sphere;
  if (s == "box") return BodyShapeType::Box;
  if (s == "triangle") return BodyShapeType::Triangle;
  if (s == "capsule") return BodyShapeType::Capsule;
  if (s == "taperedCapsule") return BodyShapeType::TaperedCapsule;
  if (s == "cylinder") return BodyShapeType::Cylinder;
  if (s == "convexHull") return BodyShapeType::ConvexHull;

// Compound shapes
  if (s == "staticCompound") return BodyShapeType::StaticCompound;
  if (s == "mutableCompaund") return BodyShapeType::MutableCompound;

// Decorated shapes
  if (s == "rotatedTranslated") return BodyShapeType::RotatedTranslated;
  if (s == "scaled") return BodyShapeType::Scaled;
  if (s == "offsetCenterOfMass") return BodyShapeType::OffsetCenterOfMass;

// Other shapes
  if (s == "mesh") return BodyShapeType::Mesh;
  if (s == "heightField") return BodyShapeType::HeightField;
  if (s == "softBody") return BodyShapeType::SoftBody;

// User defined shapes
  if (s == "user1") return BodyShapeType::User1;
  if (s == "user2") return BodyShapeType::User2;
  if (s == "user3") return BodyShapeType::User3;
  if (s == "user4") return BodyShapeType::User4;
  if (s == "user5") return BodyShapeType::User5;
  if (s == "user6") return BodyShapeType::User6;
  if (s == "user7") return BodyShapeType::User7;
  if (s == "user8") return BodyShapeType::User8;

// User defined convex shapes
  if (s == "userConvex1") return BodyShapeType::UserConvex1;
  if (s == "userConvex2") return BodyShapeType::UserConvex2;
  if (s == "userConvex3") return BodyShapeType::UserConvex3;
  if (s == "userConvex4") return BodyShapeType::UserConvex4;
  if (s == "userConvex5") return BodyShapeType::UserConvex5;
  if (s == "userConvex6") return BodyShapeType::UserConvex6;
  if (s == "userConvex7") return BodyShapeType::UserConvex7;
  if (s == "userConvex8") return BodyShapeType::UserConvex8;

// Other shapes
  if (s == "plane") return BodyShapeType::Plane;
  if (s == "taperedCylinder") return BodyShapeType::TaperedCylinder;
  return BodyShapeType::Empty;
}

napi_value JsConvert<BodyShapeType>::to(napi_env env, const BodyShapeType& s) {
  napi_value r;
  std::string str = "";
  if (s == BodyShapeType::Sphere) str = "sphere";
  if (s == BodyShapeType::Box) str = "box";
  if (s == BodyShapeType::Triangle) str = "triangle";
  if (s == BodyShapeType::Capsule) str = "capsule";
  if (s == BodyShapeType::TaperedCapsule) str = "taperedCylinder";
  if (s == BodyShapeType::Cylinder) str = "cylinter";
  if (s == BodyShapeType::ConvexHull) str = "convexHull";

	// Compound shapes
  if (s == BodyShapeType::StaticCompound) str = "staticCompound";
  if (s == BodyShapeType::MutableCompound) str = "mutableCompaund";

	// Decorated shapes
  if (s == BodyShapeType::RotatedTranslated) str = "rotatedTranslated";
  if (s == BodyShapeType::Scaled) str = "scaled";
  if (s == BodyShapeType::OffsetCenterOfMass) str = "offsetCenterOfMass";

	// Other shapes
  if (s == BodyShapeType::Mesh) str = "mesh";
  if (s == BodyShapeType::HeightField) str = "heightField";
  if (s == BodyShapeType::SoftBody) str = "softBody";

	// User defined shapes
  if (s == BodyShapeType::User1) str = "user1";
  if (s == BodyShapeType::User2) str = "user2";
  if (s == BodyShapeType::User3) str = "user3";
  if (s == BodyShapeType::User4) str = "user4";
  if (s == BodyShapeType::User5) str = "user5";
  if (s == BodyShapeType::User6) str = "user6";
  if (s == BodyShapeType::User7) str = "user7";
  if (s == BodyShapeType::User8) str = "user8";

	// User defined convex shapes
  if (s == BodyShapeType::UserConvex1) str = "userConvex1";
  if (s == BodyShapeType::UserConvex2) str = "userConvex2";
  if (s == BodyShapeType::UserConvex3) str = "userConvex3";
  if (s == BodyShapeType::UserConvex4) str = "userConvex4";
  if (s == BodyShapeType::UserConvex5) str = "userConvex5";
  if (s == BodyShapeType::UserConvex6) str = "userConvex6";
  if (s == BodyShapeType::UserConvex7) str = "userConvex7";
  if (s == BodyShapeType::UserConvex8) str = "userConvex8";

	// Other shapes
  if (s == BodyShapeType::Plane) str = "Plane";
  if (s == BodyShapeType::TaperedCylinder) str = "taperedCylinder";
  if (s == BodyShapeType::Empty || str == "") str = "empty";
  napi_create_string_utf8(env, str.c_str(), str.size(), &r);
  return r;
}

/* BodyMotionType */
BodyMotionType JsConvert<BodyMotionType>::from(napi_env env, napi_value v) {
  size_t len;
  napi_get_value_string_utf8(env, v, nullptr, 0, &len);
  std::string s;
  s.resize(len);
  napi_get_value_string_utf8(env, v, s.data(), len+1, &len);
  if (s == "kinematic") return BodyMotionType::Kinematic;
  if (s == "dynamic") return BodyMotionType::Dynamic;
  return BodyMotionType::Static;
}

napi_value JsConvert<BodyMotionType>::to(napi_env env, const BodyMotionType& s) {
  napi_value r;
  std::string str = "";
  if (s == BodyMotionType::Kinematic) str = "kinematic";
  if (s == BodyMotionType::Dynamic) str = "dynamic";
  if (s == BodyMotionType::Static || str == "") str = "static";
  napi_create_string_utf8(env, str.c_str(), str.size(), &r);
  return r;
}

/* WorldState */
WorldState JsConvert<WorldState>::from(napi_env env, napi_value v) {
  size_t len;
  napi_get_value_string_utf8(env, v, nullptr, 0, &len);
  std::string s;
  s.resize(len);
  napi_get_value_string_utf8(env, v, s.data(), len+1, &len);
  if (s == "run") return WorldState::Run;
  if (s == "step") return WorldState::Step;
  return WorldState::Stop;
}

napi_value JsConvert<WorldState>::to(napi_env env, const WorldState& s) {
  napi_value r;
  std::string str = "";
  if (s == WorldState::Run) str = "run";
  if (s == WorldState::Step) str = "step";
  if (s == WorldState::Stop || str == "") str = "stop";
  napi_create_string_utf8(env, str.c_str(), str.size(), &r);
  return r;
}

WorldSettings JsConvert<WorldSettings>::from(napi_env env, napi_value v) {
  WorldSettings r;
  bool has = false;
  napi_value vv;
  if (napi_has_named_property(env, v, "gravity", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "gravity", &vv);
    r.gravity = JsConvert<double>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "memoryPreallocatedMb", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "memoryPreallocatedMb", &vv);
    r.memoryPreallocatedMb = JsConvert<uint32_t>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "maxBodies", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "maxBodies", &vv);
    r.maxBodies = JsConvert<uint32_t>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "numBodyMutexes", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "numBodyMutexes", &vv);
    r.numBodyMutexes = JsConvert<uint32_t>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "maxBodiesPairs", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "maxBodiesPairs", &vv);
    r.maxBodiesPairs = JsConvert<uint32_t>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "maxContacts", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "maxContacts", &vv);
    r.maxContacts = JsConvert<uint32_t>::from(env, vv);
  }
  return r;
}

napi_value JsConvert<WorldSettings>::to(napi_env env, const WorldSettings &v) {
  napi_value r;
  napi_create_object(env, &r);
  napi_set_named_property(env, r, "gravity", JsConvert<double>::to(env, v.gravity));
  napi_set_named_property(env, r, "memoryPreallocatedMb", JsConvert<uint32_t>::to(env, v.memoryPreallocatedMb));
  napi_set_named_property(env, r, "maxBodies", JsConvert<uint32_t>::to(env, v.maxBodies));
  napi_set_named_property(env, r, "numBodyMutexes", JsConvert<uint32_t>::to(env, v.numBodyMutexes));
  napi_set_named_property(env, r, "maxBodiesPairs", JsConvert<uint32_t>::to(env, v.maxBodiesPairs));
  napi_set_named_property(env, r, "maxContacts", JsConvert<uint32_t>::to(env, v.maxContacts));
  return r;
}

napi_value JsConvert<EventBodyActivation>::to(napi_env env, const EventBodyActivation &v) {
  napi_value r;
  napi_create_object(env, &r);
  napi_set_named_property(env, r, "body", JsConvert<JPH::BodyID>::to(env, v.body));
  napi_set_named_property(env, r, "active", JsConvert<bool>::to(env, v.active));
  return r;
}


/*  PhysicsMaterial */
PhysicsMaterial JsConvert<PhysicsMaterial>::from(napi_env env, napi_value v) {
  PhysicsMaterial r;
  bool has = false;
  napi_value vv;
  if (napi_has_named_property(env, v, "name", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "name", &vv);
    r.name = JsConvert<std::string>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "friction", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "friction", &vv);
    r.friction = JsConvert<float>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "restitution", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "restitution", &vv);
    r.restitution = JsConvert<float>::from(env, vv);
  }
  return r;
}

napi_value JsConvert<PhysicsMaterial>::to(napi_env env, const PhysicsMaterial &v) {
  napi_value r;
  napi_create_object(env, &r);
  napi_set_named_property(env, r, "name", JsConvert<std::string>::to(env, v.name));
  napi_set_named_property(env, r, "friction",    JsConvert<float>::to(env, v.friction));
  napi_set_named_property(env, r, "restitution", JsConvert<float>::to(env, v.restitution));
  return r;
}

static void AbstractShapeFromJs(napi_env env, napi_value v, AbstractShape &r) {
  bool has = false;
  napi_value vv;
  if (napi_has_named_property(env, v, "type", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "type", &vv);
    r.type = JsConvert<BodyShapeType>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "subType", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "subType", &vv);
    r.subType = JsConvert<BodyShapeType>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "userData", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "userData", &vv);
    double d = 0;
    napi_get_value_double(env, vv, &d);
    r.userData = static_cast<uint64_t>(d);
  }
  if (napi_has_named_property(env, v, "material", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "material", &vv);
    r.material = JsConvert<PhysicsMaterial>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "convexRadius", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "convexRadius", &vv);
    r.convexRadius = JsConvert<float>::from(env, vv);
  }
}

static void AbstractShapeToJs(napi_env env, napi_value r, const AbstractShape &v) {
  napi_set_named_property(env, r, "type", JsConvert<BodyShapeType>::to(env, v.type));
  napi_set_named_property(env, r, "subType", JsConvert<BodyShapeType>::to(env, v.subType));
  napi_value ud;
  napi_create_double(env, static_cast<double>(v.userData), &ud);
  napi_set_named_property(env, r, "userData", ud);
  napi_set_named_property(env, r, "material", JsConvert<PhysicsMaterial>::to(env, v.material));
  napi_set_named_property(env, r, "convexRadius", JsConvert<float>::to(env, v.convexRadius));
}

/*  BoxShape */
BoxShape JsConvert<BoxShape>::from(napi_env env, napi_value v) {
  BoxShape r;
  AbstractShapeFromJs(env, v, r);
  bool has = false;
  napi_value vv;
  if (napi_has_named_property(env, v, "halfExtent", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "halfExtent", &vv);
    r.halfExtent = JsConvert<JPH::Vec3>::from(env, vv);
  }
  return r;
}

napi_value JsConvert<BoxShape>::to(napi_env env, const BoxShape &v) {
  napi_value r;
  napi_create_object(env, &r);
  AbstractShapeToJs(env, r, v);
  napi_set_named_property(env, r, "halfExtent", JsConvert<JPH::Vec3>::to(env, v.halfExtent));
  return r;
}

/*  SphereShape */
SphereShape JsConvert<SphereShape>::from(napi_env env, napi_value v) {
  SphereShape r;
  AbstractShapeFromJs(env, v, r);
  bool has = false;
  napi_value vv;
  if (napi_has_named_property(env, v, "radius", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "radius", &vv);
    r.radius = JsConvert<float>::from(env, vv);
  }
  return r;
}

napi_value JsConvert<SphereShape>::to(napi_env env, const SphereShape &v) {
  napi_value r;
  napi_create_object(env, &r);
  AbstractShapeToJs(env, r, v);
  napi_set_named_property(env, r, "radius", JsConvert<float>::to(env, v.radius));
  return r;
}

/*  CapsuleShape */
CapsuleShape JsConvert<CapsuleShape>::from(napi_env env, napi_value v) {
  CapsuleShape r;
  AbstractShapeFromJs(env, v, r);
  bool has = false;
  napi_value vv;
  if (napi_has_named_property(env, v, "halfHeight", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "halfHeight", &vv);
    r.inHalfHeight = JsConvert<float>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "radius", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "radius", &vv);
    r.inRadius = JsConvert<float>::from(env, vv);
  }
  return r;
}

napi_value JsConvert<CapsuleShape>::to(napi_env env, const CapsuleShape &v) {
  napi_value r;
  napi_create_object(env, &r);
  AbstractShapeToJs(env, r, v);
  napi_set_named_property(env, r, "halfHeight", JsConvert<float>::to(env, v.inHalfHeight));
  napi_set_named_property(env, r, "radius", JsConvert<float>::to(env, v.inRadius));
  return r;
}

/*  TriangleShape */
TriangleShape JsConvert<TriangleShape>::from(napi_env env, napi_value v) {
  TriangleShape r;
  AbstractShapeFromJs(env, v, r);
  bool has = false;
  napi_value vv;
  if (napi_has_named_property(env, v, "p1", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "p1", &vv);
    r.p1 = JsConvert<JPH::Vec3>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "p2", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "p2", &vv);
    r.p2 = JsConvert<JPH::Vec3>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "p3", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "p3", &vv);
    r.p3 = JsConvert<JPH::Vec3>::from(env, vv);
  }
  return r;
}

napi_value JsConvert<TriangleShape>::to(napi_env env, const TriangleShape &v) {
  napi_value r;
  napi_create_object(env, &r);
  AbstractShapeToJs(env, r, v);
  napi_set_named_property(env, r, "p1", JsConvert<JPH::Vec3>::to(env, v.p1));
  napi_set_named_property(env, r, "p2", JsConvert<JPH::Vec3>::to(env, v.p2));
  napi_set_named_property(env, r, "p3", JsConvert<JPH::Vec3>::to(env, v.p3));
  return r;
}

/*  BodyCreationSettings */
BodyCreationSettings JsConvert<BodyCreationSettings>::from(napi_env env, napi_value v) {
  BodyCreationSettings r;
  bool has = false;
  napi_value vv;
  if (napi_has_named_property(env, v, "position", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "position", &vv);
    r.position = JsConvert<JPH::Vec3>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "rotation", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "rotation", &vv);
    r.rotation = JsConvert<JPH::Quat>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "active", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "active", &vv);
    r.active = JsConvert<bool>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "motionType", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "motionType", &vv);
    r.motionType = JsConvert<BodyMotionType>::from(env, vv);
  }
  if (napi_has_named_property(env, v, "layer", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "layer", &vv);
    r.layer = JsConvert<std::string>::from(env, vv);
  }
  return r;
}

napi_value JsConvert<BodyCreationSettings>::to(napi_env env, const BodyCreationSettings &v) {
  napi_value r;
  napi_create_object(env, &r);
  napi_set_named_property(env, r, "position", JsConvert<JPH::Vec3>::to(env, v.position));
  napi_set_named_property(env, r, "rotation", JsConvert<JPH::Quat>::to(env, v.rotation));
  napi_set_named_property(env, r, "active", JsConvert<bool>::to(env, v.active));
  napi_set_named_property(env, r, "motionType", JsConvert<BodyMotionType>::to(env, v.motionType));
  napi_set_named_property(env, r, "layer", JsConvert<std::string>::to(env, v.layer));
  return r;
}

}
