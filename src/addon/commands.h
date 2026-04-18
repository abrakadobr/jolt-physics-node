#pragma once

#include "includes.h"
#include "structs.h"
#include "shapes.h"
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

struct CommandBodyAdd: CommandBody {
  bool activate;
};

struct CommandBodyDestroy: CommandBody {
  bool force;
};

struct CommandBodyCreate: CommandBase {
  BodyCreationSettings  params;
};

struct CommandSetPosition: CommandBody {
  JPH::Vec3   position;
};

struct CommandSetRotation: CommandBody {
  JPH::Quat   rotation;
};
// --------------   LAYERS    ---------------
struct CommandGetLayers: CommandBase {
  bool        objectLayers = true;
  bool        broadPhaseLayers = true;
};
struct CommandCreateLayer: CommandBase {
  std::string name;
  bool        isBroadPhase;
};
struct CommandRemoveLayer: CommandBase {
  std::string name;
  bool        isBroadPhase;
  bool        force = false;
};
struct CommandRebindLayer: CommandBase {
  std::string objectLayer;
  std::string broadPhaseLayer;
  bool        bind;
};
struct CommandModifyLayerCollision: CommandBase {
  std::string layer1;
  std::string layer2;
  bool        collide;
};

using JCommand = std::variant<
  CommandBase,
  CommandInit,
  CommandGetLayers,
  CommandCreateLayer,
  CommandRemoveLayer,
  CommandRebindLayer,
  CommandModifyLayerCollision,
  CommandBody,
  CommandBodyAdd,
  CommandBodyDestroy,
  CommandBodyCreate,
  CommandSetPosition,
  CommandSetRotation
>;

}
