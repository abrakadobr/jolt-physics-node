#pragma once

#include "includes.h"
#include "structs.h"
#include "enums.h"

namespace JOLT {

struct CommandBase {
  uint64_t commandId;
  Commands cmd;

  inline static CommandBase Invalid() {
    CommandBase c;
    c.cmd = Commands::Invalid;
    return c;
  }
};

struct CommandInit : CommandBase {
  WorldConfig config;
};

struct CommandBody: CommandBase {
  BID bodyId;
};

struct CommandCreateBox: CommandBase {
  JPH::Vec3           half;
  BodyCreationParams  params;
};

struct CommandCreateSphere: CommandBase {
  float               radius;
  BodyCreationParams  params;
};

struct CommandSetPosition: CommandBody {
  JPH::Vec3   position;
};

struct CommandSetRotation: CommandBody {
  JPH::Quat   rotation;
};

using JCommand = std::variant<
  CommandBase,
  CommandInit,
  CommandBody,
  CommandCreateBox,
  CommandCreateSphere,
  CommandSetPosition,
  CommandSetRotation
>;

}
