#pragma once

namespace JOLT {


enum class BodyMotionType: uint8_t {
  Static,     ///< Non movable
  Kinematic,  ///< Movable using velocities only, does not respond to forces
  Dynamic,    ///< Responds to forces as a normal physics object
};


enum class WorldState: uint8_t {
  Stop,
  Step,
  Run
};


enum class Commands: uint32_t {
  Init,
  Start,
  Stop,
  Step,
  Shutdown,
  CreateBox,
  CreateSphere,
  SetPosition,
  SetRotation,
  // SetPositionAndRotation,
  AddBody,
  RemoveBody,
  ActivateBody,
  DeactivateBody,
  DestroyBody,
  COUNT,
  Invalid
};

enum class Events: uint32_t {
  Init,
  Start,
  Stop,
  Step,
  Shutdown,
  BodyCreated,
  BodyDestroyed,
  BodyAdded,
  BodyRemoved,
  BodyActivated,
  BodyDeactivated,
  BodyTransform,

  Error,
  COUNT,
  Invalid
};

}
