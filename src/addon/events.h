#pragma once

#include "includes.h"
#include "enums.h"

namespace JOLT {

struct EventBase {
  Events    type;
  uint64_t  commandId;
};

struct SuccessEvent: EventBase {
  bool      success = true;
};

struct BodyEvent: SuccessEvent {
  uint32_t  bodyId;
};

struct BodyCreationEvent : BodyEvent {
  uint8_t   shapeType;
  uint8_t   sapeSubType;
};

using JEvent = std::variant<
  EventBase,
  SuccessEvent,
  BodyEvent,
  BodyCreationEvent
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
};

}
