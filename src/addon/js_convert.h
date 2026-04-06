#pragma once

#include "commands.h"
#include "events.h"

namespace JOLT {

struct JsConvert {

  static std::string GetString(napi_env env, napi_value v);

  static float GetFloatProp(napi_env env, napi_value obj, const char* key, float def = 0.0);
  static uint8_t GetUInt8Prop(napi_env env, napi_value obj, const char* key, uint8_t def = 0);
  static int32_t GetInt32Prop(napi_env env, napi_value obj, const char* key, int32_t def = 0);
  static uint32_t GetUInt32Prop(napi_env env, napi_value obj, const char* key, uint32_t def = 0);
  static uint64_t GetUInt64Prop(napi_env env, napi_value obj, const char* key, uint64_t def = 0);
  static std::string GetStringProp(napi_env env, napi_value obj, const char* key, std::string def = "");

  static JPH::Vec3 GetVec3Prop(napi_env env, napi_value obj, const char * key);
  static JPH::Quat GetQuatProp(napi_env env, napi_value obj, const char * key);

  static BodyCreationParams GetCreateParamsProp(napi_env env, napi_value obj, const char * key);

  static Commands GetCommandType(napi_env env, napi_value v);
  static Commands GetCommandProp(napi_env env, napi_value obj, const char * key);

  static std::string eventType(Events e);
  static JCommand from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const JEvent &je, bool * shutdown);

  static napi_value SetBoolean(napi_env env, const bool v);
  static napi_value SetString(napi_env env, const std::string &v);
  static napi_value SetUint8(napi_env env, const uint8_t v);
  static napi_value SetUint32(napi_env env, const uint32_t v);
  static napi_value SetInt32(napi_env env, const int32_t v);
  static napi_value SetUint64(napi_env env, const uint64_t v);
  static napi_value SetFloat(napi_env env, const float v);

  static void SetBooleanProp(napi_env env, napi_value obj, const char * prop, const bool v);
  static void SetStringProp(napi_env env, napi_value obj, const char * prop, const std::string &v);
  static void SetUint8Prop(napi_env env, napi_value obj, const char * prop, const uint8_t v);
  static void SetUint32Prop(napi_env env, napi_value obj, const char * prop, const uint32_t v);
  static void SetInt32Prop(napi_env env, napi_value obj, const char * prop, const int32_t v);
  static void SetUint64Prop(napi_env env, napi_value obj, const char * prop, const uint64_t v);
  static void SetFloatProp(napi_env env, napi_value obj, const char * prop, const float v);

};

}
