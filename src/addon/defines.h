#pragma once

namespace JOLT {

  struct WorldSettings {
    float       gravity = 9.8;
    uint32_t    memoryPreallocatedMb = 10;
    uint32_t    maxBodies = 65535;
    uint32_t    numBodyMutexes = 0;
    uint32_t    maxBodiesPairs = 65535;
    uint32_t    maxContacts = 10240;
  };

  enum class BodyMotionType: uint8_t {
    Static,     ///< Non movable
	  Kinematic,  ///< Movable using velocities only, does not respond to forces
	  Dynamic,    ///< Responds to forces as a normal physics object
  };
}
