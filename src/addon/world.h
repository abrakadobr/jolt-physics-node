#pragma once

#include "includes.h"
#include "commands.h"
#include "events.h"
#include "structs.h"
#include "jolt.h"
#include "listners.h"
#include "layers/layers_manager.h"

namespace JOLT {

class World {
public:

  World();
  ~World();

  void setSpeed(float speed);
  void start(const CommandBase &cmd, float speed = 1.0);
  void stop(const CommandBase &cmd);
  void step(const CommandBase &cmd);
  void shutdown(const CommandBase &cmd);
  inline std::thread * loopThread() { return &_thread; };

  void enqueue(JCommand cmd);

  void setTsfn(napi_threadsafe_function tsfn);
  // inline napi_threadsafe_function getTsfn() { return _tsfn; };
  void run();
  void runJolt();
  void collapse();
  void destroyJolt();

  void configure(const CommandInit &cmd);

  void addBody(const CommandBody &cmd);
  void removeBody(const CommandBody &cmd);
  void activateBody(const CommandBody &cmd);
  void deactivateBody(const CommandBody &cmd);
  void destroyBody(const CommandBody &cmd);

  void createBox(const CommandCreateBox &cmd);
  void createSphere(const CommandCreateSphere &cmd);
  void setBodyPosition(const CommandSetPosition &cmd);
  void setBodyRotation(const CommandSetRotation &cmd);
private:
  void loop();
  void processCommands();
  void executeCommand(JCommand cmd);
  void worldLoop(double deltaMs);
  void processEvents();
  void emit(JEvent ev);
private:
  std::thread _thread;
  std::atomic<bool> _running = false;
  std::atomic<bool> _started = false;
  bool              _joltCreated = false;

  std::queue<JCommand> _commands;
  std::queue<JEvent> _events;
  std::mutex _cmdMutex;
  std::mutex _evtMutex;

  std::atomic<napi_threadsafe_function>      _tsfn{nullptr};

  std::atomic<float>                         _speed = 1.0;
  float                         _fps = 0.0;

  double                        _targetFrame = 1.0/60.0;

  uint64_t                      _step = 0;
  bool                          _worldSpins = false;
  bool                          _stepRequested = false; // = true for Step mode on request to do next step
  bool                          _shutdown = false;

  WorldConfig                   _config;


  JPH::TempAllocatorImpl        * _tempAllocator = nullptr;
  JPH::JobSystemThreadPool      * _jobSystem = nullptr;
  JPH::PhysicsSystem            * _physicsSystem = nullptr;

  EngineBodyActivationListener  * _bodyActivationListner = nullptr;
  EngineContactListener         * _contactListner = nullptr;

  LayersManager                 * _layersManager = nullptr;
};

}
