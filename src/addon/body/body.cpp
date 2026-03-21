#include "body.h"
#include <Jolt/Physics/Body/BodyID.h>

namespace JOLT {

  Body::Body() {}

  Body::~Body() {}

  uint32_t Body::id() const {
    if (_body == nullptr) return JPH::BodyID::cInvalidBodyID;
    JPH::BodyID bid = _body->GetID();
    return bid.GetIndexAndSequenceNumber();
  }

  BodyShapeType Body::getType() const {
    // add implementation
    return _bodyShape;
  }

  void Body::setType(const BodyShapeType &type) {
    _bodyShape = type;
  }

  void Body::setJoltBody(JPH::Body * body) {
    _body = body;
  }

  JPH::Body * Body::getJoltBody() {
    return _body;
  }

  void Body::setJoltBodyInterface(JPH::BodyInterface * bodyInterface) {
    _bodyInterface = bodyInterface;
  }

  JPH::Vec3 Body::position() const {
    return _position;
  }
  JPH::Quat Body::rotation() const {
    return _rotation;
  }
}
