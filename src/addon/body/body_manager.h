/**
 *
 *  This is not a Jolt::BodyManager class. Jolt::BodyManager is inner-jolt class and  not accessible
 *  in regular life by developers who use jost. Various functionality of Jolt::BodyManager exists in
 *  PhysicsSystem, BodyInterface, BodyLockRead/Write
 *
 *  current Joltjs::BodyManager combine functionality from different Jolt classes into single
 *  and will be used by js developer for bodies related things
 *
 **/
#pragma once

#include <map>

#include "../jolt.h"
#include "../napi/napi_base.h"
#include "../defines.h"

#include "body.h"
#include "box.h"
#include "sphere.h"

namespace JOLT {

  class World;

  class BodyManager: public NApiBase<BodyManager> {
    public:
      static constexpr const char* ClassName = "BodyManager";

      static std::vector<napi_property_descriptor> Methods() {
        return {
          // METHOD(BodyManager,initialize),
          // emit — template method, cannot be exposed via METHOD
          METHOD(BodyManager,snapshotState),
          METHOD(BodyManager,applySnapshot)
          // METHOD(World,saveScene),
          // METHOD(World,loadScene),
        };
      }

      static BodyMotionType motionTypeFromJolt(JPH::EMotionType mt);
      static JPH::EMotionType motionTypeToJolt(BodyMotionType tp);

      explicit BodyManager(napi_env env);
      ~BodyManager();

      void initialize(World * _world);

      std::vector<uint8_t> snapshotState();
      bool applySnapshot(std::vector<uint8_t> data);

      // Body  * createBody(BodyShapeType type, )
      Box           * createBox(const JPH::Vec3 &halfSize, const JPH::Vec3 &pos = JPH::Vec3(0, 0, 0), const JPH::Quat &rot = JPH::Quat::sIdentity(), bool activate = false, const BodyMotionType &motionType = BodyMotionType::Static, const std::string &layer = "static");
      Sphere        * createSphere(const float radius, const JPH::Vec3 &pos = JPH::Vec3(0, 0, 0), const JPH::Quat &rot = JPH::Quat::sIdentity(), bool activate = false, const BodyMotionType &motionType = BodyMotionType::Static, const std::string &layer = "static");
    private:
      napi_env                      _nenv = nullptr;
      World                         * _world = nullptr;
      JPH::PhysicsSystem            * _physicsSystem = nullptr;
      JPH::BodyInterface            * _bodyInterface = nullptr;

      std::map<uint32_t, Body*>     _bodies;
  };

}
