#pragma once

#include "includes.h"
#include "jolt.h"

namespace JOLT {

  struct ShapeBase {
    JPH::EShapeType       type;
    JPH::EShapeSubType    subType;
  };

  struct ConvexShape: ShapeBase {
    float                 convexRadius;
    float                 density;
  };

  struct SphereSubShape: ConvexShape {
    float                 radius;
  };
  struct BoxSubShape: ConvexShape {
    JPH::Vec3             halfExtend;
  };


  using JSubShape = std::variant<
    SphereSubShape,
    BoxSubShape
  >;

  struct BodyCreationSettings {
    JPH::EShapeType       type;
    JPH::EShapeSubType    subType;
    JSubShape             shape;
    JPH::Vec3             position;
    JPH::Quat             rotation;
    JPH::EMotionType      motionType;
    JPH::ObjectLayer      layer;
    bool                  addToPhysics;
    bool                  activate;
  };

}
