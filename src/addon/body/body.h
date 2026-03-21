#pragma once

#include "body_shapes.h"
#include "../jolt.h"

namespace JOLT {

  class Body {

    public:

      explicit Body();
      ~Body();

      BodyShapeType       getType() const;
      void                setType(const BodyShapeType &type);
      JPH::Body *         getJoltBody();
      void                setJoltBody(JPH::Body * body);
      void                setJoltBodyInterface(JPH::BodyInterface * bodyInterface);

      JPH::Vec3           position() const;
      JPH::Quat           rotation() const;

      uint32_t            id() const;
    protected:
      BodyShapeType       _bodyShape;
      JPH::BodyInterface  * _bodyInterface;
      JPH::Body           * _body;
      JPH::Vec3           _position;
      JPH::Quat           _rotation;

  };

}
