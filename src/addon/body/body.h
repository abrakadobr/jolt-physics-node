#pragma once

#include "body_shapes.h"
#include "../jolt.h"
#include "../event_emitter.h"

namespace JOLT {

  class Body : public EventEmitter {

    public:

      explicit Body();
      ~Body();

      BodyShapeType       getType() const;
      void                setType(const BodyShapeType &type);
      JPH::Body *         getJoltBody();
      void                setJoltBody(JPH::Body * body);
      void                setJoltBodyInterface(JPH::BodyInterface * bodyInterface);
      void                getShape(AbstractShape &shape);

      void                update(int frames);
      JPH::Vec3           position() const;
      JPH::Vec3           comPosition() const;
      JPH::Quat           rotation() const;
      JPH::Vec3           linearVelocity() const;
      JPH::RMat44         transform() const;
      JPH::RMat44         comTransform() const;

      void                setPosition(const JPH::Vec3 &position);
      void                setRotation(const JPH::Quat &rotation);
      void                setPositionAndRotation(const JPH::Vec3 &position, const JPH::Quat &rotation);


      uint32_t            id() const;
      JPH::BodyID         GetID() const;
    protected:
      BodyShapeType       _bodyShape;
      JPH::BodyInterface  * _bodyInterface;
      JPH::Body           * _body;
      JPH::Vec3           _position;
      JPH::Vec3           _comPosition;
      JPH::Quat           _rotation;
      JPH::Vec3           _linearVelocity;
      JPH::RMat44         _transform;
      JPH::RMat44         _comTransform;

  };

}
