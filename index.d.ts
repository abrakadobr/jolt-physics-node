/**
 * @module jolt-physics-node
 * Node-API binding for the Jolt Physics engine.
 */

// ─── Primitive aliases ────────────────────────────────────────────────────────

/** Opaque numeric identifier returned by every body-creation method. */
export type BodyId = number;

/** Opaque numeric identifier returned by every constraint-creation method. */
export type ConstraintId = number;

// ─── Common value types ───────────────────────────────────────────────────────

/** Three-component vector used for positions, directions, velocities and forces. */
export interface Vec3 {
  x: number;
  y: number;
  z: number;
}

/** Two-component vector returned by some lambda getters. */
export interface Vec2 {
  x: number;
  y: number;
}

/** Unit quaternion used for rotations and orientations. */
export interface Quat {
  x: number;
  y: number;
  z: number;
  w: number;
}

/** Position + rotation pair (bone transform, body pose). */
export interface Transform {
  position: Vec3;
  rotation: Quat;
}

// ─── Enumerations (passed as plain numbers) ───────────────────────────────────

/**
 * Body motion type.
 * - `0` Static   — never moves, zero cost
 * - `1` Kinematic — moved by the user, not by forces
 * - `2` Dynamic   — fully simulated
 */
export type MotionType = 0 | 1 | 2;

/**
 * Motion quality (collision detection mode).
 * - `0` Discrete   — fast, may tunnel through thin objects at high speed
 * - `1` LinearCast — continuous cast, prevents tunnelling
 */
export type MotionQuality = 0 | 1;

/**
 * Constraint motor state.
 * - `0` Off      — motor disabled
 * - `1` Velocity — drive to target velocity
 * - `2` Position — drive to target angle / position
 */
export type MotorState = 0 | 1 | 2;

/**
 * Spring mode for position motors.
 * - `0` FrequencyAndDamping — specify natural frequency (Hz) and damping ratio
 * - `1` StiffnessAndDamping — specify spring constant k and damping coefficient
 */
export type SpringMode = 0 | 1;

/**
 * Path constraint rotation type. Controls how the body rotates as it moves along the path.
 * - `'free'`     / `0` — rotation unconstrained
 * - `'tangent'`  / `1` — align X-axis to path tangent
 * - `'normal'`   / `2` — align X-axis to path normal
 * - `'binormal'` / `3` — align X-axis to path binormal
 * - `'toPath'`   / `4` — rotate toward next path point
 * - `'full'`     / `5` — full Frenet–Serret frame
 */
export type PathRotationType = 'free' | 'tangent' | 'normal' | 'binormal' | 'toPath' | 'full' | 0 | 1 | 2 | 3 | 4 | 5;

// ─── Query types ──────────────────────────────────────────────────────────────

/**
 * Optional filter applied to spatial queries (ray casts, sphere casts, AABB queries).
 * Both fields are optional; omitting a field means "no restriction on that axis".
 */
export interface QueryFilter {
  /**
   * Bitmask of allowed object layers.
   * Bit N = layer N is included in the query.
   * Default (omitted): all layers.
   * @example filter: { layerMask: 0b01 }  // only NON_MOVING (layer 0)
   * @example filter: { layerMask: 0b10 }  // only MOVING (layer 1)
   */
  layerMask?: number;
  /** Body IDs to exclude from the query. */
  excludeBodyIds?: BodyId[];
}

/** Result of a successful ray or shape cast. */
export interface RayCastHit {
  /** ID of the body that was hit. */
  bodyId: BodyId;
  /**
   * Parametric distance along the ray at the hit point.
   * Actual distance = `fraction * maxDistance`.
   */
  fraction: number;
  /** Surface normal at the hit point, pointing away from the surface. */
  normal: Vec3;
  /**
   * Index into the `materials` array supplied when the mesh/heightfield was
   * created. `0` when no per-triangle material was assigned.
   */
  materialIndex: number;
}

/** Result entry from `collideSphereAll`. */
export interface SphereCollideHit {
  /** ID of the body that was hit. */
  bodyId: BodyId;
  /** Closest point on the body surface, in world space. */
  contactPoint: Vec3;
  /** Penetration depth (positive = overlap). */
  penetrationDepth: number;
  /** Separation axis (from body toward the query sphere center). */
  normal: Vec3;
  /**
   * Index into the `materials` array supplied when the mesh/heightfield was
   * created. `0` when no per-triangle material was assigned.
   */
  materialIndex: number;
}

/** Result entry from `queryAABB`. */
export interface AABBHit {
  /** ID of the body whose AABB overlaps the query volume. */
  bodyId: BodyId;
}

// ─── Event types ──────────────────────────────────────────────────────────────

/** Fired when a body wakes up or goes to sleep. */
export interface BodyActivationEvent {
  /** `'activated'` or `'deactivated'`. */
  type: 'activated' | 'deactivated';
  /** The affected body. */
  bodyId: BodyId;
}

/** Fired on each contact between two bodies. */
export interface ContactEvent {
  /** Contact lifecycle stage. */
  type: 'added' | 'persisted' | 'removed';
  /** First body involved. */
  bodyA: BodyId;
  /** Second body involved. */
  bodyB: BodyId;
  /** World-space contact point (first contact point of the manifold). */
  point?: Vec3;
  /** World-space contact normal. */
  normal?: Vec3;
  /** Penetration depth in metres. */
  penetrationDepth?: number;
}

// ─── Spring / motor settings ──────────────────────────────────────────────────

/**
 * Input settings for a position-motor spring.
 * Use `frequency` (Hz) for `FrequencyAndDamping` mode,
 * or `stiffness` (spring constant k) for `StiffnessAndDamping` mode.
 */
export interface SpringSettings {
  /** Spring mode. Default: `0` (FrequencyAndDamping). */
  mode?: SpringMode;
  /** Natural frequency in Hz (FrequencyAndDamping mode). */
  frequency?: number;
  /** Spring constant k N/m (StiffnessAndDamping mode). */
  stiffness?: number;
  /** Damping ratio (FrequencyAndDamping) or damping coefficient (StiffnessAndDamping). */
  damping?: number;
  /** Maximum force limit (for translational motors). */
  maxForce?: number;
  /** Minimum force limit (for translational motors, usually negative). */
  minForce?: number;
  /** Maximum torque limit (for rotational motors). */
  maxTorque?: number;
  /** Minimum torque limit (for rotational motors, usually negative). */
  minTorque?: number;
}

/** Spring settings as returned by getters. */
export interface SpringSettingsResult {
  mode: SpringMode;
  frequency: number;
  damping: number;
  minForceLimit: number;
  maxForceLimit: number;
  minTorqueLimit: number;
  maxTorqueLimit: number;
}

/** Linear and angular damping coefficients. */
export interface DampingSettings {
  /** Linear velocity damping (0 = none). */
  linear: number;
  /** Angular velocity damping (0 = none). */
  angular: number;
}

// ─── Constraint lambda (impulse) types ───────────────────────────────────────

export interface HingeLambdas {
  position: Vec3;
  rotation: Vec2;
  rotationLimits: number;
  motor: number;
}

export interface SliderLambdas {
  position: Vec2;
  positionLimits: number;
  rotation: Vec3;
  motor: number;
}

export interface SwingTwistLambdas {
  position: Vec3;
  twist: number;
  swingY: number;
  swingZ: number;
  motor: Vec3;
}

export interface SixDOFLambdas {
  position: Vec3;
  rotation: Vec3;
  motorTranslation: Vec3;
  motorRotation: Vec3;
}

export interface ConeLambdas {
  position: Vec3;
  rotation: number;
}

export interface PointLambdas {
  position: Vec3;
}

export interface FixedLambdas {
  position: Vec3;
  rotation: Vec3;
}

export interface PathLambdas {
  position: Vec2;
  positionLimits: number;
  motor: number;
  rotationHinge: Vec2;
  rotation: Vec3;
}

// ─── SwingTwist / SixDOF limit types ─────────────────────────────────────────

export interface SwingTwistLimits {
  normalHalfCone: number;
  planeHalfCone: number;
  twistMin: number;
  twistMax: number;
}

export interface SixDOFLimits {
  translationMin: Vec3;
  translationMax: Vec3;
  rotationMin: Vec3;
  rotationMax: Vec3;
}

export interface DistanceLimits {
  min: number;
  max: number;
}

// ─── Shape specs (sub-shapes for compound bodies) ─────────────────────────────

/** One sub-shape entry used in compound body creation. */
export interface SubShapeSpec {
  kind: 'sphere' | 'box' | 'capsule' | 'cylinder';
  /** Local position relative to the compound body's origin. */
  position?: Vec3;
  /** Local rotation relative to the compound body's origin. */
  rotation?: Quat;
  /** Radius (sphere, capsule). */
  radius?: number;
  /** Half-height of the cylindrical part (capsule, cylinder). */
  halfHeight?: number;
  /** Half-extents (box). */
  halfExtents?: Vec3;
}

// ─── Body creation option types ───────────────────────────────────────────────

interface BodyOptsBase {
  /** Initial world-space position. */
  position: Vec3;
  /** `true` = dynamic (simulated), `false` = static. Default: `true`. */
  dynamic?: boolean;
  /** Coefficient of restitution [0..1]. Default: `0.2`. */
  restitution?: number;
  /** Coefficient of friction [0..1]. Default: `0.5`. */
  friction?: number;
}

export interface SphereOpts extends BodyOptsBase {
  /** Sphere radius in metres. */
  radius: number;
}

export interface BoxOpts extends BodyOptsBase {
  /** Half-extents of the box (centre to face). */
  halfExtents: Vec3;
}

export interface CapsuleOpts extends BodyOptsBase {
  /** Half-height of the cylindrical part (excluding hemispheres). */
  halfHeight: number;
  /** Radius of the hemispheres. */
  radius: number;
}

export interface CylinderOpts extends BodyOptsBase {
  halfHeight: number;
  radius: number;
}

export interface TaperedCapsuleOpts extends BodyOptsBase {
  halfHeight: number;
  topRadius: number;
  bottomRadius: number;
}

export interface TaperedCylinderOpts extends BodyOptsBase {
  halfHeight: number;
  topRadius: number;
  bottomRadius: number;
}

export interface ConvexHullOpts extends BodyOptsBase {
  /**
   * Flat array of vertex coordinates `[x0,y0,z0, x1,y1,z1, ...]`.
   * Must have at least 4 vertices (12 values).
   */
  points: number[];
}

/** Per-material slot used in mesh and heightfield shapes. */
export interface MaterialDef {
  /** Surface friction coefficient. Default: `0.5`. */
  friction?: number;
  /** Restitution (bounciness). Default: `0.0`. */
  restitution?: number;
}

export interface MeshOpts {
  /** Flat array of vertex coordinates `[x0,y0,z0, x1,y1,z1, ...]`. */
  vertices: number[];
  /** Triangle index list `[i0, i1, i2, ...]`. */
  indices: number[];
  position: Vec3;
  /** Default: `0.5`. */
  friction?: number;
  /** Body-level restitution fallback. Default: `0.0`. */
  restitution?: number;
  /**
   * Per-triangle material slot index. Length must equal `indices.length / 3`.
   * If omitted every triangle uses slot 0.
   */
  materialIndices?: number[] | Uint32Array;
  /**
   * Material slot definitions. Index `i` maps to `materialIndices[tri] === i`.
   * If omitted but `materialIndices` is provided, slots are auto-created with
   * default friction/restitution.
   */
  materials?: MaterialDef[];
}

export interface HeightFieldOpts {
  /**
   * Row-major height samples. Length must be ≥ `sampleCount * sampleCount`.
   * Must be a multiple of the block size (default 2) — use values like 16, 32, 64.
   */
  samples: number[] | Float32Array;
  /** Grid dimension N. Creates an N×N height field. */
  sampleCount: number;
  /** World-space offset applied to every vertex. */
  offset?: Vec3;
  /** Per-axis scale applied after offset. */
  scale?: Vec3;
  /** Position of the body in the world. */
  position?: Vec3;
  friction?: number;
  restitution?: number;
  /**
   * Per-cell material slot index. Length must equal `sampleCount * sampleCount`.
   * Accepts `Uint8Array` (max 255 slots) or a plain number array.
   */
  materialIndices?: number[] | Uint8Array;
  /** Material slot definitions indexed by the values in `materialIndices`. */
  materials?: MaterialDef[];
}

export interface CompoundOpts {
  /** Array of sub-shapes. */
  shapes: SubShapeSpec[];
  position: Vec3;
  dynamic?: boolean;
  friction?: number;
  restitution?: number;
}

// ─── Constraint option types ──────────────────────────────────────────────────

export interface HingeMotorOpts {
  state?: MotorState;
  targetVelocity?: number;
  /** Target angle in radians (used in Position mode). */
  targetAngle?: number;
  maxTorque?: number;
}

export interface SliderMotorOpts {
  state?: MotorState;
  targetVelocity?: number;
  /** Target position in metres (used in Position mode). */
  targetPosition?: number;
  maxForce?: number;
}

export interface SwingTwistMotorOpts {
  swingState?: MotorState;
  twistState?: MotorState;
  targetAngularVelocity?: Vec3;
  targetOrientation?: Quat;
  maxTorque?: number;
}

export interface PathMotorOpts {
  state?: MotorState;
  targetVelocity?: number;
  /** Target fraction along the path (0 … N-1). */
  targetFraction?: number;
}

export interface PathMotorState {
  state: MotorState;
  targetVelocity: number;
  targetFraction: number;
}

export interface PathPoint {
  position: Vec3;
  tangent: Vec3;
  normal: Vec3;
}

export interface PathConstraintOpts {
  /** Array of Hermite spline control points. */
  points: PathPoint[];
  /** Whether the path loops. Default: `false`. */
  closed?: boolean;
  /** Initial position along the path in body1 local space. */
  pathPosition?: Vec3;
  /** Initial orientation in body1 local space. */
  pathRotation?: Quat;
  /** Initial parametric fraction (0 … N-1). */
  pathFraction?: number;
  /** Maximum friction force along the path. */
  maxFriction?: number;
  rotationType?: PathRotationType;
}

export interface RackAndPinionOpts {
  hingeAxis: Vec3;
  sliderAxis: Vec3;
  /**
   * Gear ratio: metres of linear travel per radian of rotation.
   * Default: `1`.
   */
  ratio?: number;
  /** Constraint ID of the hinge that drives the pinion (for error correction). */
  pinionConstraintId?: ConstraintId;
  /** Constraint ID of the slider that drives the rack (for error correction). */
  rackConstraintId?: ConstraintId;
}

// ─── Ragdoll types ────────────────────────────────────────────────────────────

export interface RagdollCreationOpts {
  /** Collision group shared by all ragdoll bodies. Default: `1`. */
  collisionGroup?: number;
  userData?: number;
  /** Activate all bodies immediately. Default: `true`. */
  activate?: boolean;
}

export interface RagdollSettingsOpts {
  /** Half-height of the default capsule for each joint. */
  capsuleHalfHeight?: number;
  /** Radius of the default capsule for each joint. */
  capsuleRadius?: number;
  /** Gap between consecutive joint capsules. */
  spacing?: number;
}

export interface RagdollJointShapeConfig {
  kind: 'capsule' | 'box' | 'sphere';
  halfHeight?: number;
  radius?: number;
  halfExtents?: Vec3;
}

export interface RagdollJointConstraintConfig {
  type: 'fixed' | 'swingTwist' | 'hinge' | 'cone';
  /** Auto-compute axes from bone direction. Default: `true`. */
  autoAxes?: boolean;
  normalHalfCone?: number;
  planeHalfCone?: number;
  twistMin?: number;
  twistMax?: number;
  minAngle?: number;
  maxAngle?: number;
  halfConeAngle?: number;
  twistAxis1?: Vec3;
  twistAxis2?: Vec3;
  planeAxis1?: Vec3;
  planeAxis2?: Vec3;
  hingeAxis1?: Vec3;
  hingeAxis2?: Vec3;
  normalAxis1?: Vec3;
  normalAxis2?: Vec3;
  coneAxis1?: Vec3;
  coneAxis2?: Vec3;
}

export interface JointInfo {
  name: string;
  parentIndex: number;
}

// ─── Skeleton ─────────────────────────────────────────────────────────────────

/**
 * Skeleton definition used to build a ragdoll.
 * Create via `world.createSkeleton()`.
 */
export declare class Skeleton {
  /**
   * Add a joint to the skeleton.
   * @param name        Unique joint name.
   * @param parentIndex Index of the parent joint, or `-1` for the root.
   * @returns `this` for chaining.
   */
  addJoint(name: string, parentIndex?: number): this;

  /**
   * Finalise the skeleton after all joints have been added.
   * Must be called before `createRagdollSettings`.
   * @returns `true` on success.
   */
  finalize(): boolean;

  /** Returns the total number of joints. */
  getJointCount(): number;

  /**
   * Returns name and parent index for the joint at `jointIndex`.
   * @throws If `jointIndex` is out of range.
   */
  getJointInfo(jointIndex: number): JointInfo;

  /**
   * Looks up a joint by name and returns its index, or `-1` if not found.
   */
  getJointIndex(name: string): number;

  /**
   * Build `RagdollSettings` from this skeleton with default capsule geometry.
   * Call `setJointShape`, `setJointTransform`, `setJointConstraint` on the result
   * before calling `createRagdoll`.
   */
  createRagdollSettings(opts?: RagdollSettingsOpts): RagdollSettings;
}

// ─── RagdollSettings ──────────────────────────────────────────────────────────

/**
 * Template for creating ragdoll instances.
 * Obtained from `skeleton.createRagdollSettings()`.
 */
export declare class RagdollSettings {
  readonly id: number;

  /**
   * Override the collision shape of a specific joint.
   * Call before `createRagdoll`.
   */
  setJointShape(jointIndex: number, config: RagdollJointShapeConfig): void;

  /**
   * Override the world-space position and rotation of a joint body.
   * Call before `createRagdoll`.
   */
  setJointTransform(jointIndex: number, position: Vec3, rotation: Quat): void;

  /**
   * Set the constraint type connecting a joint to its parent.
   * Joint 0 (root) has no parent and the call is ignored for it.
   * Call before `createRagdoll`.
   */
  setJointConstraint(jointIndex: number, config: RagdollJointConstraintConfig): void;

  /** Spawn a ragdoll instance in the world. */
  createRagdoll(opts?: RagdollCreationOpts): Ragdoll;
}

// ─── Ragdoll ──────────────────────────────────────────────────────────────────

/**
 * Live ragdoll instance with physics bodies and constraints.
 * Obtained from `ragdollSettings.createRagdoll()`.
 */
export declare class Ragdoll {
  readonly id: number;

  /** Returns the number of bones (= number of skeleton joints). */
  bodyCount(): number;

  /**
   * Returns the `BodyId` for bone at `index`.
   * Use with `world.applyImpulse`, `world.setAngularVelocity`, etc.
   */
  getBoneBodyId(index: number): BodyId;

  /** Returns the current world-space transform of the bone at `index`. */
  getBoneTransform(index: number): Transform;

  /** Teleport the bone at `index` to the given world-space transform. */
  setBoneTransform(index: number, transform: Transform): void;

  /**
   * Snapshot the current bone transforms as a pose array.
   * @returns Array of `Transform` objects, one per bone.
   */
  syncToSkeletonPose(): Transform[];

  /**
   * Apply a previously captured pose array to all bones.
   * @param bones Array returned by `syncToSkeletonPose`.
   */
  syncFromSkeletonPose(bones: Transform[]): void;

  /**
   * Returns the constraint IDs for each joint (indexed by joint index).
   * Index 0 (root joint) is always `0` (no constraint).
   * Use the IDs with `setSwingTwistMotor`, `setHingeMotor`, etc.
   */
  getConstraintIds(): ConstraintId[];

  /** Remove the ragdoll from the world and free its resources. */
  destroy(): void;
}

// ─── World ────────────────────────────────────────────────────────────────────

/** Options passed to the `World` constructor. */
export interface WorldOptions {
  /** Gravitational acceleration in m/s². Default: `9.81`. */
  gravity?: number;
}

/**
 * Physics simulation world.
 *
 * @example
 * ```js
 * const { World } = require('jolt-physics-node');
 * const world = new World({ gravity: 9.81 });
 * const ballId = world.createSphere({ radius: 0.5, position: { x: 0, y: 5, z: 0 } });
 * world.step(1 / 60);
 * const pos = world.getBodyPosition(ballId);
 * world.destroy();
 * ```
 */
export declare class World {
  constructor(options?: WorldOptions);

  // ── Lifecycle ───────────────────────────────────────────────────────────────

  /**
   * Advance the simulation by `dt` seconds.
   * Call once per frame, typically with `dt = 1/60`.
   * Dispatches queued body-activation and contact events to registered callbacks.
   */
  step(dt?: number): void;

  /**
   * Change the global gravity vector.
   * @param gravity Magnitude in m/s² (positive = downward). Default: `9.81`.
   */
  setGravity(gravity: number): void;

  /**
   * Register a callback that fires whenever a body is activated or deactivated.
   * Pass `null` to remove the callback.
   */
  onBodyActivation(callback: ((event: BodyActivationEvent) => void) | null): void;

  /**
   * Register a callback that fires on each contact event (added, persisted, removed).
   * Pass `null` to remove the callback.
   */
  onContact(callback: ((event: ContactEvent) => void) | null): void;

  /** Destroy the world and free all native resources. Must be called when done. */
  destroy(): void;

  // ── Body creation ───────────────────────────────────────────────────────────

  /**
   * Create a sphere body.
   * @returns New body ID.
   */
  createSphere(opts: SphereOpts): BodyId;

  /**
   * Create an axis-aligned box body.
   * @returns New body ID.
   */
  createBox(opts: BoxOpts): BodyId;

  /**
   * Create a capsule body (cylinder capped with hemispheres).
   * @returns New body ID.
   */
  createCapsule(opts: CapsuleOpts): BodyId;

  /**
   * Create a cylinder body.
   * @returns New body ID.
   */
  createCylinder(opts: CylinderOpts): BodyId;

  /**
   * Create a tapered capsule (frustum capped with spheres of different radii).
   * @returns New body ID.
   */
  createTaperedCapsule(opts: TaperedCapsuleOpts): BodyId;

  /**
   * Create a tapered cylinder (frustum).
   * @returns New body ID.
   */
  createTaperedCylinder(opts: TaperedCylinderOpts): BodyId;

  /**
   * Create a convex-hull body from an arbitrary point cloud.
   * The hull is computed automatically by Jolt.
   * @returns New body ID.
   */
  createConvexHull(opts: ConvexHullOpts): BodyId;

  /**
   * Create a static triangle-mesh body. Always non-dynamic.
   * @returns New body ID.
   */
  createMesh(opts: MeshOpts): BodyId;

  /**
   * Create a static height-field terrain body.
   * `sampleCount` must be divisible by 2 (the default Jolt block size).
   * @returns New body ID.
   */
  createHeightField(opts: HeightFieldOpts): BodyId;

  /**
   * Create a static compound body from multiple sub-shapes.
   * Jolt automatically shifts the body to the centre of mass.
   * @returns New body ID.
   */
  createStaticCompound(opts: CompoundOpts): BodyId;

  /**
   * Create a mutable compound body.
   * Sub-shapes can be added, removed and repositioned at runtime.
   * @returns New body ID.
   */
  createMutableCompound(opts: CompoundOpts): BodyId;

  /**
   * Add a sub-shape to a mutable compound body.
   * @returns The index of the newly added sub-shape.
   * @throws If the body is not found or the spec is invalid.
   */
  addMutableSubShape(bodyId: BodyId, shapeSpec: SubShapeSpec): number;

  /**
   * Remove the sub-shape at `index` from a mutable compound body.
   * @throws If the body is not found.
   */
  removeMutableSubShape(bodyId: BodyId, index: number): void;

  /**
   * Reposition a sub-shape within a mutable compound body.
   * @throws If the body is not found.
   */
  modifyMutableSubShape(bodyId: BodyId, index: number, position: Vec3, rotation?: Quat): void;

  /**
   * Recalculate the centre of mass after modifying a mutable compound body.
   * Always call this after `addMutableSubShape` / `removeMutableSubShape`.
   * @throws If the body is not found.
   */
  adjustMutableCenterOfMass(bodyId: BodyId): void;

  // ── Body removal ────────────────────────────────────────────────────────────

  /**
   * Remove a body from the world and destroy it.
   * @throws If the body is not found.
   */
  removeBody(bodyId: BodyId): void;

  /** Returns `true` if the body exists in the world. */
  hasBody(bodyId: BodyId): boolean;

  /** Returns `true` if the body is currently active (not sleeping). */
  isBodyActive(bodyId: BodyId): boolean;

  // ── Body state ──────────────────────────────────────────────────────────────

  /**
   * Get the current world-space position of the body's origin.
   * @throws If `bodyId` is invalid.
   */
  getBodyPosition(bodyId: BodyId): Vec3;

  /**
   * Get the current world-space rotation of the body.
   * @throws If `bodyId` is invalid.
   */
  getBodyRotation(bodyId: BodyId): Quat;

  /**
   * Teleport the body to `position`.
   * @param activate Wake the body if it was sleeping. Default: `true`.
   * @throws If `bodyId` is invalid.
   */
  setBodyPosition(bodyId: BodyId, position: Vec3, activate?: boolean): void;

  /**
   * Teleport the body to `rotation`.
   * @param activate Wake the body if it was sleeping. Default: `true`.
   * @throws If `bodyId` is invalid.
   */
  setBodyRotation(bodyId: BodyId, rotation: Quat, activate?: boolean): void;

  /**
   * Get the world-space linear velocity of the body (m/s).
   * @throws If `bodyId` is invalid.
   */
  getLinearVelocity(bodyId: BodyId): Vec3;

  /**
   * Set the world-space linear velocity (m/s).
   * @throws If `bodyId` is invalid.
   */
  setLinearVelocity(bodyId: BodyId, velocity: Vec3): void;

  /**
   * Get the world-space angular velocity (rad/s).
   * @throws If `bodyId` is invalid.
   */
  getAngularVelocity(bodyId: BodyId): Vec3;

  /**
   * Set the world-space angular velocity (rad/s).
   * @throws If `bodyId` is invalid.
   */
  setAngularVelocity(bodyId: BodyId, velocity: Vec3): void;

  /**
   * Get the world-space centre-of-mass position.
   * For simple shapes this equals `getBodyPosition`; for compound shapes
   * it is shifted to the actual centre of mass.
   * @throws If `bodyId` is invalid.
   */
  getCenterOfMassPosition(bodyId: BodyId): Vec3;

  // ── Body forces ─────────────────────────────────────────────────────────────

  /**
   * Apply a world-space linear impulse at the centre of mass (kg·m/s).
   * @throws If `bodyId` is invalid.
   */
  applyImpulse(bodyId: BodyId, impulse: Vec3): void;

  /**
   * Apply a world-space angular impulse (kg·m²/s).
   * @throws If `bodyId` is invalid.
   */
  addAngularImpulse(bodyId: BodyId, impulse: Vec3): void;

  /**
   * Add a persistent world-space force (N). Cleared each step.
   * @throws If `bodyId` is invalid.
   */
  addForce(bodyId: BodyId, force: Vec3): void;

  /**
   * Add a persistent world-space torque (N·m). Cleared each step.
   * @throws If `bodyId` is invalid.
   */
  addTorque(bodyId: BodyId, torque: Vec3): void;

  // ── Body properties ─────────────────────────────────────────────────────────

  /** @throws If `bodyId` is invalid. */
  setFriction(bodyId: BodyId, friction: number): void;
  /** @throws If `bodyId` is invalid. */
  getFriction(bodyId: BodyId): number;

  /** @throws If `bodyId` is invalid. */
  setRestitution(bodyId: BodyId, restitution: number): void;
  /** @throws If `bodyId` is invalid. */
  getRestitution(bodyId: BodyId): number;

  /**
   * Scale the global gravity applied to this body.
   * `0` = zero gravity, `1` = full gravity (default), negative = anti-gravity.
   * @throws If `bodyId` is invalid.
   */
  setGravityFactor(bodyId: BodyId, factor: number): void;
  /** @throws If `bodyId` is invalid. */
  getGravityFactor(bodyId: BodyId): number;

  /**
   * Change the motion type at runtime.
   * @param motionType `0` Static | `1` Kinematic | `2` Dynamic
   * @throws If `bodyId` is invalid.
   */
  setMotionType(bodyId: BodyId, motionType: MotionType, activate?: boolean): void;
  /** @throws If `bodyId` is invalid. */
  getMotionType(bodyId: BodyId): MotionType;

  /**
   * Set the collision detection quality.
   * @param quality `0` Discrete | `1` LinearCast
   * @throws If `bodyId` is invalid.
   */
  setMotionQuality(bodyId: BodyId, quality: MotionQuality): void;
  /** @throws If `bodyId` is invalid. */
  getMotionQuality(bodyId: BodyId): MotionQuality;

  /**
   * Move the body to a different object layer.
   * Layer `0` = NON_MOVING, layer `1` = MOVING.
   * @throws If `bodyId` is invalid.
   */
  setObjectLayer(bodyId: BodyId, layer: number): void;
  /** @throws If `bodyId` is invalid. */
  getObjectLayer(bodyId: BodyId): number;

  /** @throws If `bodyId` is invalid. */
  setDamping(bodyId: BodyId, opts: Partial<DampingSettings>): void;
  /** @throws If `bodyId` is invalid. */
  getDamping(bodyId: BodyId): DampingSettings;

  /**
   * Mark the body as a sensor (trigger volume): generates contact events
   * but exerts no physical forces.
   * @throws If `bodyId` is invalid.
   */
  setBodySensor(bodyId: BodyId, isSensor: boolean): void;
  /** @throws If `bodyId` is invalid. */
  isBodySensor(bodyId: BodyId): boolean;

  /** Wake a sleeping body. @throws If `bodyId` is invalid. */
  activateBody(bodyId: BodyId): void;
  /** Put a body to sleep. @throws If `bodyId` is invalid. */
  deactivateBody(bodyId: BodyId): void;

  /** Returns `true` if the two bodies are currently touching. */
  areBodiesInContact(bodyA: BodyId, bodyB: BodyId): boolean;

  // ── Spatial queries ─────────────────────────────────────────────────────────

  /**
   * Cast a ray and return the single closest hit, or `null` if nothing was hit.
   */
  rayCastClosest(opts: {
    origin: Vec3;
    direction: Vec3;
    maxDistance: number;
    filter?: QueryFilter;
  }): RayCastHit | null;

  /**
   * Cast a ray and return **all** hits sorted by distance.
   */
  rayCastAll(opts: {
    origin: Vec3;
    direction: Vec3;
    maxDistance: number;
    filter?: QueryFilter;
  }): RayCastHit[];

  /**
   * Find all bodies whose AABB overlaps the given axis-aligned bounding box.
   */
  queryAABB(opts: {
    min: Vec3;
    max: Vec3;
    filter?: QueryFilter;
  }): AABBHit[];

  /**
   * Find all bodies that overlap a sphere, returning contact information.
   * `maxSeparation` controls the maximum allowed gap between the sphere and the body surface (default `0`).
   */
  collideSphereAll(opts: {
    center: Vec3;
    radius: number;
    /** Maximum allowed gap between the sphere and the body surface. Default: `0`. */
    maxSeparation?: number;
    filter?: QueryFilter;
  }): SphereCollideHit[];

  /**
   * Sweep a sphere along a ray and return all hits.
   */
  castSphereAll(opts: {
    origin: Vec3;
    direction: Vec3;
    maxDistance: number;
    radius: number;
    filter?: QueryFilter;
  }): RayCastHit[];

  // ── Constraints ─────────────────────────────────────────────────────────────

  /**
   * Remove a constraint from the world.
   * @throws If `constraintId` is invalid.
   */
  removeConstraint(constraintId: ConstraintId): void;

  /**
   * Weld two bodies together at their current relative transform.
   * @returns New constraint ID.
   */
  createFixedConstraint(bodyA: BodyId, bodyB: BodyId): ConstraintId;

  /**
   * Constrain the distance between two anchor points on two bodies.
   * @param minDistance Minimum allowed distance. Default: `0`.
   * @param maxDistance Maximum allowed distance. `0` = rigid.
   * @returns New constraint ID.
   */
  createDistanceConstraint(
    bodyA: BodyId,
    bodyB: BodyId,
    pointA: Vec3,
    pointB: Vec3,
    minDistance?: number,
    maxDistance?: number,
  ): ConstraintId;

  /**
   * Create a hinge (revolute) constraint: rotation around one axis.
   * @param anchor  World-space pivot point.
   * @param axis    Hinge axis in world space.
   * @param normal  Normal axis, perpendicular to the hinge axis.
   * @returns New constraint ID.
   */
  createHingeConstraint(
    bodyA: BodyId,
    bodyB: BodyId,
    anchor: Vec3,
    axis: Vec3,
    normal: Vec3,
  ): ConstraintId;

  /**
   * Create a prismatic (slider) constraint: translation along one axis.
   * @param axis    Slide axis in world space.
   * @param normal  Normal axis, perpendicular to the slide axis.
   * @param min     Minimum position (m). Default: `-1`.
   * @param max     Maximum position (m). Default: `1`.
   * @returns New constraint ID.
   */
  createSliderConstraint(
    bodyA: BodyId,
    bodyB: BodyId,
    anchor: Vec3,
    axis: Vec3,
    normal: Vec3,
    min?: number,
    max?: number,
  ): ConstraintId;

  /**
   * Create a ball-and-socket constraint at a shared world-space point.
   * @returns New constraint ID.
   */
  createPointConstraint(bodyA: BodyId, bodyB: BodyId, point: Vec3): ConstraintId;

  /**
   * Create a cone constraint: rotation limited to a cone around `twistAxis`.
   * @param halfConeAngle Half-angle of the cone in radians.
   * @returns New constraint ID.
   */
  createConeConstraint(
    bodyA: BodyId,
    bodyB: BodyId,
    point: Vec3,
    twistAxis: Vec3,
    halfConeAngle: number,
  ): ConstraintId;

  /**
   * Create a swing-twist constraint (used for shoulder, hip joints, etc.).
   * @param twistAxis  Primary rotation axis.
   * @param planeAxis  Plane axis perpendicular to the twist axis.
   * @returns New constraint ID.
   */
  createSwingTwistConstraint(
    bodyA: BodyId,
    bodyB: BodyId,
    point: Vec3,
    twistAxis: Vec3,
    planeAxis: Vec3,
    limits?: Partial<SwingTwistLimits>,
  ): ConstraintId;

  /**
   * Create a 6-DOF constraint with independent per-axis translation and rotation limits.
   * @param axisX  Local X axis in world space.
   * @param axisY  Local Y axis in world space.
   * @returns New constraint ID.
   */
  createSixDOFConstraint(
    bodyA: BodyId,
    bodyB: BodyId,
    point: Vec3,
    axisX: Vec3,
    axisY: Vec3,
  ): ConstraintId;

  /**
   * Create a gear constraint coupling two bodies that are already
   * constrained by hinge joints.
   * @param ratio  Tooth ratio `teeth2 / teeth1`. Default: `1`.
   * @param gear1ConstraintId  Hinge constraint ID of the first gear (for error correction).
   * @param gear2ConstraintId  Hinge constraint ID of the second gear (for error correction).
   * @returns New constraint ID.
   */
  createGearConstraint(
    bodyA: BodyId,
    bodyB: BodyId,
    hingeAxis1: Vec3,
    hingeAxis2: Vec3,
    ratio?: number,
    gear1ConstraintId?: ConstraintId,
    gear2ConstraintId?: ConstraintId,
  ): ConstraintId;

  /**
   * Create a pulley constraint coupling two bodies via a rope over two fixed points.
   * `opts.ratio` is the block-and-tackle length ratio (default `1`).
   * `opts.minLength` is the minimum total rope length (default `0`).
   * `opts.maxLength` is the maximum total rope length (`-1` = auto from initial positions).
   * @returns New constraint ID.
   */
  createPulleyConstraint(
    bodyA: BodyId,
    bodyB: BodyId,
    bodyPoint1: Vec3,
    fixedPoint1: Vec3,
    bodyPoint2: Vec3,
    fixedPoint2: Vec3,
    opts?: {
      /** Block-and-tackle length ratio. Default: `1`. */
      ratio?: number;
      /** Minimum total rope length. Default: `0`. */
      minLength?: number;
      /** Maximum total rope length. `-1` = auto-calculate from initial body positions. */
      maxLength?: number;
    },
  ): ConstraintId;

  /**
   * Create a rack-and-pinion constraint that couples a hinge to a slider.
   * @returns New constraint ID.
   */
  createRackAndPinionConstraint(bodyA: BodyId, bodyB: BodyId, opts: RackAndPinionOpts): ConstraintId;

  /**
   * Create a path constraint: bodyB slides / rotates along a Hermite spline path
   * attached to bodyA.
   * @returns New constraint ID.
   */
  createPathConstraint(bodyA: BodyId, bodyB: BodyId, opts: PathConstraintOpts): ConstraintId;

  // ── Constraint setters ──────────────────────────────────────────────────────

  /** @throws If constraint not found. */
  setHingeLimits(constraintId: ConstraintId, minAngle: number, maxAngle: number): void;
  /** @throws If constraint not found. */
  setSliderLimits(constraintId: ConstraintId, minLimit: number, maxLimit: number): void;
  /** @throws If constraint not found. */
  setConeHalfAngle(constraintId: ConstraintId, halfConeAngle: number): void;
  /** @throws If constraint not found. */
  setSwingTwistLimits(constraintId: ConstraintId, limits: SwingTwistLimits): void;
  /** @throws If constraint not found. */
  setSixDOFLimits(constraintId: ConstraintId, limits: SixDOFLimits): void;
  /** @throws If constraint not found. */
  setDistanceLimits(constraintId: ConstraintId, min: number, max: number): void;
  /** @throws If constraint not found. */
  setPulleyLength(constraintId: ConstraintId, minLength: number, maxLength: number): void;

  /** @throws If constraint not found. */
  setHingeMotor(constraintId: ConstraintId, opts?: HingeMotorOpts): void;
  /** @throws If constraint not found. */
  setSliderMotor(constraintId: ConstraintId, opts?: SliderMotorOpts): void;
  /** @throws If constraint not found. */
  setSwingTwistMotor(constraintId: ConstraintId, opts?: SwingTwistMotorOpts): void;
  /** @throws If constraint not found. */
  setSixDOFMotorState(constraintId: ConstraintId, axis: number, state: MotorState): void;
  /** @throws If constraint not found. */
  setSixDOFTargetVelocity(constraintId: ConstraintId, linear: Vec3, angular: Vec3): void;
  /** @throws If constraint not found. */
  setSixDOFTargetPose(constraintId: ConstraintId, position: Vec3, orientation: Quat): void;
  /** @throws If constraint not found. */
  setPathMotor(constraintId: ConstraintId, opts?: PathMotorOpts): void;

  /** Set spring settings for a hinge position motor. @throws If constraint not found. */
  setHingeMotorSpring(constraintId: ConstraintId, opts?: SpringSettings): void;
  /** Set spring settings for a slider position motor. @throws If constraint not found. */
  setSliderMotorSpring(constraintId: ConstraintId, opts?: SpringSettings): void;
  /** Set spring settings for the swing motor of a swing-twist constraint. @throws If constraint not found. */
  setSwingMotorSpring(constraintId: ConstraintId, opts?: SpringSettings): void;
  /** Set spring settings for the twist motor of a swing-twist constraint. @throws If constraint not found. */
  setTwistMotorSpring(constraintId: ConstraintId, opts?: SpringSettings): void;
  /**
   * Set spring settings for one axis of a 6-DOF motor.
   * @param axis 0–2 = translation axes (X/Y/Z), 3–5 = rotation axes.
   * @throws If constraint not found.
   */
  setSixDOFMotorSpring(constraintId: ConstraintId, axis: number, opts?: SpringSettings): void;
  /** Set spring settings for the path position motor. @throws If constraint not found. */
  setPathMotorSpring(constraintId: ConstraintId, opts?: SpringSettings): void;
  /** @throws If constraint not found. */
  setDistanceLimitsSpring(constraintId: ConstraintId, opts?: SpringSettings): void;

  // ── Constraint getters ──────────────────────────────────────────────────────

  /** Current hinge angle in radians. @throws If constraint not found. */
  getHingeAngle(constraintId: ConstraintId): number;
  /** @throws If constraint not found. */
  getHingeMotorState(constraintId: ConstraintId): HingeMotorOpts;
  /** @throws If constraint not found. */
  getHingeLimits(constraintId: ConstraintId): { min: number; max: number };
  /** @throws If constraint not found. */
  getHingeMotorSpring(constraintId: ConstraintId): SpringSettingsResult;

  /** Current slider position in metres. @throws If constraint not found. */
  getSliderPosition(constraintId: ConstraintId): number;
  /** @throws If constraint not found. */
  getSliderMotorState(constraintId: ConstraintId): SliderMotorOpts;
  /** @throws If constraint not found. */
  getSliderLimits(constraintId: ConstraintId): { min: number; max: number };
  /** @throws If constraint not found. */
  getSliderMotorSpring(constraintId: ConstraintId): SpringSettingsResult;

  /** @throws If constraint not found. */
  getSwingTwistRotation(constraintId: ConstraintId): Quat;
  /** @throws If constraint not found. */
  getSwingTwistMotorState(constraintId: ConstraintId): SwingTwistMotorOpts;
  /** @throws If constraint not found. */
  getSwingTwistLimits(constraintId: ConstraintId): SwingTwistLimits;
  /** @throws If constraint not found. */
  getSwingMotorSpring(constraintId: ConstraintId): SpringSettingsResult;
  /** @throws If constraint not found. */
  getTwistMotorSpring(constraintId: ConstraintId): SpringSettingsResult;

  /** @throws If constraint not found. */
  getSixDOFRotation(constraintId: ConstraintId): Vec3;
  /** @throws If constraint not found. */
  getSixDOFLimits(constraintId: ConstraintId): SixDOFLimits;
  /** @throws If constraint not found. */
  getSixDOFMotorState(constraintId: ConstraintId, axis: number): MotorState;
  /** @throws If constraint not found. */
  getSixDOFMotorSpring(constraintId: ConstraintId, axis: number): SpringSettingsResult;

  /** Current parametric position along the path (0 … N-1). @throws If constraint not found. */
  getPathFraction(constraintId: ConstraintId): number;
  /** Maximum fraction (= number of control points − 1). @throws If constraint not found. */
  getPathMaxFraction(constraintId: ConstraintId): number;
  /** @throws If constraint not found. */
  getPathMotorState(constraintId: ConstraintId): PathMotorState;
  /** @throws If constraint not found. */
  getPathMotorSpring(constraintId: ConstraintId): SpringSettingsResult;

  /** @throws If constraint not found. */
  getDistanceLimits(constraintId: ConstraintId): DistanceLimits;
  /** @throws If constraint not found. */
  getDistanceLimitsSpring(constraintId: ConstraintId): SpringSettingsResult;

  /** Current total rope length of a pulley constraint. @throws If constraint not found. */
  getPulleyLength(constraintId: ConstraintId): number;
  /** @throws If constraint not found. */
  getPulleyLengthLimits(constraintId: ConstraintId): { min: number; max: number };

  // ── Constraint lambda (impulse) getters ─────────────────────────────────────

  /** @throws If constraint not found. */
  getHingeLambdas(constraintId: ConstraintId): HingeLambdas;
  /** @throws If constraint not found. */
  getSliderLambdas(constraintId: ConstraintId): SliderLambdas;
  /** @throws If constraint not found. */
  getSwingTwistLambdas(constraintId: ConstraintId): SwingTwistLambdas;
  /** @throws If constraint not found. */
  getSixDOFLambdas(constraintId: ConstraintId): SixDOFLambdas;
  /** @throws If constraint not found. */
  getConeLambdas(constraintId: ConstraintId): ConeLambdas;
  /** @throws If constraint not found. */
  getPointLambdas(constraintId: ConstraintId): PointLambdas;
  /** @throws If constraint not found. */
  getFixedLambdas(constraintId: ConstraintId): FixedLambdas;
  /** @throws If constraint not found. */
  getDistanceLambda(constraintId: ConstraintId): number;
  /** @throws If constraint not found. */
  getPulleyLambda(constraintId: ConstraintId): number;
  /** @throws If constraint not found. */
  getGearLambda(constraintId: ConstraintId): number;
  /** @throws If constraint not found. */
  getRackAndPinionLambda(constraintId: ConstraintId): number;
  /** @throws If constraint not found. */
  getPathLambdas(constraintId: ConstraintId): PathLambdas;

  // ── Serialization ───────────────────────────────────────────────────────────

  /**
   * Capture a compact binary snapshot of all body states.
   *
   * Format: 56 bytes per body —
   * `bodyId(u32) posX posY posZ(f32×3) rotX rotY rotZ rotW(f32×4) lvX lvY lvZ(f32×3) avX avY avZ(f32×3)`
   *
   * Ideal for network state synchronisation and deterministic rewind.
   * @returns `Buffer` whose byte-length is `56 * bodyCount`.
   */
  snapshotState(): Buffer;

  /**
   * Apply a snapshot produced by `snapshotState` to the current world.
   * Bodies are matched by `bodyId`; unrecognised IDs are silently skipped.
   * @returns `true` on success; `false` if the buffer size is not a multiple of 56.
   */
  applySnapshot(snapshot: Buffer): boolean;

  /**
   * Serialise all bodies (shapes, properties, current pose and velocities)
   * using Jolt's binary scene format.
   *
   * **Note:** custom constraints created via the constraint API are **not** included.
   * Re-create them after calling `loadScene`.
   *
   * @returns A `Buffer` suitable for storage or transfer.
   */
  saveScene(): Buffer;

  /**
   * Restore bodies from data produced by `saveScene`.
   * Bodies are added to the existing world; call on a freshly created `World`
   * to get a clean restore.
   *
   * @returns The number of bodies created.
   * @throws If the buffer is corrupt or incompatible.
   */
  loadScene(data: Buffer): number;

  // ── Skeleton factory ────────────────────────────────────────────────────────

  /** Create a new empty `Skeleton` owned by this world. */
  createSkeleton(): Skeleton;
}

// ─── PhysicsWorker ────────────────────────────────────────────────────────────

/** Options forwarded to `PhysicsWorker.create`. */
export interface PhysicsWorkerOptions {
  /**
   * Automatically emit a state snapshot via `onState` after every `step` call.
   * Default: `true`.
   */
  autoState?: boolean;
}

/**
 * Runs a `World` in a dedicated Node.js Worker Thread.
 *
 * All `World` methods are exposed as `async` equivalents that send
 * commands via `postMessage` and resolve when the worker responds.
 * State snapshots are delivered through `onState` callbacks after each step.
 *
 * @example
 * ```js
 * const { PhysicsWorker } = require('jolt-physics-node');
 *
 * const worker = await PhysicsWorker.create({ gravity: 9.81 });
 * const ballId = await worker.createSphere({ radius: 0.5, position: { x: 0, y: 5, z: 0 } });
 *
 * worker.onState((snapshot) => {
 *   // Buffer with 56 bytes per body — decode as snapshotState() format
 * });
 *
 * await worker.step(1 / 60);
 * await worker.terminate();
 * ```
 */
export declare class PhysicsWorker {
  /**
   * Create and initialise a `PhysicsWorker`.
   * Spawns a Worker Thread, creates a `World` with `worldOpts`, and resolves
   * when the worker is ready to accept commands.
   */
  static create(worldOpts?: WorldOptions, workerOpts?: PhysicsWorkerOptions): Promise<PhysicsWorker>;

  /**
   * Register a callback invoked after each `step` call (when `autoState` is `true`).
   * The `snapshot` is a `Buffer` in the same format as `World.snapshotState()`.
   * @returns `this` for chaining.
   */
  onState(callback: (snapshot: Buffer) => void): this;

  /**
   * Register a callback for physics events forwarded from the worker world
   * (`'bodyActivation'` and `'contact'`).
   * @returns `this` for chaining.
   */
  onEvent(callback: (kind: 'bodyActivation' | 'contact', data: BodyActivationEvent | ContactEvent) => void): this;

  /** Advance the worker simulation by `dt` seconds. */
  step(dt: number): Promise<void>;

  /** Get a manual state snapshot from the worker. */
  snapshotState(): Promise<Buffer>;
  /** Apply a snapshot to the worker world. */
  applySnapshot(snapshot: Buffer): Promise<boolean>;
  /** Serialise the worker world. */
  saveScene(): Promise<Buffer>;
  /** Restore bodies from serialised data into the worker world. */
  loadScene(data: Buffer): Promise<number>;

  createSphere(opts: SphereOpts): Promise<BodyId>;
  createBox(opts: BoxOpts): Promise<BodyId>;
  createCapsule(opts: CapsuleOpts): Promise<BodyId>;
  createCylinder(opts: CylinderOpts): Promise<BodyId>;
  createTaperedCapsule(opts: TaperedCapsuleOpts): Promise<BodyId>;
  createTaperedCylinder(opts: TaperedCylinderOpts): Promise<BodyId>;
  createConvexHull(opts: ConvexHullOpts): Promise<BodyId>;
  createMesh(opts: MeshOpts): Promise<BodyId>;
  createHeightField(opts: HeightFieldOpts): Promise<BodyId>;
  createStaticCompound(opts: CompoundOpts): Promise<BodyId>;
  createMutableCompound(opts: CompoundOpts): Promise<BodyId>;
  removeBody(bodyId: BodyId): Promise<void>;

  getBodyPosition(bodyId: BodyId): Promise<Vec3>;
  getBodyRotation(bodyId: BodyId): Promise<Quat>;
  setBodyPosition(bodyId: BodyId, position: Vec3, activate?: boolean): Promise<void>;
  setBodyRotation(bodyId: BodyId, rotation: Quat, activate?: boolean): Promise<void>;
  getLinearVelocity(bodyId: BodyId): Promise<Vec3>;
  setLinearVelocity(bodyId: BodyId, velocity: Vec3): Promise<void>;
  getAngularVelocity(bodyId: BodyId): Promise<Vec3>;
  setAngularVelocity(bodyId: BodyId, velocity: Vec3): Promise<void>;
  applyImpulse(bodyId: BodyId, impulse: Vec3): Promise<void>;
  addForce(bodyId: BodyId, force: Vec3): Promise<void>;
  addTorque(bodyId: BodyId, torque: Vec3): Promise<void>;

  hasBody(bodyId: BodyId): Promise<boolean>;
  isBodyActive(bodyId: BodyId): Promise<boolean>;
  setFriction(bodyId: BodyId, friction: number): Promise<void>;
  getFriction(bodyId: BodyId): Promise<number>;
  setRestitution(bodyId: BodyId, restitution: number): Promise<void>;
  getRestitution(bodyId: BodyId): Promise<number>;
  setGravityFactor(bodyId: BodyId, factor: number): Promise<void>;
  getGravityFactor(bodyId: BodyId): Promise<number>;
  setMotionType(bodyId: BodyId, motionType: MotionType, activate?: boolean): Promise<void>;
  getMotionType(bodyId: BodyId): Promise<MotionType>;
  setDamping(bodyId: BodyId, opts: Partial<DampingSettings>): Promise<void>;
  getDamping(bodyId: BodyId): Promise<DampingSettings>;
  activateBody(bodyId: BodyId): Promise<void>;
  deactivateBody(bodyId: BodyId): Promise<void>;
  setBodySensor(bodyId: BodyId, isSensor: boolean): Promise<void>;
  isBodySensor(bodyId: BodyId): Promise<boolean>;
  getCenterOfMassPosition(bodyId: BodyId): Promise<Vec3>;

  rayCastClosest(opts: { origin: Vec3; direction: Vec3; maxDistance: number; filter?: QueryFilter }): Promise<RayCastHit | null>;
  rayCastAll(opts: { origin: Vec3; direction: Vec3; maxDistance: number; filter?: QueryFilter }): Promise<RayCastHit[]>;
  collideSphereAll(opts: { center: Vec3; radius: number; maxSeparation?: number; filter?: QueryFilter }): Promise<SphereCollideHit[]>;
  castSphereAll(opts: { origin: Vec3; direction: Vec3; maxDistance: number; radius: number; filter?: QueryFilter }): Promise<RayCastHit[]>;
  queryAABB(opts: { min: Vec3; max: Vec3; filter?: QueryFilter }): Promise<AABBHit[]>;

  createFixedConstraint(bodyA: BodyId, bodyB: BodyId): Promise<ConstraintId>;
  createHingeConstraint(bodyA: BodyId, bodyB: BodyId, anchor: Vec3, axis: Vec3, normal: Vec3): Promise<ConstraintId>;
  createSliderConstraint(bodyA: BodyId, bodyB: BodyId, anchor: Vec3, axis: Vec3, normal: Vec3, min?: number, max?: number): Promise<ConstraintId>;
  createPointConstraint(bodyA: BodyId, bodyB: BodyId, point: Vec3): Promise<ConstraintId>;
  createDistanceConstraint(bodyA: BodyId, bodyB: BodyId, pointA: Vec3, pointB: Vec3, minDistance?: number, maxDistance?: number): Promise<ConstraintId>;
  createConeConstraint(bodyA: BodyId, bodyB: BodyId, point: Vec3, twistAxis: Vec3, halfConeAngle: number): Promise<ConstraintId>;
  createSwingTwistConstraint(bodyA: BodyId, bodyB: BodyId, point: Vec3, twistAxis: Vec3, planeAxis: Vec3, limits?: Partial<SwingTwistLimits>): Promise<ConstraintId>;
  createSixDOFConstraint(bodyA: BodyId, bodyB: BodyId, point: Vec3, axisX: Vec3, axisY: Vec3): Promise<ConstraintId>;
  removeConstraint(constraintId: ConstraintId): Promise<void>;
  setHingeLimits(constraintId: ConstraintId, minAngle: number, maxAngle: number): Promise<void>;
  setHingeMotor(constraintId: ConstraintId, opts?: HingeMotorOpts): Promise<void>;
  getHingeAngle(constraintId: ConstraintId): Promise<number>;
  setSliderLimits(constraintId: ConstraintId, minLimit: number, maxLimit: number): Promise<void>;
  setSliderMotor(constraintId: ConstraintId, opts?: SliderMotorOpts): Promise<void>;
  getSliderPosition(constraintId: ConstraintId): Promise<number>;
  setSwingTwistLimits(constraintId: ConstraintId, limits: SwingTwistLimits): Promise<void>;
  setSwingTwistMotor(constraintId: ConstraintId, opts?: SwingTwistMotorOpts): Promise<void>;
  setSixDOFLimits(constraintId: ConstraintId, limits: SixDOFLimits): Promise<void>;
  setSixDOFMotorState(constraintId: ConstraintId, axis: number, state: MotorState): Promise<void>;

  setGravity(gravity: number): Promise<void>;

  /**
   * Gracefully destroy the worker world and terminate the Worker Thread.
   * No further method calls are valid after this resolves.
   */
  terminate(): Promise<void>;
}
