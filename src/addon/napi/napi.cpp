#include "napi.h"

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
/* string */

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
  if (napi_has_named_property(env, v, "numBodyMutexts", &has) == napi_ok && has) {
    napi_get_named_property(env, v, "numBodyMutexts", &vv);
    r.numBodyMutexts = JsConvert<uint32_t>::from(env, vv);
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
  napi_set_named_property(env, r, "numBodyMutexts", JsConvert<uint32_t>::to(env, v.numBodyMutexts));
  napi_set_named_property(env, r, "maxBodiesPairs", JsConvert<uint32_t>::to(env, v.maxBodiesPairs));
  napi_set_named_property(env, r, "maxContacts", JsConvert<uint32_t>::to(env, v.maxContacts));
  return r;
}


}
