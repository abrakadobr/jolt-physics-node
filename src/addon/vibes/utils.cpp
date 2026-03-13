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

  void SetUInt32(napi_env mEnv, napi_value payload, const char *key, uint32_t value) const {
    napi_value v;
    napi_create_uint32(mEnv, value, &v);
    napi_set_named_property(mEnv, payload, key, v);
  }


