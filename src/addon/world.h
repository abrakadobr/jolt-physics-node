#pragma once

#include "jolt.h"
#include "napi/napi_base.h"
#include "napi/jolt_convert.h"
#include <map>
#include <vector>
#include <array>
#include <string>
#include <thread>

#include "layers/layers_manager.h"
#include "body/body_manager.h"
#include "events.h"
#include "event_emitter.h"

namespace JOLT {

  class World: public NApiBase<World>, public EventEmitter {
    public:

      static constexpr const char* ClassName = "World";

      static std::vector<napi_property_descriptor> Methods() {
        return {
          METHOD(World,on),
          METHOD(World,initialize),
          // emit — template method, cannot be exposed via METHOD
          // METHOD(World,snapshotState),
          // METHOD(World,applySnapshot),
          METHOD(World,reset),
          METHOD(World,runPhysics),
          METHOD(World,stepPhysics),
          METHOD(World,stopPhysics),
          METHOD(World,state),
          METHOD(World,speed),
          METHOD(World,fps),
          METHOD(World,worldRun),
          METHOD(World,worldStop),
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
      void setGravity(float gravity);

      std::vector<uint8_t> saveScene();
      int32_t loadScene(std::vector<uint8_t> data);

      LayersManager * layersManager();
      BodyManager * bodiesManager();
      JPH::PhysicsSystem * joltPhysicsSystem();
      std::vector<std::string> layers();
      // DebugGeoResult GetDebugGeometry(bool draw_bodies, bool draw_constraints, bool draw_constraint_limits, bool wireframe);
      
      void                          runPhysics(float speed = 1.0);
      void                          stepPhysics();
      void                          stopPhysics();

      WorldState                    state() const;
      float                         speed() const;
      float                         fps() const;

      void                          worldRun(float speed = 1.0, bool withPhysics = false);
      void                          worldMain();
      void                          worldLoop(double deltaMs);
      void                          worldStop();

      void                          reset();
    protected:
      void                          toState(WorldState next);
      void                          setSpeed(float speed);
    private:
      napi_env                      _nenv = nullptr;

      WorldSettings                 _settings;
      LayersManager                 * _layersManager = nullptr;
      BodyManager                   * _bodiesManager = nullptr;

      JPH::TempAllocatorImpl        * _tempAllocator = nullptr;
      JPH::JobSystemThreadPool      * _jobSystem = nullptr;
      JPH::PhysicsSystem            * _physicsSystem = nullptr;

      EngineBodyActivationListener  * _bodyActivationListner = nullptr;
      EngineContactListener         * _contactListner = nullptr;

      WorldState                    _state;
      float                         _speed = 1.0;
      float                         _fps = 0.0;

      double                        _targetFrame = 1.0/60.0;

      uint64_t                      _step = 0;
      bool                          _worldSpins = false;
      bool                          _stepRequested = false; // = true for Step mode on request to do next step

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
