#include "body.h"
#include "../napi/jolt_convert.h"
#include <Jolt/Physics/Body/BodyID.h>
#include <iostream>

namespace JOLT {

  Body::Body() {}

  Body::~Body() {}

  void Body::update(int frames) {
    if (_body == nullptr || _bodyInterface == nullptr) return;
    if (_body->GetMotionType() == JPH::EMotionType::Static && frames > 0) return;

    JPH::BodyID bid = _body->GetID();
    // std::cout << "update" << bid.GetIndexAndSequenceNumber();
    JPH::Vec3 newPos = _bodyInterface->GetPosition(bid);
    JPH::Quat newRot = _bodyInterface->GetRotation(bid);
    JPH::Vec3 newVel = _bodyInterface->GetLinearVelocity(bid);

    bool transformChanged = (newPos != _position || newRot != _rotation);
    bool velocityChanged  = (newVel != _linearVelocity);

    if (transformChanged || frames == 0) {
      _position     = newPos;
      _rotation     = newRot;
      _comPosition  = _bodyInterface->GetCenterOfMassPosition(bid);
      _transform    = _bodyInterface->GetWorldTransform(bid);
      _comTransform = _bodyInterface->GetCenterOfMassTransform(bid);
      if (frames != 0)
        emit("transform", EventBodyTransform{ bid, newPos, newRot });
    }

    if (velocityChanged || frames == 0) {
      _linearVelocity = newVel;
      if (frames != 0)
        emit("velocity", EventBodyVelocity{ bid, newVel });
    }
  }

  uint32_t Body::id() const {
    if (_body == nullptr) return JPH::BodyID::cInvalidBodyID;
    JPH::BodyID bid = _body->GetID();
    return bid.GetIndexAndSequenceNumber();
  }

  void Body::getShape(AbstractShape &shape) {
    const JPH::Shape * joltShape = _body->GetShape();
    shape.type = fromJoltShapeType(joltShape->GetType());
    shape.subType = fromJoltShapeSubType(joltShape->GetSubType());
    shape.userData = joltShape->GetUserData();
    // JPH::PhysicsMaterial * mat = joltShape->GetMaterial();
    // shape.material.name = std::string(mat->GetDebugName());
    // if (joltShape->GetType() == JPH::EShapeType::Convex)
    // shape.convexRadius = static_cast<const JPH::ConvexShape* >(joltShape)->GetConvexRadius();
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

  JPH::Vec3           Body::position() const {
    return _position;
  }
  JPH::Vec3           Body::comPosition() const {
    return _comPosition;
  }
  JPH::Quat           Body::rotation() const {
    return _rotation;
  }
  JPH::Vec3           Body::linearVelocity() const {
    return _linearVelocity;
  }
  JPH::RMat44         Body::transform() const {
    return _transform;
  }
  JPH::RMat44         Body::comTransform() const {
    return _comTransform;
  }

  void                Body::setPosition(const JPH::Vec3 &position) {
    _bodyInterface->SetPosition(_body->GetID(), position, JPH::EActivation::Activate);
    update(-1);
  }
  void                Body::setRotation(const JPH::Quat &rotation) {
    _bodyInterface->SetRotation(_body->GetID(), rotation, JPH::EActivation::Activate);
    update(-1);
  }
  void                Body::setPositionAndRotation(const JPH::Vec3 &position, const JPH::Quat &rotation) {
    _bodyInterface->SetPositionAndRotation(_body->GetID(), position, rotation, JPH::EActivation::Activate);
    update(-1);
  }

  JPH::BodyID         Body::GetID() const  {
    return _body->GetID();
  }
}
