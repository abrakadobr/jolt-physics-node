'use strict';

const path = require('path');
const native = require(path.join(__dirname, 'build/Release/jolt_backend.node'));

const kHandle = Symbol('jolt_world_handle');

function num(v, n) {
  if (typeof v !== 'number' || Number.isNaN(v)) throw new TypeError(`${n} must be a number`);
  return v;
}

function pos(v, n) {
  const x = num(v?.x, `${n}.x`);
  const y = num(v?.y, `${n}.y`);
  const z = num(v?.z, `${n}.z`);
  return { x, y, z };
}

function quat(v, n) {
  const x = num(v?.x, `${n}.x`);
  const y = num(v?.y, `${n}.y`);
  const z = num(v?.z, `${n}.z`);
  const w = num(v?.w, `${n}.w`);
  return { x, y, z, w };
}

// Throw if a setter/action returned false (body/constraint not found).
function ok(result, name) {
  if (!result) throw new Error(name + ': not found');
}

// Throw if a getter returned null (constraint/body not found); otherwise return the value.
function got(result, name) {
  if (result === null) throw new Error(name + ': not found');
  return result;
}

function normQuat(v, n) {
  const q = quat(v, n);
  const len = Math.hypot(q.x, q.y, q.z, q.w);
  if (!Number.isFinite(len) || len <= 1e-12) throw new TypeError(`${n} must be non-zero quaternion`);
  return {
    x: q.x / len,
    y: q.y / len,
    z: q.z / len,
    w: q.w / len
  };
}

class Skeleton {
  constructor(world) {
    this.world = world;
    this.id = native.createSkeleton(world._handle());
  }

  addJoint(name, parentIndex = -1) {
    if (typeof name !== 'string' || !name.length) throw new TypeError('name must be non-empty string');
    native.addSkeletonJoint(this.world._handle(), this.id, name, parentIndex | 0);
    return this;
  }

  finalize() {
    return native.finalizeSkeleton(this.world._handle(), this.id);
  }

  getJointCount() {
    return native.getSkeletonJointCount(this.world._handle(), this.id);
  }

  getJointInfo(jointIndex) {
    return native.getSkeletonJointInfo(this.world._handle(), this.id, jointIndex | 0);
  }

  getJointIndex(name) {
    if (typeof name !== 'string' || !name.length) throw new TypeError('name must be non-empty string');
    return native.getSkeletonJointIndex(this.world._handle(), this.id, name);
  }

  createRagdollSettings({ capsuleHalfHeight = 0.2, capsuleRadius = 0.1, spacing = 0.4 } = {}) {
    const settingsId = native.createRagdollSettings(
      this.world._handle(),
      this.id,
      num(capsuleHalfHeight, 'capsuleHalfHeight'),
      num(capsuleRadius, 'capsuleRadius'),
      num(spacing, 'spacing')
    );
    return new RagdollSettings(this.world, settingsId);
  }
}

class RagdollSettings {
  constructor(world, id) {
    this.world = world;
    this.id = id >>> 0;
  }

  createRagdoll({ collisionGroup = 1, userData = 0, activate = true } = {}) {
    const id = native.createRagdoll(this.world._handle(), this.id, collisionGroup >>> 0, userData >>> 0, Boolean(activate));
    return new Ragdoll(this.world, id);
  }

  // Set the shape for a specific joint in the ragdoll settings.
  // config: { kind: 'capsule'|'box'|'sphere', halfHeight?, radius?, halfExtents? }
  setJointShape(jointIndex, config = {}) {
    ok(native.setRagdollJointShape(this.world._handle(), this.id, jointIndex | 0, config), 'setJointShape');
  }

  // Override the world-space position and rotation of a joint's body in the settings.
  setJointTransform(jointIndex, position, rotation) {
    const p = pos(position, 'position');
    const q = quat(rotation, 'rotation');
    ok(native.setRagdollJointTransform(
      this.world._handle(), this.id, jointIndex | 0,
      p.x, p.y, p.z, q.x, q.y, q.z, q.w
    ), 'setJointTransform');
  }

  // Set the constraint type for a joint (joint 0 = root, which has no parent — will be ignored).
  // config: { type: 'fixed'|'swingTwist'|'hinge'|'cone', normalHalfCone?, planeHalfCone?,
  //           twistMin?, twistMax?, minAngle?, maxAngle?, halfConeAngle?,
  //           twistAxis1?, twistAxis2?, planeAxis1?, planeAxis2?,
  //           hingeAxis1?, hingeAxis2?, normalAxis1?, normalAxis2?,
  //           coneAxis1?, coneAxis2?, autoAxes? }
  setJointConstraint(jointIndex, config = {}) {
    ok(native.setRagdollJointConstraint(this.world._handle(), this.id, jointIndex | 0, config), 'setJointConstraint');
  }
}

class Ragdoll {
  constructor(world, id) {
    this.world = world;
    this.id = id >>> 0;
    this._destroyed = false;
  }

  _alive() {
    if (this._destroyed) throw new Error('Ragdoll is destroyed');
  }

  bodyCount() {
    this._alive();
    return native.getRagdollBodyCount(this.world._handle(), this.id);
  }

  getBoneBodyId(index) {
    this._alive();
    return native.getRagdollBoneBodyId(this.world._handle(), this.id, index | 0);
  }

  getBoneTransform(index) {
    this._alive();
    return native.getRagdollBoneTransform(this.world._handle(), this.id, index | 0);
  }

  setBoneTransform(index, { position, rotation }) {
    this._alive();
    const p = pos(position, 'position');
    const q = quat(rotation, 'rotation');
    ok(native.setRagdollBoneTransform(this.world._handle(), this.id, index | 0, p.x, p.y, p.z, q.x, q.y, q.z, q.w), 'setBoneTransform');
  }

  syncToSkeletonPose() {
    this._alive();
    const count = this.bodyCount();
    const bones = [];
    for (let i = 0; i < count; i += 1) bones.push(this.getBoneTransform(i));
    return bones;
  }

  syncFromSkeletonPose(bones) {
    this._alive();
    if (!Array.isArray(bones)) throw new TypeError('bones must be array');
    for (let i = 0; i < bones.length; i += 1) this.setBoneTransform(i, bones[i]);
  }

  // Returns an array of constraint IDs indexed by joint index.
  // Index 0 (root joint) is always 0 (no parent constraint).
  // Use the returned IDs with setSwingTwistMotor / setHingeMotor etc.
  getConstraintIds() {
    this._alive();
    return native.getRagdollConstraintIds(this.world._handle(), this.id);
  }

  destroy() {
    if (!this._destroyed) {
      native.destroyRagdoll(this.world._handle(), this.id);
      this._destroyed = true;
    }
  }
}

class World {
  constructor(options = {}) {
    const gravity = options.gravity == null ? 9.81 : num(options.gravity, 'gravity');
    this[kHandle] = native.createWorld(gravity);
    this._destroyed = false;
  }

  _handle() {
    if (this._destroyed || !this[kHandle]) throw new Error('World is destroyed');
    return this[kHandle];
  }

  destroy() {
    if (!this._destroyed) {
      native.destroyWorld(this._handle());
      this[kHandle] = null;
      this._destroyed = true;
    }
  }

  step(dt = 1 / 60) {
    native.step(this._handle(), num(dt, 'dt'));
  }

  setGravity(gravity) {
    native.setGravity(this._handle(), num(gravity, 'gravity'));
  }

  onBodyActivation(callback) {
    if (callback != null && typeof callback !== 'function') throw new TypeError('callback must be function or null');
    native.setBodyActivationCallback(this._handle(), callback ?? null);
  }

  onContact(callback) {
    if (callback != null && typeof callback !== 'function') throw new TypeError('callback must be function or null');
    native.setContactCallback(this._handle(), callback ?? null);
  }

  createSphere({ radius, position, dynamic = true, restitution = 0.2, friction = 0.5 }) {
    const p = pos(position, 'position');
    return native.createSphere(this._handle(), num(radius, 'radius'), p.x, p.y, p.z, Boolean(dynamic), num(restitution, 'restitution'), num(friction, 'friction'));
  }

  createBox({ halfExtents, position, dynamic = true, restitution = 0.2, friction = 0.5 }) {
    const h = pos(halfExtents, 'halfExtents');
    const p = pos(position, 'position');
    return native.createBox(
      this._handle(),
      num(h.x, 'halfExtents.x'),
      num(h.y, 'halfExtents.y'),
      num(h.z, 'halfExtents.z'),
      p.x,
      p.y,
      p.z,
      Boolean(dynamic),
      num(restitution, 'restitution'),
      num(friction, 'friction')
    );
  }

  createCapsule({ halfHeight, radius, position, dynamic = true, restitution = 0.2, friction = 0.5 }) {
    const p = pos(position, 'position');
    return native.createCapsule(
      this._handle(),
      num(halfHeight, 'halfHeight'),
      num(radius, 'radius'),
      p.x,
      p.y,
      p.z,
      Boolean(dynamic),
      num(restitution, 'restitution'),
      num(friction, 'friction')
    );
  }

  createCylinder({ halfHeight, radius, position, dynamic = true, restitution = 0.2, friction = 0.5 }) {
    const p = pos(position, 'position');
    return native.createCylinder(
      this._handle(),
      num(halfHeight, 'halfHeight'),
      num(radius, 'radius'),
      p.x,
      p.y,
      p.z,
      Boolean(dynamic),
      num(restitution, 'restitution'),
      num(friction, 'friction')
    );
  }

  createTaperedCapsule({ halfHeight, topRadius, bottomRadius, position, dynamic = true, restitution = 0.2, friction = 0.5 }) {
    const p = pos(position, 'position');
    return native.createTaperedCapsule(
      this._handle(),
      num(halfHeight, 'halfHeight'),
      num(topRadius, 'topRadius'),
      num(bottomRadius, 'bottomRadius'),
      p.x,
      p.y,
      p.z,
      Boolean(dynamic),
      num(restitution, 'restitution'),
      num(friction, 'friction')
    );
  }

  createTaperedCylinder({ halfHeight, topRadius, bottomRadius, position, dynamic = true, restitution = 0.2, friction = 0.5 }) {
    const p = pos(position, 'position');
    return native.createTaperedCylinder(
      this._handle(),
      num(halfHeight, 'halfHeight'),
      num(topRadius, 'topRadius'),
      num(bottomRadius, 'bottomRadius'),
      p.x,
      p.y,
      p.z,
      Boolean(dynamic),
      num(restitution, 'restitution'),
      num(friction, 'friction')
    );
  }

  createConvexHull({ points, position, dynamic = true, restitution = 0.2, friction = 0.5 }) {
    if (!Array.isArray(points) || points.length < 12 || points.length % 3 !== 0) {
      throw new TypeError('points must be flat number array [x,y,z,...] with at least 4 points');
    }
    const p = pos(position, 'position');
    return native.createConvexHull(this._handle(), points, p.x, p.y, p.z, Boolean(dynamic), num(restitution, 'restitution'), num(friction, 'friction'));
  }

  createMesh({ vertices, indices, position, friction = 0.5, materialIndices, materials, restitution }) {
    if (!Array.isArray(vertices) || !Array.isArray(indices)) throw new TypeError('vertices/indices must be arrays');
    const p = pos(position, 'position');
    const hasOpts = materialIndices !== undefined || materials !== undefined || restitution !== undefined;
    const opts = hasOpts ? { materialIndices, materials, restitution } : undefined;
    return opts !== undefined
      ? native.createMesh(this._handle(), vertices, indices, p.x, p.y, p.z, num(friction, 'friction'), opts)
      : native.createMesh(this._handle(), vertices, indices, p.x, p.y, p.z, num(friction, 'friction'));
  }

  getBodyPosition(bodyId) {
    return native.getBodyPosition(this._handle(), bodyId >>> 0);
  }

  getBodyRotation(bodyId) {
    return native.getBodyRotation(this._handle(), bodyId >>> 0);
  }

  setBodyPosition(bodyId, position, activate = true) {
    const p = pos(position, 'position');
    ok(native.setBodyPosition(this._handle(), bodyId >>> 0, p.x, p.y, p.z, Boolean(activate)), 'setBodyPosition');
  }

  setBodyRotation(bodyId, rotation, activate = true) {
    const q = normQuat(rotation, 'rotation');
    ok(native.setBodyRotation(this._handle(), bodyId >>> 0, q.x, q.y, q.z, q.w, Boolean(activate)), 'setBodyRotation');
  }

  getLinearVelocity(bodyId) {
    return native.getLinearVelocity(this._handle(), bodyId >>> 0);
  }

  setLinearVelocity(bodyId, velocity) {
    const v = pos(velocity, 'velocity');
    ok(native.setLinearVelocity(this._handle(), bodyId >>> 0, v.x, v.y, v.z), 'setLinearVelocity');
  }

  getAngularVelocity(bodyId) {
    return native.getAngularVelocity(this._handle(), bodyId >>> 0);
  }

  setAngularVelocity(bodyId, velocity) {
    const v = pos(velocity, 'velocity');
    ok(native.setAngularVelocity(this._handle(), bodyId >>> 0, v.x, v.y, v.z), 'setAngularVelocity');
  }

  applyImpulse(bodyId, impulse) {
    const i = pos(impulse, 'impulse');
    ok(native.applyImpulse(this._handle(), bodyId >>> 0, i.x, i.y, i.z), 'applyImpulse');
  }

  addAngularImpulse(bodyId, impulse) {
    const i = pos(impulse, 'impulse');
    ok(native.addAngularImpulse(this._handle(), bodyId >>> 0, i.x, i.y, i.z), 'addAngularImpulse');
  }

  addForce(bodyId, force) {
    const f = pos(force, 'force');
    ok(native.addForce(this._handle(), bodyId >>> 0, f.x, f.y, f.z), 'addForce');
  }

  addTorque(bodyId, torque) {
    const t = pos(torque, 'torque');
    ok(native.addTorque(this._handle(), bodyId >>> 0, t.x, t.y, t.z), 'addTorque');
  }

  setFriction(bodyId, friction) {
    ok(native.setFriction(this._handle(), bodyId >>> 0, num(friction, 'friction')), 'setFriction');
  }

  getFriction(bodyId) {
    return native.getFriction(this._handle(), bodyId >>> 0);
  }

  setRestitution(bodyId, restitution) {
    ok(native.setRestitution(this._handle(), bodyId >>> 0, num(restitution, 'restitution')), 'setRestitution');
  }

  getRestitution(bodyId) {
    return native.getRestitution(this._handle(), bodyId >>> 0);
  }

  setGravityFactor(bodyId, factor) {
    ok(native.setGravityFactor(this._handle(), bodyId >>> 0, num(factor, 'factor')), 'setGravityFactor');
  }

  getGravityFactor(bodyId) {
    return native.getGravityFactor(this._handle(), bodyId >>> 0);
  }

  setMotionType(bodyId, motionType, activate = true) {
    ok(native.setMotionType(this._handle(), bodyId >>> 0, motionType | 0, Boolean(activate)), 'setMotionType');
  }

  getMotionType(bodyId) {
    return native.getMotionType(this._handle(), bodyId >>> 0);
  }

  setMotionQuality(bodyId, quality) {
    ok(native.setMotionQuality(this._handle(), bodyId >>> 0, quality | 0), 'setMotionQuality');
  }

  getMotionQuality(bodyId) {
    return native.getMotionQuality(this._handle(), bodyId >>> 0);
  }

  setObjectLayer(bodyId, layer) {
    ok(native.setObjectLayer(this._handle(), bodyId >>> 0, layer >>> 0), 'setObjectLayer');
  }

  getObjectLayer(bodyId) {
    return native.getObjectLayer(this._handle(), bodyId >>> 0);
  }

  setDamping(bodyId, { linear = 0.05, angular = 0.05 }) {
    ok(native.setDamping(this._handle(), bodyId >>> 0, num(linear, 'linear'), num(angular, 'angular')), 'setDamping');
  }

  getDamping(bodyId) {
    return native.getDamping(this._handle(), bodyId >>> 0);
  }

  activateBody(bodyId) {
    ok(native.activateBody(this._handle(), bodyId >>> 0), 'activateBody');
  }

  deactivateBody(bodyId) {
    ok(native.deactivateBody(this._handle(), bodyId >>> 0), 'deactivateBody');
  }

  removeBody(bodyId) {
    ok(native.removeBody(this._handle(), bodyId >>> 0), 'removeBody');
  }

  hasBody(bodyId) {
    return native.hasBody(this._handle(), bodyId >>> 0);
  }

  isBodyActive(bodyId) {
    return native.isBodyActive(this._handle(), bodyId >>> 0);
  }

  setBodySensor(bodyId, isSensor) {
    ok(native.setBodySensor(this._handle(), bodyId >>> 0, Boolean(isSensor)), 'setBodySensor');
  }

  isBodySensor(bodyId) {
    return native.isBodySensor(this._handle(), bodyId >>> 0);
  }

  getCenterOfMassPosition(bodyId) {
    return native.getCenterOfMassPosition(this._handle(), bodyId >>> 0);
  }

  rayCastClosest({ origin, direction, maxDistance, filter }) {
    const o = pos(origin, 'origin');
    const d = pos(direction, 'direction');
    const args = [this._handle(), o.x, o.y, o.z, d.x, d.y, d.z, num(maxDistance, 'maxDistance')];
    if (filter != null) args.push(filter);
    return native.rayCastClosest(...args);
  }

  rayCastAll({ origin, direction, maxDistance, filter }) {
    const o = pos(origin, 'origin');
    const d = pos(direction, 'direction');
    const args = [this._handle(), o.x, o.y, o.z, d.x, d.y, d.z, num(maxDistance, 'maxDistance')];
    if (filter != null) args.push(filter);
    return native.rayCastAll(...args);
  }

  queryAABB({ min, max, filter }) {
    const minPos = pos(min, 'min');
    const maxPos = pos(max, 'max');
    const args = [this._handle(), minPos.x, minPos.y, minPos.z, maxPos.x, maxPos.y, maxPos.z];
    if (filter != null) args.push(filter);
    return native.queryAABB(...args);
  }

  areBodiesInContact(bodyA, bodyB) {
    return native.areBodiesInContact(this._handle(), bodyA >>> 0, bodyB >>> 0);
  }

  collideSphereAll({ center, radius, maxSeparation = 0, filter }) {
    const c = pos(center, 'center');
    const args = [this._handle(), c.x, c.y, c.z, num(radius, 'radius'), num(maxSeparation, 'maxSeparation')];
    if (filter != null) args.push(filter);
    return native.collideSphereAll(...args);
  }

  castSphereAll({ origin, direction, maxDistance, radius, filter }) {
    const o = pos(origin, 'origin');
    const d = pos(direction, 'direction');
    const args = [this._handle(), o.x, o.y, o.z, d.x, d.y, d.z, num(maxDistance, 'maxDistance'), num(radius, 'radius')];
    if (filter != null) args.push(filter);
    return native.castSphereAll(...args);
  }

  createFixedConstraint(bodyA, bodyB) {
    return native.createFixedConstraint(this._handle(), bodyA >>> 0, bodyB >>> 0);
  }

  createDistanceConstraint(bodyA, bodyB, pointA, pointB, minDistance = 0, maxDistance = 0) {
    const a = pos(pointA, 'pointA');
    const b = pos(pointB, 'pointB');
    return native.createDistanceConstraint(
      this._handle(),
      bodyA >>> 0,
      bodyB >>> 0,
      a.x,
      a.y,
      a.z,
      b.x,
      b.y,
      b.z,
      num(minDistance, 'minDistance'),
      num(maxDistance, 'maxDistance')
    );
  }

  createHingeConstraint(bodyA, bodyB, anchor, axis, normal) {
    const p = pos(anchor, 'anchor');
    const a = pos(axis, 'axis');
    const n = pos(normal, 'normal');
    return native.createHingeConstraint(this._handle(), bodyA >>> 0, bodyB >>> 0, p.x, p.y, p.z, a.x, a.y, a.z, n.x, n.y, n.z);
  }

  createSliderConstraint(bodyA, bodyB, anchor, axis, normal, min = -1, max = 1) {
    const p = pos(anchor, 'anchor');
    const a = pos(axis, 'axis');
    const n = pos(normal, 'normal');
    return native.createSliderConstraint(
      this._handle(),
      bodyA >>> 0,
      bodyB >>> 0,
      p.x,
      p.y,
      p.z,
      a.x,
      a.y,
      a.z,
      n.x,
      n.y,
      n.z,
      num(min, 'min'),
      num(max, 'max')
    );
  }

  removeConstraint(constraintId) {
    ok(native.removeConstraint(this._handle(), constraintId >>> 0), 'removeConstraint');
  }

  createPointConstraint(bodyA, bodyB, point) {
    const p = pos(point, 'point');
    return native.createPointConstraint(this._handle(), bodyA >>> 0, bodyB >>> 0, p.x, p.y, p.z);
  }

  createConeConstraint(bodyA, bodyB, point, twistAxis, halfConeAngle) {
    const p = pos(point, 'point');
    const a = pos(twistAxis, 'twistAxis');
    return native.createConeConstraint(this._handle(), bodyA >>> 0, bodyB >>> 0, p.x, p.y, p.z, a.x, a.y, a.z, num(halfConeAngle, 'halfConeAngle'));
  }

  createSwingTwistConstraint(bodyA, bodyB, point, twistAxis, planeAxis, limits = {}) {
    const p = pos(point, 'point');
    const t = pos(twistAxis, 'twistAxis');
    const pl = pos(planeAxis, 'planeAxis');
    const {
      normalHalfCone = 0.8,
      planeHalfCone = 0.8,
      twistMin = -0.5,
      twistMax = 0.5
    } = limits;
    return native.createSwingTwistConstraint(
      this._handle(),
      bodyA >>> 0,
      bodyB >>> 0,
      p.x,
      p.y,
      p.z,
      t.x,
      t.y,
      t.z,
      pl.x,
      pl.y,
      pl.z,
      num(normalHalfCone, 'normalHalfCone'),
      num(planeHalfCone, 'planeHalfCone'),
      num(twistMin, 'twistMin'),
      num(twistMax, 'twistMax')
    );
  }

  createGearConstraint(bodyA, bodyB, hingeAxis1, hingeAxis2, ratio = 1, gear1ConstraintId = 0, gear2ConstraintId = 0) {
    const h1 = pos(hingeAxis1, 'hingeAxis1');
    const h2 = pos(hingeAxis2, 'hingeAxis2');
    return native.createGearConstraint(
      this._handle(), bodyA >>> 0, bodyB >>> 0,
      h1.x, h1.y, h1.z, h2.x, h2.y, h2.z,
      num(ratio, 'ratio'), gear1ConstraintId >>> 0, gear2ConstraintId >>> 0
    );
  }

  createPulleyConstraint(bodyA, bodyB, bodyPoint1, fixedPoint1, bodyPoint2, fixedPoint2, { ratio = 1, minLength = 0, maxLength = -1 } = {}) {
    const bp1 = pos(bodyPoint1, 'bodyPoint1');
    const fp1 = pos(fixedPoint1, 'fixedPoint1');
    const bp2 = pos(bodyPoint2, 'bodyPoint2');
    const fp2 = pos(fixedPoint2, 'fixedPoint2');
    return native.createPulleyConstraint(
      this._handle(), bodyA >>> 0, bodyB >>> 0,
      bp1.x, bp1.y, bp1.z, fp1.x, fp1.y, fp1.z,
      bp2.x, bp2.y, bp2.z, fp2.x, fp2.y, fp2.z,
      num(ratio, 'ratio'), num(minLength, 'minLength'), num(maxLength, 'maxLength')
    );
  }

  createSixDOFConstraint(bodyA, bodyB, point, axisX, axisY) {
    const p = pos(point, 'point');
    const x = pos(axisX, 'axisX');
    const y = pos(axisY, 'axisY');
    return native.createSixDOFConstraint(this._handle(), bodyA >>> 0, bodyB >>> 0, p.x, p.y, p.z, x.x, x.y, x.z, y.x, y.y, y.z);
  }

  setHingeLimits(constraintId, minAngle, maxAngle) {
    ok(native.setHingeLimits(this._handle(), constraintId >>> 0, num(minAngle, 'minAngle'), num(maxAngle, 'maxAngle')), 'setHingeLimits');
  }

  setSliderLimits(constraintId, minLimit, maxLimit) {
    ok(native.setSliderLimits(this._handle(), constraintId >>> 0, num(minLimit, 'minLimit'), num(maxLimit, 'maxLimit')), 'setSliderLimits');
  }

  setHingeMotor(constraintId, { state = 1, targetVelocity = 0, targetAngle = 0, maxTorque = 100 } = {}) {
    ok(native.setHingeMotor(
      this._handle(),
      constraintId >>> 0,
      state | 0,
      num(targetVelocity, 'targetVelocity'),
      num(targetAngle, 'targetAngle'),
      num(maxTorque, 'maxTorque')
    ), 'setHingeMotor');
  }

  setSliderMotor(constraintId, { state = 1, targetVelocity = 0, targetPosition = 0, maxForce = 100 } = {}) {
    ok(native.setSliderMotor(
      this._handle(),
      constraintId >>> 0,
      state | 0,
      num(targetVelocity, 'targetVelocity'),
      num(targetPosition, 'targetPosition'),
      num(maxForce, 'maxForce')
    ), 'setSliderMotor');
  }

  setConeHalfAngle(constraintId, halfConeAngle) {
    ok(native.setConeHalfAngle(this._handle(), constraintId >>> 0, num(halfConeAngle, 'halfConeAngle')), 'setConeHalfAngle');
  }

  setSwingTwistLimits(constraintId, { normalHalfCone, planeHalfCone, twistMin, twistMax }) {
    ok(native.setSwingTwistLimits(
      this._handle(),
      constraintId >>> 0,
      num(normalHalfCone, 'normalHalfCone'),
      num(planeHalfCone, 'planeHalfCone'),
      num(twistMin, 'twistMin'),
      num(twistMax, 'twistMax')
    ), 'setSwingTwistLimits');
  }

  setSwingTwistMotor(
    constraintId,
    {
      swingState = 1,
      twistState = 1,
      targetAngularVelocity = { x: 0, y: 0, z: 0 },
      targetOrientation = { x: 0, y: 0, z: 0, w: 1 },
      maxTorque = 100
    } = {}
  ) {
    const v = pos(targetAngularVelocity, 'targetAngularVelocity');
    const q = normQuat(targetOrientation, 'targetOrientation');
    ok(native.setSwingTwistMotor(
      this._handle(),
      constraintId >>> 0,
      swingState | 0,
      twistState | 0,
      v.x,
      v.y,
      v.z,
      q.x,
      q.y,
      q.z,
      q.w,
      num(maxTorque, 'maxTorque')
    ), 'setSwingTwistMotor');
  }

  setSixDOFLimits(constraintId, { translationMin, translationMax, rotationMin, rotationMax }) {
    const tmin = pos(translationMin, 'translationMin');
    const tmax = pos(translationMax, 'translationMax');
    const rmin = pos(rotationMin, 'rotationMin');
    const rmax = pos(rotationMax, 'rotationMax');
    ok(native.setSixDOFLimits(this._handle(), constraintId >>> 0, tmin.x, tmin.y, tmin.z, tmax.x, tmax.y, tmax.z, rmin.x, rmin.y, rmin.z, rmax.x, rmax.y, rmax.z), 'setSixDOFLimits');
  }

  setSixDOFMotorState(constraintId, axis, state) {
    ok(native.setSixDOFMotorState(this._handle(), constraintId >>> 0, axis | 0, state | 0), 'setSixDOFMotorState');
  }

  setSixDOFTargetVelocity(constraintId, linear, angular) {
    const l = pos(linear, 'linear');
    const a = pos(angular, 'angular');
    ok(native.setSixDOFTargetVelocity(this._handle(), constraintId >>> 0, l.x, l.y, l.z, a.x, a.y, a.z), 'setSixDOFTargetVelocity');
  }

  setSixDOFTargetPose(constraintId, position, orientation) {
    const p = pos(position, 'position');
    const q = normQuat(orientation, 'orientation');
    ok(native.setSixDOFTargetPose(this._handle(), constraintId >>> 0, p.x, p.y, p.z, q.x, q.y, q.z, q.w), 'setSixDOFTargetPose');
  }

  // Motor spring/damping settings.
  // opts: { frequency?, stiffness?, damping?, mode?, maxForce?, minForce?, maxTorque?, minTorque? }
  setHingeMotorSpring(constraintId, opts = {}) {
    ok(native.setHingeMotorSpring(this._handle(), constraintId >>> 0, opts), 'setHingeMotorSpring');
  }

  setSliderMotorSpring(constraintId, opts = {}) {
    ok(native.setSliderMotorSpring(this._handle(), constraintId >>> 0, opts), 'setSliderMotorSpring');
  }

  setSwingMotorSpring(constraintId, opts = {}) {
    ok(native.setSwingMotorSpring(this._handle(), constraintId >>> 0, opts), 'setSwingMotorSpring');
  }

  setTwistMotorSpring(constraintId, opts = {}) {
    ok(native.setTwistMotorSpring(this._handle(), constraintId >>> 0, opts), 'setTwistMotorSpring');
  }

  setSixDOFMotorSpring(constraintId, axis, opts = {}) {
    ok(native.setSixDOFMotorSpring(this._handle(), constraintId >>> 0, axis | 0, opts), 'setSixDOFMotorSpring');
  }

  getHingeMotorSpring(constraintId) {
    return got(native.getHingeMotorSpring(this._handle(), constraintId >>> 0), 'getHingeMotorSpring');
  }

  getSliderMotorSpring(constraintId) {
    return got(native.getSliderMotorSpring(this._handle(), constraintId >>> 0), 'getSliderMotorSpring');
  }

  getSwingMotorSpring(constraintId) {
    return got(native.getSwingMotorSpring(this._handle(), constraintId >>> 0), 'getSwingMotorSpring');
  }

  getTwistMotorSpring(constraintId) {
    return got(native.getTwistMotorSpring(this._handle(), constraintId >>> 0), 'getTwistMotorSpring');
  }

  getSixDOFMotorSpring(constraintId, axis) {
    return got(native.getSixDOFMotorSpring(this._handle(), constraintId >>> 0, axis | 0), 'getSixDOFMotorSpring');
  }

  getHingeAngle(constraintId) {
    return got(native.getHingeAngle(this._handle(), constraintId >>> 0), 'getHingeAngle');
  }

  getHingeMotorState(constraintId) {
    return got(native.getHingeMotorState(this._handle(), constraintId >>> 0), 'getHingeMotorState');
  }

  getSliderPosition(constraintId) {
    return got(native.getSliderPosition(this._handle(), constraintId >>> 0), 'getSliderPosition');
  }

  getSliderMotorState(constraintId) {
    return got(native.getSliderMotorState(this._handle(), constraintId >>> 0), 'getSliderMotorState');
  }

  getSixDOFRotation(constraintId) {
    return got(native.getSixDOFRotation(this._handle(), constraintId >>> 0), 'getSixDOFRotation');
  }

  getSixDOFLimits(constraintId) {
    return got(native.getSixDOFLimits(this._handle(), constraintId >>> 0), 'getSixDOFLimits');
  }

  getSixDOFMotorState(constraintId, axis) {
    return got(native.getSixDOFMotorState(this._handle(), constraintId >>> 0, axis | 0), 'getSixDOFMotorState');
  }

  getSwingTwistRotation(constraintId) {
    return got(native.getSwingTwistRotation(this._handle(), constraintId >>> 0), 'getSwingTwistRotation');
  }

  getSwingTwistMotorState(constraintId) {
    return got(native.getSwingTwistMotorState(this._handle(), constraintId >>> 0), 'getSwingTwistMotorState');
  }

  getHingeLimits(constraintId) {
    return got(native.getHingeLimits(this._handle(), constraintId >>> 0), 'getHingeLimits');
  }

  getSliderLimits(constraintId) {
    return got(native.getSliderLimits(this._handle(), constraintId >>> 0), 'getSliderLimits');
  }

  getSwingTwistLimits(constraintId) {
    return got(native.getSwingTwistLimits(this._handle(), constraintId >>> 0), 'getSwingTwistLimits');
  }

  getPulleyLength(constraintId) {
    return got(native.getPulleyLength(this._handle(), constraintId >>> 0), 'getPulleyLength');
  }

  setPulleyLength(constraintId, minLength, maxLength) {
    ok(native.setPulleyLength(this._handle(), constraintId >>> 0, num(minLength, 'minLength'), num(maxLength, 'maxLength')), 'setPulleyLength');
  }

  getPulleyLengthLimits(constraintId) {
    return got(native.getPulleyLengthLimits(this._handle(), constraintId >>> 0), 'getPulleyLengthLimits');
  }

  createPathConstraint(bodyA, bodyB, opts = {}) {
    const {
      points = [],
      closed = false,
      pathPosition = { x: 0, y: 0, z: 0 },
      pathRotation = { x: 0, y: 0, z: 0, w: 1 },
      pathFraction = 0,
      maxFriction = 0,
      rotationType = 'free'
    } = opts;
    const ROTATION_TYPES = { free: 0, tangent: 1, normal: 2, binormal: 3, toPath: 4, full: 5 };
    const rotType = typeof rotationType === 'number' ? rotationType : (ROTATION_TYPES[rotationType] ?? 0);
    const flat = [];
    for (const pt of points) {
      const p = pos(pt.position, 'point.position');
      const t = pos(pt.tangent, 'point.tangent');
      const n = pos(pt.normal, 'point.normal');
      flat.push(p.x, p.y, p.z, t.x, t.y, t.z, n.x, n.y, n.z);
    }
    const pp = pos(pathPosition, 'pathPosition');
    const pr = quat(pathRotation, 'pathRotation');
    const id = native.createPathConstraint(
      this._handle(), bodyA >>> 0, bodyB >>> 0,
      flat, closed,
      pp.x, pp.y, pp.z,
      pr.x, pr.y, pr.z, pr.w,
      pathFraction, maxFriction, rotType
    );
    if (!id) throw new Error('createPathConstraint: failed (bodies not found)');
    return id;
  }

  getPathFraction(constraintId) {
    return got(native.getPathFraction(this._handle(), constraintId >>> 0), 'getPathFraction');
  }

  getPathMaxFraction(constraintId) {
    return got(native.getPathMaxFraction(this._handle(), constraintId >>> 0), 'getPathMaxFraction');
  }

  setPathMotor(constraintId, opts = {}) {
    const { state = 0, targetVelocity, targetFraction } = opts;
    const args = [this._handle(), constraintId >>> 0, state | 0];
    if (targetVelocity !== undefined || targetFraction !== undefined) {
      args.push(targetVelocity !== undefined ? targetVelocity : 0);
    }
    if (targetFraction !== undefined) args.push(targetFraction);
    ok(native.setPathMotor(...args), 'setPathMotor');
  }

  getPathMotorState(constraintId) {
    return got(native.getPathMotorState(this._handle(), constraintId >>> 0), 'getPathMotorState');
  }

  setPathMotorSpring(constraintId, opts = {}) {
    ok(native.setPathMotorSpring(this._handle(), constraintId >>> 0, opts), 'setPathMotorSpring');
  }

  getPathMotorSpring(constraintId) {
    return got(native.getPathMotorSpring(this._handle(), constraintId >>> 0), 'getPathMotorSpring');
  }

  getHingeLambdas(constraintId) {
    return got(native.getHingeLambdas(this._handle(), constraintId >>> 0), 'getHingeLambdas');
  }

  getSliderLambdas(constraintId) {
    return got(native.getSliderLambdas(this._handle(), constraintId >>> 0), 'getSliderLambdas');
  }

  getSwingTwistLambdas(constraintId) {
    return got(native.getSwingTwistLambdas(this._handle(), constraintId >>> 0), 'getSwingTwistLambdas');
  }

  getSixDOFLambdas(constraintId) {
    return got(native.getSixDOFLambdas(this._handle(), constraintId >>> 0), 'getSixDOFLambdas');
  }

  getConeLambdas(constraintId) {
    return got(native.getConeLambdas(this._handle(), constraintId >>> 0), 'getConeLambdas');
  }

  getPointLambdas(constraintId) {
    return got(native.getPointLambdas(this._handle(), constraintId >>> 0), 'getPointLambdas');
  }

  getFixedLambdas(constraintId) {
    return got(native.getFixedLambdas(this._handle(), constraintId >>> 0), 'getFixedLambdas');
  }

  getDistanceLambda(constraintId) {
    return got(native.getDistanceLambda(this._handle(), constraintId >>> 0), 'getDistanceLambda');
  }

  getPulleyLambda(constraintId) {
    return got(native.getPulleyLambda(this._handle(), constraintId >>> 0), 'getPulleyLambda');
  }

  getGearLambda(constraintId) {
    return got(native.getGearLambda(this._handle(), constraintId >>> 0), 'getGearLambda');
  }

  getPathLambdas(constraintId) {
    return got(native.getPathLambdas(this._handle(), constraintId >>> 0), 'getPathLambdas');
  }

  // ── Distance constraint limits & spring ──────────────────────────────────

  setDistanceLimits(constraintId, min, max) {
    ok(native.setDistanceLimits(this._handle(), constraintId >>> 0, num(min, 'min'), num(max, 'max')), 'setDistanceLimits');
  }

  getDistanceLimits(constraintId) {
    return got(native.getDistanceLimits(this._handle(), constraintId >>> 0), 'getDistanceLimits');
  }

  setDistanceLimitsSpring(constraintId, opts = {}) {
    ok(native.setDistanceLimitsSpring(this._handle(), constraintId >>> 0, opts), 'setDistanceLimitsSpring');
  }

  getDistanceLimitsSpring(constraintId) {
    return got(native.getDistanceLimitsSpring(this._handle(), constraintId >>> 0), 'getDistanceLimitsSpring');
  }

  // ── RackAndPinion constraint ──────────────────────────────────────────────

  createRackAndPinionConstraint(bodyA, bodyB, opts = {}) {
    const id = native.createRackAndPinionConstraint(this._handle(), bodyA >>> 0, bodyB >>> 0, opts);
    if (!id) throw new Error('createRackAndPinionConstraint: failed (bodies not found)');
    return id;
  }

  getRackAndPinionLambda(constraintId) {
    return got(native.getRackAndPinionLambda(this._handle(), constraintId >>> 0), 'getRackAndPinionLambda');
  }

  // ── HeightField shape ─────────────────────────────────────────────────────

  createHeightField(opts) {
    return native.createHeightField(this._handle(), opts);
  }

  // ── Compound shapes ───────────────────────────────────────────────────────

  createStaticCompound(opts) {
    return native.createStaticCompound(this._handle(), opts);
  }

  createMutableCompound(opts) {
    return native.createMutableCompound(this._handle(), opts);
  }

  addMutableSubShape(bodyId, shapeSpec) {
    const idx = native.addMutableSubShape(this._handle(), bodyId >>> 0, shapeSpec);
    if (idx < 0) throw new Error('addMutableSubShape: body not found or invalid spec');
    return idx;
  }

  removeMutableSubShape(bodyId, index) {
    ok(native.removeMutableSubShape(this._handle(), bodyId >>> 0, index >>> 0), 'removeMutableSubShape');
  }

  modifyMutableSubShape(bodyId, index, position, rotation = { x: 0, y: 0, z: 0, w: 1 }) {
    const p = pos(position, 'position');
    const r = quat(rotation, 'rotation');
    ok(native.modifyMutableSubShape(this._handle(), bodyId >>> 0, index >>> 0, p, r), 'modifyMutableSubShape');
  }

  adjustMutableCenterOfMass(bodyId) {
    ok(native.adjustMutableCenterOfMass(this._handle(), bodyId >>> 0), 'adjustMutableCenterOfMass');
  }

  // ── Serialization ─────────────────────────────────────────────────────────

  // Returns a Buffer with compact state of all bodies (56 bytes each).
  // Suitable for network sync / state rewind.
  snapshotState() {
    return native.snapshotState(this._handle());
  }

  // Applies a snapshot Buffer to existing bodies (by bodyId).
  applySnapshot(buf) {
    return native.applySnapshot(this._handle(), buf);
  }

  // Full scene save: body shapes + current pos/rot/vel. Returns Buffer.
  // Note: constraints are NOT included — recreate them after loadScene().
  saveScene() {
    return native.saveScene(this._handle());
  }

  // Loads bodies from a saveScene() Buffer into this world.
  // Returns the number of bodies created.
  loadScene(buf) {
    return native.loadScene(this._handle(), buf);
  }

  // ─────────────────────────────────────────────────────────────────────────

  createSkeleton() {
    return new Skeleton(this);
  }
}

const { PhysicsWorker } = require('./src/PhysicsWorker');

module.exports = {
  World,
  Skeleton,
  RagdollSettings,
  Ragdoll,
  PhysicsWorker,
};
