#pragma once

#include "jolt.h"

namespace JOLT {

typedef uint32_t  BID;

struct WorldConfig {
  float       gravity = 9.8;
  uint32_t    memoryPreallocatedMb = 10;
  uint32_t    maxBodies = 65535;
  uint32_t    numBodyMutexes = 0;
  uint32_t    maxBodiesPairs = 65535;
  uint32_t    maxContacts = 10240;
};

/*
struct BodyCreationParams {
  JPH::Vec3         position = JPH::Vec3::sZero();
  JPH::Quat         rotation = JPH::Quat::sIdentity();
  JPH::EMotionType  motionType = JPH::EMotionType::Static;
  JPH::ObjectLayer  layer = 0;
};
*/

struct BLayer {
  std::string               name;
  uint32_t                   id;
  JPH::BroadPhaseLayer      broadPhaseLayer;
};

struct OLayer {
  std::string               name;
  uint32_t                  id;
  JPH::ObjectLayer          objectLayer = 0;
  std::vector<uint32_t>     oCollisions;
  std::vector<uint32_t>     bCollisions;
  uint32_t                  bLayerId;
  bool                      collides = false;
};

}
