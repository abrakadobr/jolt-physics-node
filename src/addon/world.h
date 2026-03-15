#pragma once

#include "napi/napi_base.h"
#include "napi/jolt_convert.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Math/Vec3.h>
#include <map>
#include <vector>
#include <array>
#include <string>
#include <thread>

#include "layers_manager.h"
#include "body/body_manager.h"
#include "events.h"

namespace JOLT {

  class World: public NApiBase<World> {
    public:

      static constexpr const char* ClassName = "World";

      static std::vector<napi_property_descriptor> Methods() {
        return {
          METHOD(World,on),
          METHOD(World,initialize),
          // emit — template method, cannot be exposed via METHOD
          // METHOD(World,snapshotState),
          // METHOD(World,applySnapshot),
          METHOD(World,saveScene),
          METHOD(World,loadScene),
          METHOD(World,setGravity),
          METHOD(World,layers),
          METHOD(World,layersManager),
          METHOD(World,bodiesManager)
        };
      }

      explicit World(napi_env env);
      ~World();

      void initialize(WorldSettings s);

      void on(const std::string &event, JsCallback);
      template<class T>
      void emit(const std::string &event, T data) const {
        if (_callbacksMap.count(event) > 0) {
          for (const JsCallback &cb: _callbacksMap.at(event)) {
            cb.call(data);
          }
        }
      }

      // std::vector<uint8_t> snapshotState();
      // bool applySnapshot(std::vector<uint8_t> data);
      std::vector<uint8_t> saveScene();
      int32_t loadScene(std::vector<uint8_t> data);

      LayersManager * layersManager();
      BodyManager * bodiesManager();

      JPH::PhysicsSystem * joltPhysicsSystem();
      // DebugGeoResult GetDebugGeometry(bool draw_bodies, bool draw_constraints, bool draw_constraint_limits, bool wireframe);

      void setGravity(float gravity);

      // just helper for debugging
      std::vector<std::string> layers();
    private:
      napi_env                      _nenv = nullptr;

      WorldSettings                 _settings;
      LayersManager                 * _layersManager = nullptr;
      BodyManager                   * _bodiesManager = nullptr;

      JsCallbacksMap                _callbacksMap;
      JPH::TempAllocatorImpl        * _tempAllocator = nullptr;
      JPH::JobSystemThreadPool      * _jobSystem = nullptr;
      JPH::PhysicsSystem            * _physicsSystem = nullptr;

      EngineBodyActivationListener  * _bodyActivationListner = nullptr;
      EngineContactListener         * _contactListner = nullptr;
      // BPLayerInterfaceImpl mBroadPhaseLayerInterface;
      // ObjectVsBroadPhaseLayerFilterImpl mObjectVsBroadPhaseLayerFilter;
      // ObjectLayerPairFilterImpl mObjectLayerPairFilter;
      // ActivationListenerImpl mActivationListener;
      // ContactListenerImpl mContactListener;
      // napi_ref mBodyActivationCallbackRef = nullptr;
      // napi_ref mContactCallbackRef = nullptr;
      // std::mutex mPendingEventsMutex;
      // std::vector<PendingEvent> mPendingEvents;

      // uint32_t mNextConstraintId = 1;
      // std::unordered_map<uint32_t, Ref<Constraint>> mConstraints;

      // uint32_t mNextSkeletonId = 1;
      // std::unordered_map<uint32_t, Ref<Skeleton>> mSkeletons;

      // uint32_t mNextRagdollSettingsId = 1;
      // std::unordered_map<uint32_t, Ref<RagdollSettings>> mRagdollSettings;

      // uint32_t mNextRagdollId = 1;
      // std::unordered_map<uint32_t, Ref<Ragdoll>> mRagdolls;
      // std::unordered_map<uint32_t, std::vector<uint32_t>> mRagdollConstraintIds;

      // uint32_t mNextSkeletonPoseId = 1;
      // std::unordered_map<uint32_t, std::unique_ptr<SkeletonPose>> mSkeletonPoses;

      // std::unordered_map<uint32_t, Ref<MutableCompoundShape>> mMutableCompounds;

      // uint32_t mNextCharacterId = 1;
      // std::unordered_map<uint32_t, std::unique_ptr<CharacterVirtual>> mCharacters;


  };

}

// JOLT::REGISTER_CLASS(JOLT::World);
