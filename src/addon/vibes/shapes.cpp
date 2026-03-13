
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


