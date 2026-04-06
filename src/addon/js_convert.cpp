#include "js_convert.h"
#include <iostream>

namespace JOLT {

  std::string JsConvert::GetString(napi_env env, napi_value v) {
    size_t len;
    napi_get_value_string_utf8(env, v, nullptr, 0, &len);
    std::string s;
    s.resize(len);
    napi_get_value_string_utf8(env, v, s.data(), len+1, &len);
    return s;
  }

  float JsConvert::GetFloatProp(napi_env env, napi_value obj, const char* key, float def) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return def;
    napi_value v;
    napi_get_named_property(env, obj, key, &v);
    double d = 0.0;
    napi_get_value_double(env, v, &d);
    return static_cast<float>(d);
  }

  uint8_t JsConvert::GetUInt8Prop(napi_env env, napi_value obj, const char* key, uint8_t def) {
    uint32_t i32 = GetUInt32Prop(env, obj, key, def);
    if (i32 > 255) return def;
    return (uint8_t)i32;
  }

  int32_t JsConvert::GetInt32Prop(napi_env env, napi_value obj, const char* key, int32_t def) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return def;
    napi_value v;
    napi_get_named_property(env, obj, key, &v);
    int32_t i = 0;
    napi_get_value_int32(env, v, &i);
    return i;
  }

  uint32_t JsConvert::GetUInt32Prop(napi_env env, napi_value obj, const char* key, uint32_t def) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return def;
    napi_value v;
    napi_get_named_property(env, obj, key, &v);
    uint32_t i = 0;
    napi_get_value_uint32(env, v, &i);
    return i;
  }

  uint64_t JsConvert::GetUInt64Prop(napi_env env, napi_value obj, const char* key, uint64_t def) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return def;
    napi_value v;
    napi_get_named_property(env, obj, key, &v);
    uint64_t i = 0;
    bool ok = false;
    napi_get_value_bigint_uint64(env, v, &i, &ok);
    return ok ? i : def;
  }

  std::string JsConvert::GetStringProp(napi_env env, napi_value obj, const char* key, std::string def) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return def;
    napi_value v;
    napi_get_named_property(env, obj, key, &v);
    return GetString(env, v);
  }

  JPH::Vec3 JsConvert::GetVec3Prop(napi_env env, napi_value obj, const char * key) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return JPH::Vec3::sZero();
    napi_value vec;
    napi_get_named_property(env, obj, key, &vec);
    JPH::Vec3 ret;
    ret.SetX( GetFloatProp(env, vec, "x") );
    ret.SetY( GetFloatProp(env, vec, "y") );
    ret.SetZ( GetFloatProp(env, vec, "z") );
    return ret;
  }

  JPH::Quat JsConvert::GetQuatProp(napi_env env, napi_value obj, const char * key) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return JPH::Quat::sIdentity();
    napi_value q;
    napi_get_named_property(env, obj, key, &q);
    JPH::Quat ret;
    ret.SetX( GetFloatProp(env, q, "x") );
    ret.SetY( GetFloatProp(env, q, "y") );
    ret.SetZ( GetFloatProp(env, q, "z") );
    ret.SetW( GetFloatProp(env, q, "w") );
    return ret;
  }


  BodyCreationParams JsConvert::GetCreateParamsProp(napi_env env, napi_value obj, const char * key) {
    BodyCreationParams ret;
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return ret;
    napi_value props;
    napi_get_named_property(env, obj, key, &props);
    ret.position = GetVec3Prop(env, props, "position");
    ret.rotation = GetQuatProp(env, props, "rotation");
    ret.motionType = static_cast<JPH::EMotionType>(GetUInt8Prop(env, props, "motionType"));
    ret.layer = static_cast<JPH::ObjectLayer>(GetUInt32Prop(env, props, "layer"));
    return ret;
  }

  Commands JsConvert::GetCommandType(napi_env env, napi_value v) {
    std::string str = GetString(env, v);
    if (str == "init") return Commands::Init; 
    if (str == "start") return Commands::Start; 
    if (str == "stop") return Commands::Stop; 
    if (str == "step") return Commands::Step; 
    if (str == "shutdown") return Commands::Shutdown; 
    if (str == "createBox") return Commands::CreateBox; 
    if (str == "createSphere") return Commands::CreateSphere; 
    if (str == "setPosition") return Commands::SetPosition; 
    if (str == "setRotation") return Commands::SetRotation; 
    if (str == "addBody") return Commands::AddBody; 
    if (str == "removeBody") return Commands::RemoveBody; 
    if (str == "activateBody") return Commands::ActivateBody; 
    if (str == "deactivateBody") return Commands::DeactivateBody; 
    if (str == "destroyBody") return Commands::DestroyBody; 
    return Commands::Invalid;
  }

  Commands JsConvert::GetCommandProp(napi_env env, napi_value obj, const char * key) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return Commands::Invalid;
    napi_value v;
    napi_get_named_property(env, obj, key, &v);
    return GetCommandType(env, v);
  }

  JCommand JsConvert::from(napi_env env, napi_value v) {
    Commands cmde = GetCommandProp(env, v, "cmd" );
    uint32_t cmd_int = static_cast<uint32_t>(cmde);
    if (cmd_int >= static_cast<uint32_t>(Commands::COUNT)) {
      return CommandBase::Invalid();
    }
    CommandBase ret;
    ret.commandId = GetUInt64Prop(env, v, "commandId", 0);
    ret.cmd = cmde;
    WorldConfig cfg;
    CommandInit cInit;
    CommandBody cBody;
    CommandCreateBox cCBox;
    CommandCreateSphere cCSph;
    CommandSetPosition cSPos;
    CommandSetRotation cSRot;
    switch (ret.cmd) {
      case Commands::Init:
        napi_value vcfg;
        cInit.commandId = ret.commandId;
        cInit.cmd = ret.cmd;
        if (napi_get_named_property(env, v, "config", &vcfg) == napi_ok) {
          cInit.config.gravity = GetFloatProp(env, vcfg, "gravity", cfg.gravity);
          cInit.config.memoryPreallocatedMb = GetUInt32Prop(env, vcfg, "memoryPreallocatedMb", cfg.memoryPreallocatedMb);
          cInit.config.maxBodies = GetUInt32Prop(env, vcfg, "maxBodies", cfg.maxBodies);
          cInit.config.numBodyMutexes = GetUInt32Prop(env, vcfg, "numBodyMutexes", cfg.numBodyMutexes);
          cInit.config.maxBodiesPairs = GetUInt32Prop(env, vcfg, "maxBodiesPairs", cfg.maxBodiesPairs);
          cInit.config.maxContacts = GetUInt32Prop(env, vcfg, "maxContacts", cfg.maxContacts);
          return cInit;
        }
        return CommandBase::Invalid();
      case Commands::Start:
      case Commands::Stop:
      case Commands::Step:
      case Commands::Shutdown:
        return ret;
      case Commands::AddBody:
      case Commands::RemoveBody:
      case Commands::ActivateBody:
      case Commands::DeactivateBody:
      case Commands::DestroyBody:
        cBody.commandId = ret.commandId;
        cBody.cmd = ret.cmd;
        cBody.bodyId = GetUInt32Prop(env, v, "bodyId");
        return cBody;
      case Commands::CreateBox:
        cCBox.commandId = ret.commandId;
        cCBox.cmd = ret.cmd;
        cCBox.half = GetVec3Prop(env, v, "half");
        cCBox.params = GetCreateParamsProp(env, v, "params");
        return cCBox;
      case Commands::CreateSphere:
        cCSph.commandId = ret.commandId;
        cCSph.cmd = ret.cmd;
        cCSph.radius = GetFloatProp(env, v, "radius");
        cCSph.params = GetCreateParamsProp(env, v, "params");
        return cCSph;
      case Commands::SetPosition:
        cSPos.commandId = ret.commandId;
        cSPos.cmd = ret.cmd;
        cSPos.bodyId = GetUInt32Prop(env, v, "bodyId");
        cSPos.position = GetVec3Prop(env, v, "position");
        return cSPos;
      case Commands::SetRotation:
        cSRot.commandId = ret.commandId;
        cSRot.cmd = ret.cmd;
        cSRot.bodyId = GetUInt32Prop(env, v, "bodyId");
        cSRot.rotation = GetQuatProp(env, v, "rotation");
        return cSRot;
      default:
        return CommandBase::Invalid();
    }
  }

  std::string JsConvert::eventType(Events e) {
    if (e == Events::Init) return "init";
    if (e == Events::Start) return "start";
    if (e == Events::Stop) return "stop";
    if (e == Events::Step) return "step";
    if (e == Events::Shutdown) return "shutdown";
    if (e == Events::BodyCreated) return "bodyCreated";
    if (e == Events::BodyDestroyed) return "bodyDestroyed";
    if (e == Events::BodyAdded) return "bodyAdded";
    if (e == Events::BodyRemoved) return "bodyRemoved";
    if (e == Events::BodyActivated) return "bodyActivated";
    if (e == Events::BodyDeactivated) return "bodyDeactivated";
    if (e == Events::BodyTransform) return "bodyTransform";
    if (e == Events::Error) return "error";
    return "invalid";
  }

  napi_value JsConvert::to(napi_env env, const JEvent &je, bool * shutdown) {
    napi_value ret;
    napi_create_object(env, &ret);
    *shutdown = false;
    std::visit([env, ret, shutdown](auto&& e) {
      using T = std::decay_t<decltype(e)>;

      if constexpr (std::is_same_v<T, EventBase>) {
        std::string type = eventType(e.type);
        SetStringProp(env, ret, "type", type);
        SetUint64Prop(env, ret, "commandId", e.commandId);
        return ret;
      } else if constexpr (std::is_same_v<T, SuccessEvent>) {
        std::string type = eventType(e.type);
        SetStringProp(env, ret, "type", type);
        SetUint64Prop(env, ret, "commandId", e.commandId);
        SetBooleanProp(env, ret, "success", e.success);
        if (e.type == Events::Shutdown)
          *shutdown = true;
        return ret;
      } else if constexpr (std::is_same_v<T, BodyEvent>) {
        std::string type = eventType(e.type);
        SetStringProp(env, ret, "type", type);
        SetUint64Prop(env, ret, "commandId", e.commandId);
        SetBooleanProp(env, ret, "success", e.success);
        SetUint32Prop(env, ret, "bodyId", e.bodyId);
        return ret;
      } else if constexpr (std::is_same_v<T, BodyCreationEvent>) {
        std::string type = eventType(e.type);
        SetStringProp(env, ret, "type", type);
        SetUint64Prop(env, ret, "commandId", e.commandId);
        SetBooleanProp(env, ret, "success", e.success);
        SetUint32Prop(env, ret, "bodyId", e.bodyId);
        return ret;
      }
    }, je);

    return ret;
  }


napi_value JsConvert::SetBoolean(napi_env env, const bool v) {
  napi_value r;
  napi_get_boolean(env, v, &r);
  return r;
}
napi_value JsConvert::SetString(napi_env env, const std::string &v) {
  napi_value r;
  napi_create_string_utf8(env, v.c_str(), v.size(), &r);
  return r;
}
napi_value JsConvert::SetUint8(napi_env env, const uint8_t v) {
  napi_value r;
  napi_create_uint32(env, static_cast<uint32_t>(v), &r);
  return r;
}
napi_value JsConvert::SetUint32(napi_env env, const uint32_t v) {
  napi_value r;
  napi_create_uint32(env, v, &r);
  return r;
}
napi_value JsConvert::SetInt32(napi_env env, const int32_t v) {
  napi_value r;
  napi_create_int32(env, v, &r);
  return r;
}
napi_value JsConvert::SetUint64(napi_env env, const uint64_t v) {
  napi_value r;
  napi_create_bigint_uint64(env, v, &r);
  return r;
}
napi_value JsConvert::SetFloat(napi_env env, const float v) {
  napi_value r;
  napi_create_double(env, static_cast<double>(v), &r);
  return r;
}


void JsConvert::SetBooleanProp(napi_env env, napi_value obj, const char * prop, const bool v) {
  napi_value nv = SetBoolean(env, v);
  napi_set_named_property(env, obj, prop, nv);
}
void JsConvert::SetStringProp(napi_env env, napi_value obj, const char * prop, const std::string &v) {
  napi_value nv = SetString(env, v);
  napi_set_named_property(env, obj, prop, nv);
}
void JsConvert::SetUint8Prop(napi_env env, napi_value obj, const char * prop, const uint8_t v) {
  napi_value nv = SetUint8(env, v);
  napi_set_named_property(env, obj, prop, nv);
}
void JsConvert::SetUint32Prop(napi_env env, napi_value obj, const char * prop, const uint32_t v) {
  napi_value nv = SetUint32(env, v);
  napi_set_named_property(env, obj, prop, nv);
}
void JsConvert::SetInt32Prop(napi_env env, napi_value obj, const char * prop, const int32_t v) {
  napi_value nv = SetInt32(env, v);
  napi_set_named_property(env, obj, prop, nv);

}
void JsConvert::SetUint64Prop(napi_env env, napi_value obj, const char * prop, const uint64_t v) {
  napi_value nv = SetUint64(env, v);
  napi_set_named_property(env, obj, prop, nv);
}
void JsConvert::SetFloatProp(napi_env env, napi_value obj, const char * prop, const float v) {
  napi_value nv = SetFloat(env, v);
  napi_set_named_property(env, obj, prop, nv);
}

/* vector<uint8_t> — Node.js Buffer */
/*
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
*/

}
