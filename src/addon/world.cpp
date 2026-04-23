#include "world.h"

namespace JOLT {


World::World() {
  run();
}

World::~World() {
  CommandBase c;
  c.commandId = 0;
  stop(c);
  collapse();
}


void World::setTsfn(napi_threadsafe_function tsfn) {
  _tsfn.store(tsfn, std::memory_order_relaxed);
}

void World::setSpeed(float speed) {
  _speed.store(speed, std::memory_order_relaxed);
  float sp = _speed.load(std::memory_order_acquire);
  _targetFrame = 1000.0 / (60.0 * sp);
}

void World::run() {
  if (_running) return;
  _running = true;
  setSpeed(1.0);
  _thread = std::thread([this]() { loop(); });
}

void World::runJolt() {
  if (_joltCreated) return;
  _joltCreated = true;
  // std::cout << "[W@J!!]" << std::endl;
  _layersManager = new LayersManager();
  // _layersManager->init();

  JPH::RegisterDefaultAllocator();
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();

  _tempAllocator = new JPH::TempAllocatorImpl(_config.memoryPreallocatedMb * 1024 * 1024);
  _jobSystem = new JPH::JobSystemThreadPool(
    JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, // comes from Jolt/Physics/PhysicsSettings.h
    std::max(1u, std::thread::hardware_concurrency() - 1)
  );
  _physicsSystem = new JPH::PhysicsSystem();
  _physicsSystem->SetGravity(JPH::Vec3(0.0f, -_config.gravity, 0.0f));
  _physicsSystem->Init(
    _config.maxBodies,
    _config.numBodyMutexes,
    _config.maxBodiesPairs,
    _config.maxContacts,
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
  _bodyInterface = &_physicsSystem->GetBodyInterface();
}

void World::start(const CommandBase &cmd) {
  // setSpeed(speed);
  if (!_joltCreated) runJolt();
  if (_started) return;
  _started = true;
  emit( EGen::Start(cmd.commandId, true) );
}

void World::stop(const CommandBase &cmd) {
  _started = false;
  emit( EGen::Stop(cmd.commandId, true) );
}

void World::step(const CommandBase &cmd) {
  // _started = false;
}

void World::shutdown(const CommandBase &cmd) {
  stop(cmd);
  collapse();
  emit( EGen::Shutdown(cmd.commandId, true) );
}

void World::collapse() {
  if (_joltCreated) destroyJolt();
  if (!_running) return;
  _running = false;
}

void World::destroyJolt() {
  if (!_joltCreated) return;
  // if (_bodiesManager) delete _bodiesManager;
  if (_physicsSystem) delete _physicsSystem;
  if (_contactListner)  delete _contactListner;
  if (_bodyActivationListner) delete _bodyActivationListner;
  if (_jobSystem) delete _jobSystem;
  if (_tempAllocator) delete _tempAllocator;
  if (_layersManager) delete _layersManager;
  _joltCreated = false;
}

void World::enqueue(JCommand cmd) {
  std::lock_guard<std::mutex> lock(_cmdMutex);
  _commands.push(std::move(cmd));
}

void World::processCommands() {
  std::queue<JCommand> local;

  {
    std::lock_guard<std::mutex> lock(_cmdMutex);
    std::swap(local, _commands);
  }
  while (!local.empty()) {
    // std::cout << "ProcessCommands" << local.empty() << std::endl;
    executeCommand(local.front());
    local.pop();
  }
}

void World::executeCommand(JCommand jcmd) {
  std::visit([this](auto&& cmd) {
    using T = std::decay_t<decltype(cmd)>;

    if constexpr (std::is_same_v<T, CommandBase>) {
      if (cmd.cmd == Commands::Start) return start(cmd);
      if (cmd.cmd == Commands::Stop) return stop(cmd);
      if (cmd.cmd == Commands::Step) return step(cmd);
      if (cmd.cmd == Commands::Shutdown) return shutdown(cmd);
    } else if constexpr (std::is_same_v<T, CommandInit>) {
      if (cmd.cmd == Commands::Init) return configure(cmd);
    // } else if constexpr (std::is_same_v<T, CommandGetLayers>) {
      // return _layersManager->getLayers(cmd);
    } else if constexpr (std::is_same_v<T, CommandBodyAdd>) {
      return addBody(cmd);
    } else if constexpr (std::is_same_v<T, CommandBodyDestroy>) {
      return destroyBody(cmd);
    } else if constexpr (std::is_same_v<T, CommandBody>) {
      if (cmd.cmd == Commands::RemoveBody) return removeBody(cmd);
      if (cmd.cmd == Commands::ActivateBody) return activateBody(cmd);
      if (cmd.cmd == Commands::DeactivateBody) return deactivateBody(cmd);
    } else if constexpr (std::is_same_v<T, CommandBodyCreate>) {
      if (cmd.cmd == Commands::CreateBody) return createBody(cmd);
    } else if constexpr (std::is_same_v<T, CommandSetPosition>) {
      if (cmd.cmd == Commands::SetPosition) return setBodyPosition(cmd);
    } else if constexpr (std::is_same_v<T, CommandSetRotation>) {
      if (cmd.cmd == Commands::SetRotation) return setBodyRotation(cmd);
    }
  }, jcmd);
}

void World::configure(const CommandInit &cmd) {
  // std::cout << "configure" << cmd.commandId << std::endl;
  bool wasStarted = _started.load();
  if (wasStarted) {
    stop(cmd);
    if (_joltCreated) destroyJolt();
  }
  _config = cmd.config;
  emit( EGen::Init(cmd.commandId, true) );
  if (wasStarted) start(cmd);
}

void World::processEvents() {
  napi_threadsafe_function tsfn = _tsfn.load(std::memory_order_relaxed);
  if (!tsfn) return;
  std::queue<JEvent> local;

  {
    std::lock_guard<std::mutex> lock(_evtMutex);
    std::swap(local, _events);
  }

  if (local.empty()) return;
  napi_call_threadsafe_function(tsfn, new std::queue<JEvent>(std::move(local)), napi_tsfn_nonblocking);
}

void World::loop() {
  using namespace std::chrono;
  using Clock = std::chrono::steady_clock;

  int fpsAccumulator = 0;
  double secondAccumulator = 0;
  auto last = Clock::now();

  runJolt();
  while (_running) {
    auto frameStart = Clock::now();
    double deltaTimeMs = std::chrono::duration<double, std::milli>(frameStart - last).count();
    last = frameStart;
    processCommands();
    // --- loop with delta
    worldLoop(deltaTimeMs);
    // --- server side fps
    secondAccumulator += deltaTimeMs;
    fpsAccumulator++;
    if (secondAccumulator >= 1000.0) {
        _fps = fpsAccumulator;
        fpsAccumulator = 0;
        secondAccumulator -= 1000.0;
        emit(EGen::FPS(_fps));
    }
    processEvents();
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
  napi_threadsafe_function tsfn = _tsfn.load(std::memory_order_acquire);
  napi_release_threadsafe_function(tsfn, napi_tsfn_release);
  _tsfn.store(nullptr, std::memory_order_relaxed);
}

void World::worldLoop(double deltaMs) {
    if (!_started) return;
    if (_doOptimize)
      _physicsSystem->OptimizeBroadPhase();
    int collisionSteps = std::round(deltaMs/_targetFrame);
    float dms = deltaMs / 1000.0f;
    _physicsSystem->Update(dms, collisionSteps, _tempAllocator, _jobSystem);
    for (const JPH::BodyID &bid: _activeBodies) {
      JPH::RMat44 transform = _bodyInterface->GetWorldTransform(bid);
      emit(EGen::BodyTransform(bid.GetIndexAndSequenceNumber(), transform));
    }
    // std::cout << "." << deltaMs << " - " << collisionSteps; // << std::endl;
}

void World::emit(JEvent ev) {
  std::lock_guard<std::mutex> lock(_evtMutex);
  _events.push(ev);
}

void World::addBody(const CommandBodyAdd &cmd) {
  JPH::BodyID bid(cmd.bodyId);
  JPH::EActivation mode = cmd.activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
  _bodyInterface->AddBody(bid, mode);
  bool ok = _bodyInterface->IsAdded(bid);
  _doOptimize = true;
  // std::cout << "[add body]" << cmd.bodyId << ok << std::endl;
  emit(EGen::BodyAdded(cmd.commandId, cmd.bodyId, ok));
}
void World::removeBody(const CommandBody &cmd) {
  // std::cout << "[remove body]" << cmd.bodyId << std::endl;
  JPH::BodyID bid(cmd.bodyId);
  _bodyInterface->RemoveBody(bid);
  bool exists = _bodyInterface->IsAdded(bid);
  emit(EGen::BodyRemoved(cmd.commandId, cmd.bodyId, !exists));
}
void World::activateBody(const CommandBody &cmd) {
  // std::cout << "[activate body]" << cmd.bodyId << std::endl;
  JPH::BodyID bid(cmd.bodyId);
  // JPH::EActivation mode = cmd.activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
  _bodyInterface->ActivateBody(bid);
  _activationCommands[cmd.bodyId] = cmd.commandId;
  _doOptimize = true;
  // bool active = _bodyInterface->IsActive(bid);
  // emit(EGen::BodyActivated(cmd.commandId, cmd.bodyId, active));
}
void World::deactivateBody(const CommandBody &cmd) {
  // std::cout << "[deactivate body]" << cmd.bodyId << std::endl;
  JPH::BodyID bid(cmd.bodyId);
  // JPH::EActivation mode = cmd.activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
  _bodyInterface->DeactivateBody(bid);
  _activationCommands[cmd.bodyId] = cmd.commandId;
  _doOptimize = true;
  // bool active = _bodyInterface->IsActive(bid);
  // emit(EGen::BodyDeactivated(cmd.commandId, cmd.bodyId, !active));
}


void World::onBodyActivate(const JPH::BodyID &bodyId, uint64_t userData) {
  uint64_t commandId = 0;
  _activeBodies.push_back(bodyId);
  _doOptimize = true;
  std::cout << "World::onBodyActivate" << bodyId.GetIndexAndSequenceNumber() << std::endl;
  if (_activationCommands.count(bodyId.GetIndexAndSequenceNumber())) {
    commandId = _activationCommands[bodyId.GetIndexAndSequenceNumber()];
    _activationCommands.erase(bodyId.GetIndexAndSequenceNumber());
  }
  emit(EGen::BodyActivated(commandId, bodyId.GetIndexAndSequenceNumber(), true));
}
void World::onBodyDeactivate(const JPH::BodyID &bodyId, uint64_t userData) {
  std::cout << "World::onBodyDeactivate" << bodyId.GetIndexAndSequenceNumber() << std::endl;
  _doOptimize = true;
  _activeBodies.erase(std::remove(_activeBodies.begin(), _activeBodies.end(), bodyId), _activeBodies.end());
  // std::erase(_activeBodies, bodyId);
  uint64_t commandId = 0;
  if (_activationCommands.count(bodyId.GetIndexAndSequenceNumber())) {
    commandId = _activationCommands[bodyId.GetIndexAndSequenceNumber()];
    _activationCommands.erase(bodyId.GetIndexAndSequenceNumber());
  }
  emit(EGen::BodyDeactivated(commandId, bodyId.GetIndexAndSequenceNumber(), true));
}


void World::destroyBody(const CommandBodyDestroy &cmd) {
  // std::cout << "[destroy body]" << cmd.bodyId << std::endl;
  JPH::BodyID bid(cmd.bodyId);
  _doOptimize = true;
  bool exists = _bodyInterface->IsAdded(bid);
  if (exists) {
    if (!cmd.force) {
      emit(EGen::BodyDestroyed(cmd.commandId, cmd.bodyId, false));
      return;
    }
    bool active = _bodyInterface->IsActive(bid);
    if (active) {
      _bodyInterface->DeactivateBody(bid);
      active = _bodyInterface->IsActive(bid);
      if (active) {
        emit(EGen::BodyDestroyed(cmd.commandId, cmd.bodyId, false));
        return;
      }
      emit(EGen::BodyDeactivated(cmd.commandId, cmd.bodyId, !active));
    }
    _bodyInterface->RemoveBody(bid);
    exists = _bodyInterface->IsAdded(bid);
    if (exists) {
      emit(EGen::BodyDestroyed(cmd.commandId, cmd.bodyId, false));
      return;
    }
    emit(EGen::BodyRemoved(cmd.commandId, cmd.bodyId, false));
  }
  _bodyInterface->DestroyBody(bid);
  emit(EGen::BodyDestroyed(cmd.commandId, cmd.bodyId, true));
}

void World::createBody(const CommandBodyCreate &cmd) {
  _doOptimize = true;
    BodyCreationSettings params = cmd.params;
    JPH::Shape * inShape;
    JSubShape jshape = params.shape;
    std::visit([this, &inShape](auto&& shape) {
      using T = std::decay_t<decltype(shape)>;
      if constexpr (std::is_same_v<T, SphereSubShape>) {
        inShape = new JPH::SphereShape(shape.radius);
      } else if constexpr (std::is_same_v<T, BoxSubShape>) {
        inShape = new JPH::BoxShape(shape.halfExtend);
      }
    }, jshape);
    // std::cout << "create body " << params.position.GetX() << "/" << params.position.GetY() << "/" << params.position.GetZ() << std::endl;
    JPH::BodyCreationSettings jbcs(inShape, params.position, params.rotation, params.motionType, params.layer);
    JPH::BodyID bid; // = 0xffffffff;//JPH::BodyID::cInvalidBodyID;
    JPH::EActivation mode = JPH::EActivation::DontActivate;
    bool success = true;
    if (params.addToPhysics) {
      if (params.activate) mode = JPH::EActivation::Activate;
      bid = _bodyInterface->CreateAndAddBody(jbcs, mode);

      if (bid.GetIndexAndSequenceNumber() == 0xffffffff)
        success = false;
    } else {
      JPH::Body * b = _bodyInterface->CreateBody(jbcs);
      if (!b) {
        success = false;
      } else {
        bid = b->GetID();
      }
    }
    BodyCreationEvent e = EGen::BodyCreated(cmd.commandId, success, bid.GetIndexAndSequenceNumber(), params);
    emit(e);
}
void World::setBodyPosition(const CommandSetPosition &cmd) {
    std::cout << "[set body position]" << std::endl;
}
void World::setBodyRotation(const CommandSetRotation &cmd) {
    std::cout << "[set body rotation]" << std::endl;
}

}
