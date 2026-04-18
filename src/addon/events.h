#pragma once

#include "includes.h"
#include "enums.h"
#include "shapes.h"

namespace JOLT {

struct EventBase {
  Events    type;
  uint64_t  commandId;
};

struct SuccessEvent: EventBase {
  bool      success = true;
};

struct LayersListEvent: EventBase {
  std::vector<std::string> objectLayers;
  std::vector<std::string> broadPhaseLayers;
};

struct BodyEvent: SuccessEvent {
  uint32_t  bodyId;
};

struct BodyCreationEvent : BodyEvent {
  BodyCreationSettings  params;
};

struct BodyTransformEvent: BodyEvent {
  JPH::RMat44 transform;
};

struct EngineFpsEvent: EventBase {
  uint32_t    fps;
};

using JEvent = std::variant<
  EventBase,
  SuccessEvent,
  // LayersListEvent,
  BodyEvent,
  BodyCreationEvent,
  BodyTransformEvent,
  EngineFpsEvent
>;

struct EGen {
  static SuccessEvent Init(uint64_t cmd, bool result) {
    SuccessEvent ret;
    ret.type = Events::Init;
    ret.commandId = cmd;
    ret.success = result;
    return ret;
  }

  static SuccessEvent Start(uint64_t cmd, bool result) {
    SuccessEvent ret;
    ret.type = Events::Start;
    ret.commandId = cmd;
    ret.success = result;
    return ret;
  }
  static SuccessEvent Stop(uint64_t cmd, bool result) {
    SuccessEvent ret;
    ret.type = Events::Stop;
    ret.commandId = cmd;
    ret.success = result;
    return ret;
  }
  static SuccessEvent Step(uint64_t cmd, bool result) {
    SuccessEvent ret;
    ret.type = Events::Step;
    ret.commandId = cmd;
    ret.success = result;
    return ret;
  }
  static SuccessEvent Shutdown(uint64_t cmd, bool result) {
    SuccessEvent ret;
    ret.type = Events::Shutdown;
    ret.commandId = cmd;
    ret.success = result;
    return ret;
  }
  // -------------    layers
  // static LayersListEvent LayersList(uint64_t cmd, std::vector<std::string> oLayers, std::vector<std::string> bLayers) {
    // LayersListEvent ret;
    // ret.type = Events::G
  // }
  // -------------    bodies
  static BodyCreationEvent BodyCreated(uint64_t cmd, bool success, BID bodyId, BodyCreationSettings params) {
    BodyCreationEvent ret;
    ret.type = Events::BodyCreated;
    ret.commandId = cmd;
    ret.success = success;
    ret.bodyId = success ? static_cast<uint32_t>(bodyId) : 0xffffffff;
    ret.params = params;
    return ret;
  }
  static BodyTransformEvent BodyTransform(BID bodyId, JPH::RMat44 mat) {
    BodyTransformEvent ret;
    ret.type = Events::BodyTransform;
    ret.bodyId = static_cast<uint32_t>(bodyId);
    ret.transform = mat;
    return ret;
  }
  static EngineFpsEvent FPS(uint32_t fps) {
    EngineFpsEvent ret;
    ret.type = Events::EngineFps;
    ret.fps = fps;
    ret.commandId = 0;
    return ret;
  }
  static BodyEvent BodyAdded(uint64_t cmd, uint32_t bid, bool success) {
    BodyEvent ret;
    ret.type = Events::BodyAdded;
    ret.commandId = cmd;
    ret.bodyId = bid;
    ret.success = success;
    return ret;
  }
  static BodyEvent BodyRemoved(uint64_t cmd, uint32_t bid, bool success) {
    BodyEvent ret;
    ret.type = Events::BodyRemoved;
    ret.commandId = cmd;
    ret.bodyId = bid;
    ret.success = success;
    return ret;
  }
  static BodyEvent BodyActivated(uint64_t cmd, uint32_t bid, bool success) {
    BodyEvent ret;
    ret.type = Events::BodyActivated;
    ret.commandId = cmd;
    ret.bodyId = bid;
    ret.success = success;
    return ret;
  }
  static BodyEvent BodyDeactivated(uint64_t cmd, uint32_t bid, bool success) {
    BodyEvent ret;
    ret.type = Events::BodyDeactivated;
    ret.commandId = cmd;
    ret.bodyId = bid;
    ret.success = success;
    return ret;
  }
  static BodyEvent BodyDestroyed(uint64_t cmd, uint32_t bid, bool success) {
    BodyEvent ret;
    ret.type = Events::BodyDestroyed;
    ret.commandId = cmd;
    ret.bodyId = bid;
    ret.success = success;
    return ret;
  }
};

}
