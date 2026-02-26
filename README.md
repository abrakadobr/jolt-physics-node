# jolt-physics-node

[![npm version](https://img.shields.io/npm/v/jolt-physics-node)](https://www.npmjs.com/package/jolt-physics-node)
[![npm downloads](https://img.shields.io/npm/dm/jolt-physics-node)](https://www.npmjs.com/package/jolt-physics-node)
[![license](https://img.shields.io/npm/l/jolt-physics-node)](./LICENSE)
[![node](https://img.shields.io/node/v/jolt-physics-node)](https://www.npmjs.com/package/jolt-physics-node)

Node.js native binding for [Jolt Physics](https://github.com/jrouwe/JoltPhysics) via Node-API (N-API).
Runs entirely on the server — no WebAssembly, no browser target.

> **Russian docs:** [README.ru.md](./README.ru.md)

---

## Features

- Rigid body simulation — spheres, boxes, capsules, cylinders, convex hulls, meshes, height fields, compound shapes
- Full constraint system — hinge, slider, point, cone, fixed, distance, swing-twist, 6DOF, gear, pulley, rack-and-pinion, path
- Constraint motors with spring/damping parameters
- Spatial queries — ray casts, sphere collides/sweeps, AABB overlap
- Per-triangle / per-cell material indices (friction, restitution) on mesh and height field shapes
- Skeleton + Ragdoll system with per-joint shapes and constraints
- Compact binary state snapshots for network sync / replay
- Full scene save/load (Jolt `PhysicsScene` format)
- `PhysicsWorker` — runs a world in a dedicated Worker thread with async API
- Contact and body-activation event callbacks

---

## Requirements

- Node.js 18+
- C++17 toolchain (`g++` or `clang++`, `make`, Python 3 for `node-gyp`)

---

## Installation

**Via npm** (recommended):

```bash
npm install jolt-physics-node
```

The native addon is compiled automatically during install. No prebuilt binaries —
a C++ toolchain is required (see Requirements above).

**From source** (for development / contributing):

```bash
git clone --recurse-submodules https://github.com/abrakadobr/jolt-physics-node
cd jolt-physics-node
npm install
```

If you cloned without `--recurse-submodules`:

```bash
git submodule update --init
npm install
```

---

## Quick Start

```js
const { World } = require('jolt-physics-node');

const world = new World({ gravity: 9.81 });

const ball = world.createSphere({
  radius: 0.5,
  position: { x: 0, y: 5, z: 0 },
  dynamic: true,
  restitution: 0.4,
  friction: 0.5,
});

for (let i = 0; i < 120; i++) world.step(1 / 60);

console.log(world.getBodyPosition(ball)); // y ≈ 0

world.destroy();
```

---

## API Reference

### World

```js
const world = new World({ gravity?: number });  // default gravity = 9.81 m/s²
world.step(dt);          // advance simulation
world.setGravity(9.81);
world.destroy();         // free all resources
```

Every `World` creates a static ground plane at `y = -1` automatically.

---

### Body creation

All creation methods return a numeric `BodyId`. Common options for every shape:

| Option | Type | Default | Description |
|---|---|---|---|
| `position` | `Vec3` | required | World-space position |
| `dynamic` | `boolean` | `true` | `false` = static body |
| `friction` | `number` | `0.5` | |
| `restitution` | `number` | `0.2` | |

```js
world.createSphere({ radius, position, dynamic?, friction?, restitution? })
world.createBox({ halfExtents: Vec3, position, ... })
world.createCapsule({ halfHeight, radius, position, ... })
world.createCylinder({ halfHeight, radius, position, ... })
world.createTaperedCapsule({ halfHeight, topRadius, bottomRadius, position, ... })
world.createTaperedCylinder({ halfHeight, topRadius, bottomRadius, position, ... })
world.createConvexHull({ points: number[], position, ... })  // flat [x,y,z,...], ≥4 verts
```

**Mesh** (static only):

```js
world.createMesh({
  vertices: number[],   // flat [x,y,z, ...]
  indices:  number[],   // triangle list [i0,i1,i2, ...]
  position: Vec3,
  friction?: number,
  restitution?: number,
  materialIndices?: number[] | Uint32Array,  // one index per triangle
  materials?: Array<{ friction?: number, restitution?: number }>,
})
```

**Height field** (static terrain):

```js
world.createHeightField({
  samples: Float32Array | number[],  // N×N values, row-major
  sampleCount: number,               // N (≥2, e.g. 64)
  offset?: Vec3,
  scale?: Vec3,
  position?: Vec3,
  friction?: number,
  restitution?: number,
  materialIndices?: Uint8Array | number[],  // one per cell
  materials?: Array<{ friction?: number, restitution?: number }>,
})
```

**Compound shapes:**

```js
// Static compound — multiple shapes baked into one body
world.createStaticCompound({
  shapes: SubShapeSpec[],  // see below
  position: Vec3,
  dynamic?: boolean,
  friction?: number,
  restitution?: number,
})

// Mutable compound — sub-shapes can be changed at runtime
const id = world.createMutableCompound({ shapes, position, dynamic?, ... })
const idx = world.addMutableSubShape(id, spec)
world.removeMutableSubShape(id, idx)
world.modifyMutableSubShape(id, idx, position, rotation?)
world.adjustMutableCenterOfMass(id)  // call after any modification
```

`SubShapeSpec` — `{ kind: 'sphere'|'box'|'capsule'|'cylinder', position?, rotation?, radius?, halfHeight?, halfExtents? }`

---

### Body state

```js
// Position / rotation
world.getBodyPosition(id)              // → Vec3
world.getBodyRotation(id)              // → Quat
world.getCenterOfMassPosition(id)      // → Vec3
world.setBodyPosition(id, pos, activate = true)
world.setBodyRotation(id, quat, activate = true)

// Velocity
world.getLinearVelocity(id)            // → Vec3 (m/s)
world.getAngularVelocity(id)           // → Vec3 (rad/s)
world.setLinearVelocity(id, vec3)
world.setAngularVelocity(id, vec3)

// Forces & impulses
world.applyImpulse(id, vec3)           // kg·m/s
world.addAngularImpulse(id, vec3)
world.addForce(id, vec3)               // N (cleared each step)
world.addTorque(id, vec3)              // N·m

// Material properties
world.getFriction(id) / world.setFriction(id, v)
world.getRestitution(id) / world.setRestitution(id, v)

// Dynamics
world.getMotionType(id) / world.setMotionType(id, type, activate?)
// type: 0=Static, 1=Kinematic, 2=Dynamic
world.getMotionQuality(id) / world.setMotionQuality(id, quality)
// quality: 0=Discrete, 1=LinearCast (CCD)
world.getGravityFactor(id) / world.setGravityFactor(id, v)  // 0 = no gravity
world.getObjectLayer(id) / world.setObjectLayer(id, layer)  // 0=NON_MOVING, 1=MOVING
world.getDamping(id) / world.setDamping(id, { linear, angular })

// Lifecycle
world.activateBody(id)
world.deactivateBody(id)
world.removeBody(id)

// Predicates (return boolean, never throw)
world.hasBody(id)
world.isBodyActive(id)
world.isBodySensor(id)
world.areBodiesInContact(idA, idB)
world.setBodySensor(id, bool)          // trigger volume — detects overlaps, no forces
```

---

### Spatial queries

All query methods accept an optional `filter`:

```js
filter?: {
  layerMask?: number,         // bitmask — bit N = layer N allowed (default all)
  excludeBodyIds?: number[],  // ignore specific bodies
}
```

```js
// Closest ray hit — returns null on miss
const hit = world.rayCastClosest({
  origin: Vec3, direction: Vec3, maxDistance: number, filter?
})
// → { bodyId, fraction, normal: Vec3, materialIndex } | null

// All ray hits sorted by distance
const hits = world.rayCastAll({ origin, direction, maxDistance, filter? })
// → [{ bodyId, fraction, normal, materialIndex }]

// Bodies overlapping a sphere
const overlaps = world.collideSphereAll({
  center: Vec3, radius: number, maxSeparation?: number, filter?
})
// → [{ bodyId, contactPoint: Vec3, penetrationDepth, normal: Vec3, materialIndex }]

// Sweep a sphere along a ray
const sweeps = world.castSphereAll({
  origin: Vec3, direction: Vec3, maxDistance: number, radius: number, filter?
})
// → [{ bodyId, fraction, penetrationDepth, point: Vec3, normal: Vec3, materialIndex }]

// AABB overlap — returns body IDs only
const bodies = world.queryAABB({ min: Vec3, max: Vec3, filter? })
// → [{ bodyId }]
```

`materialIndex` is 0 when no per-triangle material was assigned, otherwise it is the index into the `materials` array provided at shape creation time.

---

### Constraints

All creators return a numeric `ConstraintId`. Invalid IDs throw in setters/getters.

```js
world.createFixedConstraint(bodyA, bodyB)

world.createDistanceConstraint(bodyA, bodyB, pointA, pointB, minDist?, maxDist?)
world.setDistanceLimits(id, min, max)
world.getDistanceLimits(id)            // → { min, max }
world.setDistanceLimitsSpring(id, springOpts)
world.getDistanceLimitsSpring(id)      // → SpringSettings

world.createHingeConstraint(bodyA, bodyB, anchor, axis, normal)
world.setHingeLimits(id, minRad, maxRad)
world.getHingeLimits(id)               // → { min, max }
world.getHingeAngle(id)                // → radians
world.setHingeMotor(id, motorOpts)
world.getHingeMotorState(id)

world.createSliderConstraint(bodyA, bodyB, anchor, axis, normal, min?, max?)
world.setSliderLimits(id, min, max)
world.getSliderLimits(id)              // → { min, max }
world.getSliderPosition(id)            // → metres
world.setSliderMotor(id, motorOpts)
world.getSliderMotorState(id)

world.createPointConstraint(bodyA, bodyB, pivotWorld)

world.createConeConstraint(bodyA, bodyB, pivot, twistAxis, halfConeAngle)
world.setConeHalfAngle(id, radians)

world.createSwingTwistConstraint(bodyA, bodyB, pivot, twistAxis, planeAxis, limits?)
// limits: { normalHalfCone, planeHalfCone, twistMin, twistMax }
world.setSwingTwistLimits(id, limits)
world.getSwingTwistLimits(id)
world.getSwingTwistRotation(id)        // → Quat
world.setSwingTwistMotor(id, motorOpts)
world.getSwingTwistMotorState(id)

world.createSixDOFConstraint(bodyA, bodyB, pivot, axisX, axisY)
world.setSixDOFLimits(id, {
  translationMin: Vec3, translationMax: Vec3,
  rotationMin: Vec3, rotationMax: Vec3,
})
world.getSixDOFLimits(id)
world.getSixDOFRotation(id)            // → Vec3 (Euler)
world.setSixDOFMotorState(id, axis, state)   // axis 0–5
world.getSixDOFMotorState(id, axis)
world.setSixDOFTargetVelocity(id, linVec3, angVec3)
world.setSixDOFTargetPose(id, posVec3, rotQuat)

world.createGearConstraint(bodyA, bodyB, hingeAxis1, hingeAxis2,
                            ratio?, hinge1Id?, hinge2Id?)

world.createPulleyConstraint(bodyA, bodyB,
  bodyPoint1, fixedPoint1, bodyPoint2, fixedPoint2,
  { ratio?, minLength?, maxLength? })
world.getPulleyLength(id)              // → current total rope length
world.getPulleyLengthLimits(id)        // → { min, max }
world.setPulleyLength(id, min, max)

world.createRackAndPinionConstraint(bodyA, bodyB, {
  hingeAxis, sliderAxis, ratio,
  pinionConstraintId?, rackConstraintId?,
})

world.createPathConstraint(bodyA, bodyB, {
  points: Array<{ position, tangent, normal }>,  // Vec3 each
  closed?, pathPosition?, pathRotation?,
  pathFraction?, maxFriction?,
  rotationType?: 'free'|'tangent'|'normal'|'binormal'|'toPath'|'full',
})
world.getPathFraction(id)             // → 0 to N-1
world.getPathMaxFraction(id)          // → N-1
world.setPathMotor(id, { state, targetVelocity?, targetFraction? })
world.getPathMotorState(id)

world.removeConstraint(id)
```

#### Motor options

```js
{
  state: 0 | 1 | 2,       // 0=Off, 1=Velocity, 2=Position
  targetVelocity?: number,
  targetAngle?: number,    // Hinge — radians
  targetPosition?: number, // Slider — metres
  maxTorque?: number,
  maxForce?: number,
}
```

#### Motor spring / damping

Available on: Hinge, Slider, SwingTwist (swing + twist separately), SixDOF (per axis), Path, Distance limits.

```js
world.setHingeMotorSpring(id, {
  mode?: 0 | 1,       // 0=FrequencyAndDamping (default), 1=StiffnessAndDamping
  frequency?: number, // Hz  (mode 0)
  stiffness?: number, // N/m (mode 1)
  damping?: number,
  maxTorque?: number,
  minTorque?: number,
  maxForce?: number,
  minForce?: number,
})

world.getHingeMotorSpring(id)
// → { mode, frequency, damping, minForceLimit, maxForceLimit, minTorqueLimit, maxTorqueLimit }

// Same pattern for:
world.setSliderMotorSpring(id, opts)
world.setSwingMotorSpring(id, opts)
world.setTwistMotorSpring(id, opts)
world.setSixDOFMotorSpring(id, axis, opts)  // axis 0–5
world.setPathMotorSpring(id, opts)
world.setDistanceLimitsSpring(id, opts)
```

#### Constraint lambda (impulse) getters

Impulses applied by a constraint in the last simulation step — useful for stress monitoring.

```js
world.getHingeLambdas(id)      // → { position: Vec3, rotation: Vec2, rotationLimits, motor }
world.getSliderLambdas(id)     // → { position: Vec2, positionLimits, rotation: Vec3, motor }
world.getSwingTwistLambdas(id) // → { position: Vec3, twist, swingY, swingZ, motor: Vec3 }
world.getSixDOFLambdas(id)     // → { position, rotation, motorTranslation, motorRotation } (all Vec3)
world.getConeLambdas(id)       // → { position: Vec3, rotation }
world.getPointLambdas(id)      // → { position: Vec3 }
world.getFixedLambdas(id)      // → { position: Vec3, rotation: Vec3 }
world.getDistanceLambda(id)    // → number
world.getPulleyLambda(id)      // → number
world.getGearLambda(id)        // → number
world.getPathLambdas(id)       // → { position: Vec2, positionLimits, motor, rotationHinge: Vec2, rotation: Vec3 }
```

---

### Skeleton & Ragdoll

```js
const skeleton = world.createSkeleton();
skeleton.addJoint('root', -1);      // name, parentIndex (-1 = root)
skeleton.addJoint('spine', 0);
skeleton.finalize();

skeleton.getJointCount()
skeleton.getJointIndex('spine')     // → number | -1
skeleton.getJointInfo(0)            // → { name, parentIndex }

const settings = skeleton.createRagdollSettings({
  capsuleHalfHeight?: number,   // default 0.2
  capsuleRadius?: number,       // default 0.1
  spacing?: number,             // distance between joints, default 0.4
});

// Per-joint overrides
settings.setJointShape(index, {
  kind: 'capsule' | 'box' | 'sphere',
  halfHeight?, radius?, halfExtents?
})
settings.setJointTransform(index, position, rotation)
settings.setJointConstraint(index, {
  type: 'fixed' | 'swingTwist' | 'hinge' | 'cone',
  autoAxes?: boolean,          // auto-compute axes from bone direction (default true)
  // SwingTwist:
  normalHalfCone?, planeHalfCone?, twistMin?, twistMax?,
  // Hinge:
  minAngle?, maxAngle?,
  // Cone:
  halfConeAngle?,
})

const ragdoll = settings.createRagdoll({
  collisionGroup?: number,
  userData?: number,
  activate?: boolean,
})

ragdoll.bodyCount()
ragdoll.getBoneBodyId(index)          // → BodyId — use with world.* methods
ragdoll.getBoneTransform(index)       // → { position: Vec3, rotation: Quat }
ragdoll.setBoneTransform(index, transform)
ragdoll.syncToSkeletonPose()          // → Transform[]
ragdoll.syncFromSkeletonPose(poses)
ragdoll.getConstraintIds()            // → number[] — use with setSwingTwistMotor, etc.
ragdoll.destroy()
```

---

### Events

```js
world.onBodyActivation((event) => {
  // event: { type: 'activated'|'deactivated', bodyId }
})

world.onContact((event) => {
  // event.type: 'added' | 'persisted' | 'removed'
  // For 'added' / 'persisted': + bodyA, bodyB, point, normal, penetrationDepth
  // For 'removed':             + bodyA, bodyB
})
```

---

### Serialization

```js
// Compact snapshot: position + rotation + velocities for all bodies
// 56 bytes per body — suitable for network sync, interpolation
const buf = world.snapshotState()      // → Buffer
world.applySnapshot(buf)               // apply to existing bodies by ID

// Full scene (Jolt PhysicsScene format)
// Includes shape geometry + current transform + velocities
// Note: custom constraints are NOT included — recreate them after loadScene()
const sceneBuf = world.saveScene()     // → Buffer
const count = world.loadScene(buf)     // → number of bodies restored
```

---

### PhysicsWorker

Runs a `World` in a dedicated `worker_threads` thread. All `World` methods are available as `async` equivalents.

```js
const { PhysicsWorker } = require('jolt-physics-node');

const worker = await PhysicsWorker.create(
  { gravity: 9.81 },        // World options
  { autoState: true },      // emit snapshot after every step (default true)
);

worker.onState((snapshot) => {
  // Buffer — same 56-bytes/body format as snapshotState()
  // zero-copy transfer from worker thread
});

worker.onEvent((kind, data) => {
  // kind: 'bodyActivation' | 'contact'
});

const ballId = await worker.createSphere({ radius: 0.5, position: {x:0,y:5,z:0} });
await worker.step(1 / 60);

const pos = await worker.getBodyPosition(ballId);
await worker.applyImpulse(ballId, { x: 0, y: 10, z: 0 });

const hit = await worker.rayCastClosest({
  origin: {x:0,y:10,z:0}, direction: {x:0,y:-1,z:0}, maxDistance: 30
});

// Serialization
const buf = await worker.snapshotState();
await worker.applySnapshot(buf);

await worker.terminate();   // sends 'destroy' with 2s timeout, rejects pending calls
```

All `World` methods are available as async methods on `PhysicsWorker` with identical signatures.

---

### Error handling

| Situation | Behaviour |
|---|---|
| Invalid `bodyId` / `constraintId` in setter or action | throws `Error` |
| Invalid `bodyId` / `constraintId` in getter | throws `Error` |
| `rayCastClosest` — no hit | returns `null` |
| `rayCastAll`, `collideSphereAll`, etc. — no hits | returns `[]` |
| `hasBody`, `isBodyActive`, `isBodySensor`, `areBodiesInContact` | returns `boolean`, never throws |

---

## Examples

```bash
npm run examples
```

Opens an interactive physics demo at `http://127.0.0.1:8787` with scenarios:
falling spheres, constraints, convex hulls, mesh bodies, ragdoll skeleton.

---

## Building from source

```bash
npm run build    # node-gyp rebuild
npm test         # smoke test
```

JoltPhysics is included as a git submodule — no separate installation needed.

---

## License

MIT
