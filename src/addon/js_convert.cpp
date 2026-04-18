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

  bool JsConvert::GetBoolean(napi_env env, napi_value v) {
    bool ret;
    napi_get_value_bool(env, v, &ret);
    return ret;
  }
  bool JsConvert::GetBooleanProp(napi_env env, napi_value obj, const char * key, bool def = false) {
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) return def;
    napi_value v;
    napi_get_named_property(env, obj, key, &v);
    return GetBoolean(env, v);
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


  napi_value JsConvert::SetRMat44(napi_env env, JPH::RMat44 v) {
    napi_value arr;
    napi_create_array_with_length(env, 16, &arr);

    for (uint32_t col = 0; col < 4; ++col) {
        for (uint32_t row = 0; row < 4; ++row) {
            uint32_t i = col * 4 + row; // column-major индекс
            napi_value elem;
            napi_create_double(env, v(row, col), &elem);
            napi_set_element(env, arr, i, elem);
        }
    }
    return arr;
  }
  napi_value JsConvert::SetVec3(napi_env env, JPH::Vec3 v) {
    napi_value ret;
    napi_create_object(env, &ret);
    SetFloatProp(env, ret, "x", v.GetX());
    SetFloatProp(env, ret, "y", v.GetY());
    SetFloatProp(env, ret, "z", v.GetZ());
    return ret;
  }
  napi_value JsConvert::SetQuat(napi_env env, JPH::Quat v) {
    napi_value ret;
    napi_create_object(env, &ret);
    SetFloatProp(env, ret, "x", v.GetX());
    SetFloatProp(env, ret, "y", v.GetY());
    SetFloatProp(env, ret, "z", v.GetZ());
    SetFloatProp(env, ret, "w", v.GetW());
    return ret;
  }


  void JsConvert::SetRMat44Prop(napi_env env, napi_value obj, const char * key, JPH::RMat44 v) {
    napi_value nv = SetRMat44(env, v);
    napi_set_named_property(env, obj, key, nv);
  }
  void JsConvert::SetVec3Prop(napi_env env, napi_value obj, const char * key, JPH::Vec3 v) {
    napi_value nv = SetVec3(env, v);
    napi_set_named_property(env, obj, key, nv);
  }
  void JsConvert::SetQuatProp(napi_env env, napi_value obj, const char * key, JPH::Quat v) {
    napi_value nv = SetQuat(env, v);
    napi_set_named_property(env, obj, key, nv);
  }


  BodyCreationSettings JsConvert::GetCreateParamsProp(napi_env env, napi_value obj, const char * key) {
    BodyCreationSettings ret;
    bool exists = false;
    napi_has_named_property(env, obj, key, &exists);
    if (!exists) {
      // std::cout << "create params not exists!" << std::endl;
      return ret;
    }
    napi_value props;
    napi_get_named_property(env, obj, key, &props);
    ret.position = GetVec3Prop(env, props, "position");
    // std::cout << "create params parsing position" << ret.position.GetX() << "/" << ret.position.GetY() << "/" << ret.position.GetZ() << std::endl;
    ret.rotation = GetQuatProp(env, props, "rotation");
    ret.motionType = static_cast<JPH::EMotionType>(GetUInt8Prop(env, props, "motionType"));
    ret.layer = static_cast<JPH::ObjectLayer>(GetUInt32Prop(env, props, "layer"));
    ret.addToPhysics = GetBooleanProp(env, props, "addToPhysics");
    ret.activate = GetBooleanProp(env, props, "activate");
    return ret;
  }

  Commands JsConvert::GetCommandType(napi_env env, napi_value v) {
    std::string str = GetString(env, v);
    // std::cout << "command type: " << str << std::endl;
    // ----------       world
    if (str == "init") return Commands::Init; 
    if (str == "start") return Commands::Start; 
    if (str == "stop") return Commands::Stop; 
    if (str == "step") return Commands::Step; 
    if (str == "shutdown") return Commands::Shutdown; 
    // ----------       layers
    if (str == "getLayers") return Commands::GetLayers; 
    if (str == "createLayer") return Commands::CreateLayer; 
    if (str == "removeLayer") return Commands::RemoveLayer; 
    if (str == "rebindLayer") return Commands::RebindLayer; 
    if (str == "modifyLayerCollision") return Commands::ModifyLayerCollision; 
    // ----------       body
    if (str == "createBody") return Commands::CreateBody; 
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
    // std::cout << "JsConvert::from" << cmd_int << std::endl;
    if (cmd_int >= static_cast<uint32_t>(Commands::COUNT)) {
      return CommandBase::Invalid();
    }
    CommandBase ret;
    ret.commandId = GetUInt64Prop(env, v, "commandId", 0);
    ret.cmd = cmde;
    WorldConfig cfg;
    CommandInit cInit;
    CommandBody cBody;
    CommandBodyAdd cABody;
    CommandBodyDestroy cDBody;
    CommandBodyCreate cCBody;
    CommandSetPosition cSPos;
    CommandSetRotation cSRot;
    CommandGetLayers cLGet;
    CommandCreateLayer cLCreate;
    CommandRemoveLayer cLRemove;
    CommandRebindLayer cLRebind;
    CommandModifyLayerCollision cLModify;
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
      case Commands::GetLayers:
        cLGet.cmd = ret.cmd;
        cLGet.commandId = ret.commandId;
        cLGet.objectLayers = GetBooleanProp(env, v, "object", true);
        cLGet.broadPhaseLayers = GetBooleanProp(env, v, "broadPhase", false);
        return cLGet;
      case Commands::RemoveLayer:
        cLRemove.cmd = ret.cmd;
        cLRemove.commandId = ret.commandId;
        cLRemove.name = GetStringProp(env, v, "name", "");
        cLRemove.isBroadPhase = GetStringProp(env, v, "type", "object") != "object";
        return cLRemove;
      case Commands::CreateLayer:
        cLCreate.cmd = ret.cmd;
        cLCreate.commandId = ret.commandId;
        cLCreate.name = GetStringProp(env, v, "name", "");
        cLCreate.isBroadPhase = GetStringProp(env, v, "type", "object") != "object";
        return cLCreate;
      case Commands::RebindLayer:
        cLRebind.cmd = ret.cmd;
        cLRebind.commandId = ret.commandId;
        cLRebind.objectLayer = GetStringProp(env, v, "objectLayer", "");
        cLRebind.broadPhaseLayer = GetStringProp(env, v, "broadPhaseLayer", "");
        cLRebind.bind = GetStringProp(env, v, "action", "bind") == "bind" ? true : false;
        return cLRebind;
      case Commands::ModifyLayerCollision:
        cLModify.cmd = ret.cmd;
        cLModify.commandId = ret.commandId;
        cLModify.layer1 = GetStringProp(env, v, "layer1", "");
        cLModify.layer2 = GetStringProp(env, v, "layer2", "");
        cLModify.collide = GetBooleanProp(env, v, "collide", false);
        return cLModify;
      // -------------------    body
      case Commands::AddBody:
        // std::cout << "AddBody?" << std::endl;
        cABody.cmd = ret.cmd;
        cABody.commandId = ret.commandId;
        cABody.bodyId = GetUInt32Prop(env, v, "bodyId");
        cABody.activate = GetBooleanProp(env, v, "activate");
        return cABody;
      case Commands::DestroyBody:
        cDBody.commandId = ret.commandId;
        cDBody.cmd = ret.cmd;
        cDBody.bodyId = GetUInt32Prop(env, v, "bodyId");
        cDBody.force = GetBooleanProp(env, v, "force");
        return cDBody;
      case Commands::RemoveBody:
      case Commands::ActivateBody:
      case Commands::DeactivateBody:
        cBody.commandId = ret.commandId;
        cBody.cmd = ret.cmd;
        cBody.bodyId = GetUInt32Prop(env, v, "bodyId");
        return cBody;
      case Commands::CreateBody:
        // std::cout << "parsing create command" << std::endl;
        cCBody.commandId = ret.commandId;
        cCBody.cmd = ret.cmd;
        napi_value params;
        napi_get_named_property(env, v, "params", &params);
        cCBody.params = GetBodyCreationSettings(env, params);
        return cCBody;
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
    if (e == Events::EngineFps) return "engineFps";
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
      } else if constexpr (std::is_same_v<T, EngineFpsEvent>) {
        std::string type = eventType(e.type);
        SetStringProp(env, ret, "type", type);
        SetUint64Prop(env, ret, "commandId", e.commandId);
        SetUint32Prop(env, ret, "fps", e.fps);
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
        napi_value params = SetBodyCreationSettings(env, e.params);
        napi_set_named_property(env, ret, "params", params);
        SetUint32Prop(env, ret, "bodyId", e.bodyId);
        return ret;
      } else if constexpr (std::is_same_v<T, BodyTransformEvent>) {
        std::string type = eventType(e.type);
        SetStringProp(env, ret, "type", type);
        SetUint32Prop(env, ret, "bodyId", e.bodyId);
        SetRMat44Prop(env, ret, "transform", e.transform);
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

JPH::EMotionType JsConvert::GetMotionType(napi_env env, napi_value v) {
  std::string str = GetString(env, v);
  if (str == "dynamic") return JPH::EMotionType::Dynamic;
  if (str == "kinematic") return JPH::EMotionType::Kinematic;
  return JPH::EMotionType::Static;
}

napi_value JsConvert::SetMotionType(napi_env env, JPH::EMotionType v) {
  std::string s = "static";
  if (v == JPH::EMotionType::Dynamic) s = "dynamic";
  if (v == JPH::EMotionType::Kinematic) s = "kinematic";
  return SetString(env, s);
}

void JsConvert::SetMotionTypeProp(napi_env env, napi_value obj, const char * key, JPH::EMotionType v) {
  napi_value nv = SetMotionType(env, v);
  napi_set_named_property(env, obj, key, nv);
}

JPH::EShapeType  JsConvert::GetShapeType(napi_env env, napi_value v) {
  std::string str = GetString(env, v);
  if (str == "convex") return JPH::EShapeType::Convex;							///< Used by ConvexShape, all shapes that use the generic convex vs convex collision detection system (box, sphere, capsule, tapered capsule, cylinder, triangle)
	if (str == "compound") return JPH::EShapeType::Compound;						///< Used by CompoundShape
	if (str == "decorated") return JPH::EShapeType::Decorated;						///< Used by DecoratedShape
	if (str == "mesh") return JPH::EShapeType::Mesh;							///< Used by MeshShape
	if (str == "heightField") return JPH::EShapeType::HeightField;					///< Used by HeightFieldShape
	if (str == "softBody") return JPH::EShapeType::SoftBody;						///< Used by SoftBodyShape

	// User defined shapes
	if (str == "user1") return JPH::EShapeType::User1;
	if (str == "user2") return JPH::EShapeType::User2;
	if (str == "user3") return JPH::EShapeType::User3;
	if (str == "user4") return JPH::EShapeType::User4;

	if (str == "plane") return JPH::EShapeType::Plane;							///< Used by PlaneShape
	// if (str == "empty")
  return JPH::EShapeType::Empty;							///< Used by EmptyShape 
}
JPH::EShapeSubType JsConvert::GetShapeSubType(napi_env env, napi_value v) {
  std::string str = GetString(env, v);
  // std::cout << "get subtype" << str << std::endl;
  	// Convex shapes
	if (str == "sphere") {
    // std::cout << "subType is sphere" << static_cast<uint32_t>(JPH::EShapeSubType::Sphere) << std::endl;
    return JPH::EShapeSubType::Sphere;
  }
	if (str == "box") {
    // std::cout << "subType is box" << static_cast<uint32_t>(JPH::EShapeSubType::Box) << std::endl;
    return JPH::EShapeSubType::Box;
  }
	if (str == "triangle") return JPH::EShapeSubType::Triangle;
	if (str == "capsule") return JPH::EShapeSubType::Capsule;
	if (str == "taperedCapsule") return JPH::EShapeSubType::TaperedCapsule;
	if (str == "cylinder") return JPH::EShapeSubType::Cylinder;
	if (str == "convexHull") return JPH::EShapeSubType::ConvexHull;

	// Compound shapes
	if (str == "staticCompound") return JPH::EShapeSubType::StaticCompound;
	if (str == "mutableCompound") return JPH::EShapeSubType::MutableCompound;

	// Decorated shapes
	if (str == "rotatedTranslated") return JPH::EShapeSubType::RotatedTranslated;
	if (str == "scaled") return JPH::EShapeSubType::Scaled;
	if (str == "offsetCenterOfMass") return JPH::EShapeSubType::OffsetCenterOfMass;

	// Other shapes
	if (str == "mesh") return JPH::EShapeSubType::Mesh;
	if (str == "heightField") return JPH::EShapeSubType::HeightField;
	if (str == "softBody") return JPH::EShapeSubType::SoftBody;

	// User defined shapes
	if (str == "user1") return JPH::EShapeSubType::User1;
	if (str == "user2") return JPH::EShapeSubType::User2;
	if (str == "user3") return JPH::EShapeSubType::User3;
	if (str == "user4") return JPH::EShapeSubType::User4;
	if (str == "user5") return JPH::EShapeSubType::User5;
	if (str == "user6") return JPH::EShapeSubType::User6;
	if (str == "user7") return JPH::EShapeSubType::User7;
	if (str == "user8") return JPH::EShapeSubType::User8;

	// User defined convex shapes
	if (str == "userConvex1") return JPH::EShapeSubType::UserConvex1;
	if (str == "userConvex1") return JPH::EShapeSubType::UserConvex2;
	if (str == "userConvex1") return JPH::EShapeSubType::UserConvex3;
	if (str == "userConvex1") return JPH::EShapeSubType::UserConvex4;
	if (str == "userConvex1") return JPH::EShapeSubType::UserConvex5;
	if (str == "userConvex1") return JPH::EShapeSubType::UserConvex6;
	if (str == "userConvex1") return JPH::EShapeSubType::UserConvex7;
	if (str == "userConvex1") return JPH::EShapeSubType::UserConvex8;

	// Other shapes
	if (str == "plane") return JPH::EShapeSubType::Plane;
	if (str == "taperedCylinder") return JPH::EShapeSubType::TaperedCylinder;
	// if (str == "empty")
  return JPH::EShapeSubType::Empty;
}
std::string JsConvert::StrShapeType(JPH::EShapeType v) {
  std::string tp = "empty";
  if (v == JPH::EShapeType::Convex) tp = "convex";							///< Used by ConvexShape, all shapes that use the generic convex vs convex collision detection system (box, sphere, capsule, tapered capsule, cylinder, triangle)
	if (v == JPH::EShapeType::Compound) tp = "compound";						///< Used by CompoundShape
	if (v == JPH::EShapeType::Decorated) tp = "decorated";						///< Used by DecoratedShape
	if (v == JPH::EShapeType::Mesh) tp = "mesh";							///< Used by MeshShape
	if (v == JPH::EShapeType::HeightField) tp = "heightField";					///< Used by HeightFieldShape
	if (v == JPH::EShapeType::SoftBody) tp = "softBody";						///< Used by SoftBodyShape

	// User defined shapes
	if (v == JPH::EShapeType::User1) tp = "user1";
	if (v == JPH::EShapeType::User2) tp = "user2";
	if (v == JPH::EShapeType::User3) tp = "user3";
	if (v == JPH::EShapeType::User4) tp = "user4";

	if (v == JPH::EShapeType::Plane) tp = "plane";
	if (v == JPH::EShapeType::Empty) tp = "empty";
  return tp;
}

std::string JsConvert::StrShapeSubType(JPH::EShapeSubType v) {
	// Convex shapes
	if (v == JPH::EShapeSubType::Sphere) return "sphere";
	if (v == JPH::EShapeSubType::Box) return "box";
	if (v == JPH::EShapeSubType::Triangle) return "triangle";
	if (v == JPH::EShapeSubType::Capsule) return "capsule";
	if (v == JPH::EShapeSubType::TaperedCapsule) return "taperedCapsule";
	if (v == JPH::EShapeSubType::Cylinder) return "cylinder";
	if (v == JPH::EShapeSubType::ConvexHull) return "convexHull";

	// Compound shapes
	if (v == JPH::EShapeSubType::StaticCompound) return "staticCompound";
	if (v == JPH::EShapeSubType::MutableCompound) return "mutableCompound";

	// Decorated shapes
	if (v == JPH::EShapeSubType::RotatedTranslated) return "rotatedTranslated";
	if (v == JPH::EShapeSubType::Scaled) return "scaled";
	if (v == JPH::EShapeSubType::OffsetCenterOfMass) return "offsetCenterOfMass";

	// Other shapes
	if (v == JPH::EShapeSubType::Mesh) return "mesh";
	if (v == JPH::EShapeSubType::HeightField) return "heightField";
	if (v == JPH::EShapeSubType::SoftBody) return "softBody";

	// User defined shapes
	if (v == JPH::EShapeSubType::User1) return "user1";
	if (v == JPH::EShapeSubType::User2) return "user2";
	if (v == JPH::EShapeSubType::User3) return "user3";
	if (v == JPH::EShapeSubType::User4) return "user4";
	if (v == JPH::EShapeSubType::User5) return "user5";
	if (v == JPH::EShapeSubType::User6) return "user6";
	if (v == JPH::EShapeSubType::User7) return "user7";
	if (v == JPH::EShapeSubType::User8) return "user8";

	// User defined convex shapes
	if (v == JPH::EShapeSubType::UserConvex1) return "userConvex1";
	if (v == JPH::EShapeSubType::UserConvex2) return "userConvex2";
	if (v == JPH::EShapeSubType::UserConvex3) return "userConvex3";
	if (v == JPH::EShapeSubType::UserConvex4) return "userConvex4";
	if (v == JPH::EShapeSubType::UserConvex5) return "userConvex5";
	if (v == JPH::EShapeSubType::UserConvex6) return "userConvex6";
	if (v == JPH::EShapeSubType::UserConvex7) return "userConvex7";
	if (v == JPH::EShapeSubType::UserConvex8) return "userConvex8";

	// Other shapes
	if (v == JPH::EShapeSubType::Plane) return "plane";
	if (v == JPH::EShapeSubType::TaperedCylinder) return "taperedCylinder";
	// if (v == JPH::EShapeSubType::Empty) return "empty";
  return "empty";
}


napi_value JsConvert::SetShapeType(napi_env env, JPH::EShapeType v) {
  std::string s = StrShapeType(v);
  return SetString(env, s);
}
napi_value JsConvert::SetShapeSubType(napi_env env, JPH::EShapeSubType v) {
  std::string s = StrShapeSubType(v);
  return SetString(env, s);
}

void JsConvert::SetShapeTypeProp(napi_env env, napi_value obj, const char * key, JPH::EShapeType v) {
  napi_value nv = SetShapeType(env, v);
  napi_set_named_property(env, obj, key, nv);
}
void JsConvert::SetShapeSubTypeProp(napi_env env, napi_value obj, const char * key, JPH::EShapeSubType v) {
  napi_value nv = SetShapeSubType(env, v);
  napi_set_named_property(env, obj, key, nv);
}




JPH::EMotionType JsConvert::GetMotionTypeProp(napi_env env, napi_value obj, const char * key) {
  napi_value v;
  napi_get_named_property(env, obj, key, &v);
  return GetMotionType(env, v);
}
JPH::EShapeType JsConvert::GetShapeTypeProp(napi_env env, napi_value obj, const char * key) {
  napi_value v;
  napi_get_named_property(env, obj, key, &v);
  return GetShapeType(env, v);
}
JPH::EShapeSubType JsConvert::GetShapeSubTypeProp(napi_env env, napi_value obj, const char * key) {
  napi_value v;
  napi_get_named_property(env, obj, key, &v);
  return GetShapeSubType(env, v);
}
BodyCreationSettings JsConvert::GetBodyCreationSettings(napi_env env, napi_value v) {
  BodyCreationSettings ret;
  ret.type = GetShapeTypeProp(env, v, "type");
  ret.subType = GetShapeSubTypeProp(env, v, "subType");
  ret.position = GetVec3Prop(env, v, "position");
  ret.rotation = GetQuatProp(env, v, "rotation");
  ret.motionType = GetMotionTypeProp(env, v, "motionType");
  ret.layer = static_cast<JPH::ObjectLayer>(GetUInt32Prop(env, v, "layer"));
  ret.addToPhysics = GetBooleanProp(env, v, "addToPhysics");
  ret.activate = GetBooleanProp(env, v, "activate");
  napi_value nshape;
  napi_get_named_property(env, v, "shape", &nshape);
  if (ret.subType == JPH::EShapeSubType::Sphere) {
    SphereSubShape sphereShape;
    sphereShape.convexRadius = GetFloatProp(env, nshape, "convexRadius", 0.0);
    sphereShape.density = GetFloatProp(env, nshape, "density", 1.0);
    sphereShape.radius = GetFloatProp(env, nshape, "radius", 1.0);
    ret.shape = sphereShape;
  }
  if (ret.subType == JPH::EShapeSubType::Box) {
    BoxSubShape boxShape;
    boxShape.convexRadius = GetFloatProp(env, nshape, "convexRadius", 0.0);
    boxShape.density = GetFloatProp(env, nshape, "density", 1.0);
    boxShape.halfExtend = GetVec3Prop(env, nshape, "halfExtend");
    ret.shape = boxShape;
  }
  return ret;
}

napi_value JsConvert::SetBodyCreationSettings(napi_env env, const BodyCreationSettings &bcs) {
  napi_value ret;
  napi_create_object(env, &ret);
  SetShapeTypeProp(env, ret, "type", bcs.type);
  SetShapeSubTypeProp(env, ret, "subType", bcs.subType);
  SetVec3Prop(env, ret, "position", bcs.position);
  SetQuatProp(env, ret, "rotation", bcs.rotation);
  SetBooleanProp(env, ret, "addToPhysics", bcs.addToPhysics);
  SetBooleanProp(env, ret, "activate", bcs.activate);
  SetMotionTypeProp(env, ret, "motionType", bcs.motionType);
  SetUint32Prop(env, ret, "layer", bcs.layer);
  napi_value nshape;
  napi_create_object(env, &nshape);
  std::visit([env, bcs, nshape](auto&& shape) {
    using T = std::decay_t<decltype(shape)>;

    if constexpr (std::is_same_v<T, BoxSubShape>) {
      SetVec3Prop(env, nshape, "halfExtend", shape.halfExtend);
    } else if constexpr (std::is_same_v<T, SphereSubShape>) {
      SetFloatProp(env, nshape, "radius", shape.radius);
    }
  }, bcs.shape);
  napi_set_named_property(env, ret, "shape", nshape);
  return ret;
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
