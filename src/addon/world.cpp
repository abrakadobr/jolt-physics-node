#include "world.h"
#include "napi/napi_registry.h"
#include <chrono>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace JOLT {

  using Clock = std::chrono::steady_clock;

  World::World(napi_env env) : _nenv(env) {}

  void World::initialize(WorldSettings s) {
    _settings = s;
    if (_settings.memoryPreallocatedMb < 1) _settings.memoryPreallocatedMb = 10;
    if (_settings.maxBodies < 1) _settings.maxBodies = 1024;
    if (_settings.maxBodiesPairs < 1) _settings.maxBodiesPairs = 1024;
    if (_settings.maxContacts < 1) _settings.maxContacts = 1024;
    /*
    printf("\n\nsettings mem: %d, bodies: %d, mutex: %d, pairs: %d, contacts: %d, gravity: %f",
      _settings.memoryPreallocatedMb,
      _settings.maxBodies,
      _settings.numBodyMutexes,
      _settings.maxBodiesPairs,
      _settings.maxContacts,
      _settings.gravity
    );
    */

    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    _tempAllocator = new JPH::TempAllocatorImpl(_settings.memoryPreallocatedMb * 1024 * 1024);
    _jobSystem = new JPH::JobSystemThreadPool(
      JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, // comes from Jolt/Physics/PhysicsSettings.h
      std::max(1u, std::thread::hardware_concurrency() - 1)
    );
    _layersManager = new LayersManager(_nenv);
    _layersManager->init();

    _physicsSystem = new JPH::PhysicsSystem();
    _physicsSystem->Init(
      _settings.maxBodies,
      _settings.numBodyMutexes,
      _settings.maxBodiesPairs,
      _settings.maxContacts,
      _layersManager->iBPLayerInterface,
      _layersManager->iObjectVsBroadPhaseLayerFilter,
      _layersManager->iObjectLayerPairFilter
    );
    _bodyActivationListner = new EngineBodyActivationListener();
    _bodyActivationListner->setWorld(this);
    _physicsSystem->SetBodyActivationListener(_bodyActivationListner);

    _contactListner = new EngineContactListener();
    _contactListner->setWorld(this);
    _physicsSystem->SetContactListener(_contactListner);

    _bodiesManager = new BodyManager(_nenv);
    _bodiesManager->initialize(this);

    setGravity(_settings.gravity);
  }

  LayersManager * World::layersManager() {
    return _layersManager;
  }
  BodyManager * World::bodiesManager() {
    return _bodiesManager;
  }
  JPH::PhysicsSystem * World::joltPhysicsSystem() {
    return _physicsSystem;
  }

  std::vector<std::string> World::layers() {
    std::vector<std::string> ret;
    std::vector<uint32_t> ids = _layersManager->layersIDs();
    for (auto lid: ids) {
      ret.push_back(_layersManager->layerName(lid));
    }
    return ret;
  }

  World::~World() {
    if (_bodiesManager) delete _bodiesManager;
    if (_physicsSystem) delete _physicsSystem;
    if (_contactListner)  delete _contactListner;
    if (_bodyActivationListner) delete _bodyActivationListner;
    if (_jobSystem) delete _jobSystem;
    if (_tempAllocator) delete _tempAllocator;
    if (_layersManager) delete _layersManager;
  }

  void World::setGravity(float gravity) {
    _physicsSystem->SetGravity(JPH::Vec3(0.0f, -gravity, 0.0f));
  }


  // --- Serialization ---

  // Full scene save using Jolt PhysicsScene (shapes + current pos/rot/vel).
  // Note: constraints are NOT included.
  std::vector<uint8_t> World::saveScene() {
    JPH::PhysicsScene scene;
    scene.FromPhysicsSystem(_physicsSystem);
    std::ostringstream ss;
    JPH::StreamOutWrapper stream(ss);
    scene.SaveBinaryState(stream, /*inSaveShapes=*/true, /*inSaveGroupFilter=*/true);
    const std::string& str = ss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
  }

  // Restore bodies from saveScene() data. Returns count of created bodies, -1 on error.
  int32_t World::loadScene(std::vector<uint8_t> buf) {
    std::string str(buf.begin(), buf.end());
    std::istringstream ss(str);
    JPH::StreamInWrapper stream(ss);
    JPH::PhysicsScene::PhysicsSceneResult result = JPH::PhysicsScene::sRestoreFromBinaryState(stream);
    if (result.HasError()) return -1;

    // Remember existing body IDs to detect newly created ones after CreateBodies
    JPH::BodyIDVector beforeIds;
    _physicsSystem->GetBodies(beforeIds);
    std::unordered_set<uint32_t> existing;
    existing.reserve(beforeIds.size());
    for (const JPH::BodyID& bid : beforeIds)
      existing.insert(bid.GetIndexAndSequenceNumber());

    if (!result.Get()->CreateBodies(_physicsSystem)) return -1;

    // Wrap newly created Jolt bodies with typed wrappers (Box/Sphere/etc.)
    // determined by reading shape->GetSubType() from the restored Jolt body.
    JPH::BodyIDVector afterIds;
    _physicsSystem->GetBodies(afterIds);
    JPH::BodyInterface& bi = _physicsSystem->GetBodyInterface();
    int32_t count = 0;
    for (const JPH::BodyID& bid : afterIds) {
      if (bid.IsInvalid()) continue;
      uint32_t rawId = bid.GetIndexAndSequenceNumber();
      if (existing.count(rawId)) continue;

      JPH::Body* joltBody = nullptr;
      JPH::EShapeSubType subType = JPH::EShapeSubType::Box;
      {
        JPH::BodyLockWrite lock(_physicsSystem->GetBodyLockInterface(), bid);
        if (!lock.Succeeded()) continue;
        joltBody = &lock.GetBody();
        subType = joltBody->GetShape()->GetSubType();
      }

      Body* wrapper = nullptr;
      switch (subType) {
        case JPH::EShapeSubType::Sphere:   wrapper = new Sphere(_nenv);   break;
        case JPH::EShapeSubType::Triangle: wrapper = new Triangle(_nenv); break;
        case JPH::EShapeSubType::Capsule:  wrapper = new Capsule(_nenv);  break;
        default:                           wrapper = new Box(_nenv);      break;
      }
      wrapper->setJoltBody(joltBody);
      wrapper->setJoltBodyInterface(&bi);
      _bodiesManager->trackBody(rawId, wrapper);
      count++;
    }
    return count;
  }

  void World::setSpeed(float speed) {
    _speed = speed;
    _targetFrame = 1000.0 / (60.0 * speed);
  }

  void World::toState(WorldState next) {
    _state = next;
  }

  WorldState World::togglePhysics(float speed) {
    if (_state == WorldState::Run) {
      stopPhysics();
      return WorldState::Stop;
    }
    if (_state == WorldState::Stop) {
      runPhysics(speed);
      return WorldState::Run;
    }
    return WorldState::Stop;
  }

  void World::runPhysics(float speed) {
    if (_state == WorldState::Run) return;
    setSpeed(speed);
    toState(WorldState::Run);
  }

  void World::stepPhysics() {
    if (_state != WorldState::Step) return;
    _stepRequested = true;
  }

  void World::stopPhysics() {
    if (_state == WorldState::Stop) return;
    toState(WorldState::Stop);
  }

  WorldState World::state() const {
    return _state;
  }

  float World::speed() const {
    return _speed;
  }

  float World::fps() const {
    return _fps;
  }

  void World::worldRun(float speed, bool withPhysics) {
    if (_worldSpins) return;
    setSpeed(speed);
    _worldSpins = true;
    if (withPhysics)
      runPhysics(_speed);
    worldMain();
  }

  void World::worldMain() {
    using Clock = std::chrono::steady_clock;
    int fpsAccumulator = 0;
    double secondAccumulator = 0;
    auto last = Clock::now();
    while (_worldSpins) {
      auto frameStart = Clock::now();
      double deltaTimeMs = std::chrono::duration<double, std::milli>(frameStart - last).count();
      last = frameStart;
      // --- loop with delta
      worldLoop(deltaTimeMs);
      // --- server side fps
      secondAccumulator += deltaTimeMs;
      fpsAccumulator++;
      if (secondAccumulator >= 1000.0) {
          _fps = fpsAccumulator;
          fpsAccumulator = 0;
          secondAccumulator -= 1000.0;
      }
      // --- frame limiting ---
      auto frameEnd = Clock::now();
      double frameTimeMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
      double msLeft = _targetFrame - frameTimeMs;
      if (msLeft > 2.0) {
          std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(msLeft - 1.0));
      }
      while (std::chrono::duration<double, std::milli>(Clock::now() - frameStart).count() < _targetFrame) {
          std::this_thread::yield();
      }
    }
    // world stop spining
  }

  void  World::worldLoop(double deltaMs) {
    if(_state == WorldState::Stop) return;
    if (_state == WorldState::Step && !_stepRequested) return;
    if (_state == WorldState::Step) {
      _physicsSystem->Update(1.0/60.0, 1, _tempAllocator, _jobSystem);
      _step++;
      _stepRequested = false;
      _bodiesManager->update(1);
      return;
    }
    if (_state != WorldState::Run) return;
    int frames = std::round(deltaMs / _targetFrame);
    if (frames < 1) return;
    _physicsSystem->Update(deltaMs / 1000.0, frames, _tempAllocator, _jobSystem);
    _step += frames;
    _bodiesManager->update(frames);
  }

  void World::worldStop() {
    _worldSpins = false;
    if (_state == WorldState::Stop) return;
    stopPhysics();
    toState(WorldState::Stop);
  }

  void World::reset() {
    worldStop();
    _bodiesManager->reset();
  }
}

static JOLT::AutoRegister _auto_reg_world(JOLT::World::Init);
