#pragma once

#include "commands.h"
#include "events.h"
#include "shapes.h"

namespace JOLT {

struct JsConvert {

  static std::string GetString(napi_env env, napi_value v);
  static bool GetBoolean(napi_env env, napi_value v);
  static bool GetBooleanProp(napi_env env, napi_value obj, const char * key, bool def);

  static float GetFloatProp(napi_env env, napi_value obj, const char* key, float def = 0.0);
  static uint8_t GetUInt8Prop(napi_env env, napi_value obj, const char* key, uint8_t def = 0);
  static int32_t GetInt32Prop(napi_env env, napi_value obj, const char* key, int32_t def = 0);
  static uint32_t GetUInt32Prop(napi_env env, napi_value obj, const char* key, uint32_t def = 0);
  static uint64_t GetUInt64Prop(napi_env env, napi_value obj, const char* key, uint64_t def = 0);
  static std::string GetStringProp(napi_env env, napi_value obj, const char* key, std::string def = "");

  static JPH::Vec3 GetVec3Prop(napi_env env, napi_value obj, const char * key);
  static JPH::Quat GetQuatProp(napi_env env, napi_value obj, const char * key);

  static napi_value SetRMat44(napi_env env, JPH::RMat44 v);
  static napi_value SetVec3(napi_env env, JPH::Vec3 v);
  static napi_value SetQuat(napi_env env, JPH::Quat v);

  static void SetRMat44Prop(napi_env env, napi_value obj, const char * key, JPH::RMat44 v);
  static void SetVec3Prop(napi_env env, napi_value obj, const char * key, JPH::Vec3 v);
  static void SetQuatProp(napi_env env, napi_value obj, const char * key, JPH::Quat v);

  static BodyCreationSettings GetCreateParamsProp(napi_env env, napi_value obj, const char * key);

  static Commands GetCommandType(napi_env env, napi_value v);
  static Commands GetCommandProp(napi_env env, napi_value obj, const char * key);

  static std::string eventType(Events e);
  static JCommand from(napi_env env, napi_value v);


  static JPH::EMotionType  GetMotionType(napi_env env, napi_value v);
  static napi_value SetMotionType(napi_env env, JPH::EMotionType v);
  static void SetMotionTypeProp(napi_env evn, napi_value obj, const char * key, JPH::EMotionType v);
  static JPH::EShapeType  GetShapeType(napi_env env, napi_value v);
  static JPH::EShapeSubType GetShapeSubType(napi_env env, napi_value v);
  static JPH::EMotionType  GetMotionTypeProp(napi_env env, napi_value obj, const char * key);
  static JPH::EShapeType  GetShapeTypeProp(napi_env env, napi_value obj, const char * key);
  static JPH::EShapeSubType GetShapeSubTypeProp(napi_env env, napi_value obj, const char * key);

  static std::string StrShapeType(JPH::EShapeType v);
  static std::string StrShapeSubType(JPH::EShapeSubType v);

  static napi_value SetShapeType(napi_env env, JPH::EShapeType v);
  static napi_value SetShapeSubType(napi_env env, JPH::EShapeSubType v);

  static void SetShapeTypeProp(napi_env env, napi_value obj, const char * key, JPH::EShapeType v);
  static void SetShapeSubTypeProp(napi_env env, napi_value obj, const char * key, JPH::EShapeSubType v);

  static BodyCreationSettings GetBodyCreationSettings(napi_env env, napi_value v);

  static napi_value SetBodyCreationSettings(napi_env env, const BodyCreationSettings &bcs);

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
