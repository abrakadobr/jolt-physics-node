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
#include "triangle.h"
#include "capsule.h"
#include "convexhull.h"

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
          METHOD(BodyManager,snapshotBodies),
          METHOD(BodyManager,applySnapshot),
          METHOD(BodyManager,createBox),
          METHOD(BodyManager,createSphere),
          METHOD(BodyManager,createTriangle),
          METHOD(BodyManager,createCapsule),
          METHOD(BodyManager,snapshotBodies),
          METHOD(BodyManager,snapshotState),
          // METHOD(BodyManager,addBody),
          METHOD(BodyManager,addBodyI),
          // METHOD(BodyManager,addBodies),
          METHOD(BodyManager,addBodiesI),
          // METHOD(BodyManager,removeBody),
          METHOD(BodyManager,removeBodyI),
          // METHOD(BodyManager,removeBodies),
          METHOD(BodyManager,removeBodiesI),
          // METHOD(BodyManager,destroyBody),
          METHOD(BodyManager,destroyBodyI),
          // METHOD(BodyManager,destroyBodies),
          METHOD(BodyManager,destroyBodiesI),
          METHOD(BodyManager,setBodyPositionI),
          METHOD(BodyManager,reshapeBodyToSphereI),

          // METHOD(BodyManager,createConvexHull)
        };
      }

      static BodyMotionType motionTypeFromJolt(JPH::EMotionType mt);
      static JPH::EMotionType motionTypeToJolt(BodyMotionType tp);

      explicit BodyManager(napi_env env);
      ~BodyManager();

      void        initialize(World * _world);
      void        update(int frames);
      void        reset();

      Body*       getBody(JPH::BodyID id);
      Body*       getBodyI(uint32_t id);

      template<class T>
      T * getTBodyI(uint32_t bid) {
        auto it = _bodies.find(bid);
        return it != _bodies.end() ? static_cast<T* >(it->second) : nullptr;
      }


      std::vector<uint8_t> snapshotState();
      std::vector<uint8_t> snapshotBodies(std::vector<uint32_t> bids);
      bool        applySnapshot(std::vector<uint8_t> data);

      void        trackBody(uint32_t id, Body* body);

      Box         * createBox(const BoxShape &shape, const BodyCreationSettings &settings);
      Sphere      * createSphere(const SphereShape &shape, const BodyCreationSettings &settings);
      Triangle    * createTriangle(const TriangleShape &shape, const BodyCreationSettings &settings);
      Capsule     * createCapsule(const CapsuleShape &shape, const BodyCreationSettings &settings);

      //  add existing body to physics system
      void        addBody(Body * body, bool active);
      void        addBodyI(uint32_t bid, bool active);
      void        addBodies(std::vector<Body*> bodies, bool active);
      void        addBodiesI(std::vector<uint32_t> bids, bool active);
      //  remove body from physics system (keeps body in memory)
      void        removeBody(Body * body);
      void        removeBodyI(uint32_t bid);
      void        removeBodies(std::vector<Body*> bodies);
      void        removeBodiesI(std::vector<uint32_t> bids);
      //  destroy body (remove + free)
      void        destroyBody(Body * body);
      void        destroyBodyI(uint32_t bid);
      void        destroyBodies(std::vector<Body*> bodies);
      void        destroyBodiesI(std::vector<uint32_t> bids);

      void        setBodyPositionI(uint32_t bid, JPH::Vec3 position);

      void        reshapeBodyToSphereI(uint32_t bid, const SphereShape &shape);
      //void        activateBodies()
      // Box           * createBox(const JPH::Vec3 &halfSize, const JPH::Vec3 &pos = JPH::Vec3(0, 0, 0), const JPH::Quat &rot = JPH::Quat::sIdentity(), bool activate = false, const BodyMotionType &motionType = BodyMotionType::Static, const std::string &layer = "static");
      // Sphere        * createSphere(const float radius, const JPH::Vec3 &pos = JPH::Vec3(0, 0, 0), const JPH::Quat &rot = JPH::Quat::sIdentity(), bool activate = false, const BodyMotionType &motionType = BodyMotionType::Static, const std::string &layer = "static");
      // Triangle      * createTriangle(const JPH::Vec3 &p1, const JPH::Vec3 &p2, const JPH::Vec3 p3, const float radius, const JPH::Vec3 &pos = JPH::Vec3(0, 0, 0), const JPH::Quat &rot = JPH::Quat::sIdentity(), bool activate = false, const BodyMotionType &motionType = BodyMotionType::Static, const std::string &layer = "static");
      // Capsule       * createCapsule(const float halfHeight, const float radius, const JPH::Vec3 &pos = JPH::Vec3(0, 0, 0), const JPH::Quat &rot = JPH::Quat::sIdentity(), bool activate = false, const BodyMotionType &motionType = BodyMotionType::Static, const std::string &layer = "static");
      // ConvexHull    * createConvexHull(const JPH::Vec3 * points, int pointsNum, const float radius, const JPH::Vec3 &pos = JPH::Vec3(0, 0, 0), const JPH::Quat &rot = JPH::Quat::sIdentity(), bool activate = false, const BodyMotionType &motionType = BodyMotionType::Static, const std::string &layer = "static");
    private:
      template<class T>
      T* _addBody(T* body, JPH::Shape* shape, const BodyCreationSettings &settings);

      napi_env                      _nenv = nullptr;
      World                         * _world = nullptr;
      JPH::PhysicsSystem            * _physicsSystem = nullptr;
      JPH::BodyInterface            * _bodyInterface = nullptr;

      std::map<uint32_t, Body*>     _bodies;
  };

}
