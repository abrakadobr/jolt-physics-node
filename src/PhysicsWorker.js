'use strict';
/**
 * PhysicsWorker — runs a Jolt World in a dedicated Worker Thread.
 *
 * All World methods are available as async equivalents.
 * State snapshots are delivered via onState() after each step().
 *
 * Example:
 *   const worker = await PhysicsWorker.create({ gravity: 9.81 });
 *   const ballId = await worker.createSphere({ radius: 0.5, position: { x:0, y:5, z:0 }, dynamic: true });
 *   worker.onState((snapshot) => { ... }); // Buffer, 56 bytes/body
 *   await worker.step(1/60);
 *   await worker.terminate();
 */

const { Worker } = require('worker_threads');
const path = require('path');

class PhysicsWorker {
  /** @private */
  constructor(worker) {
    this._worker = worker;
    this._pending = new Map();   // id → { resolve, reject }
    this._nextId = 1;
    this._stateCallbacks = [];
    this._eventCallbacks = [];

    worker.on('message', (msg) => {
      if (msg.type === 'state') {
        const snap = Buffer.from(msg.snapshot);
        for (const cb of this._stateCallbacks) cb(snap);
        return;
      }
      if (msg.type === 'event') {
        for (const cb of this._eventCallbacks) cb(msg.kind, msg.data);
        return;
      }
      const prom = this._pending.get(msg.id);
      if (!prom) return;
      this._pending.delete(msg.id);
      if (msg.error != null) prom.reject(new Error(msg.error));
      else prom.resolve(msg.result);
    });

    worker.on('error', (err) => {
      for (const [, p] of this._pending) p.reject(err);
      this._pending.clear();
    });
  }

  /**
   * Create and initialize a PhysicsWorker.
   * @param {object} worldOpts  - options for new World({ gravity, ... })
   * @param {object} [workerOpts]
   * @param {boolean} [workerOpts.autoState=true]  emit state snapshot after each step
   */
  static async create(worldOpts = {}, workerOpts = {}) {
    const workerScript = path.join(__dirname, 'physics-worker.js');
    const indexPath = path.join(__dirname, '..', 'index.js');

    const worker = new Worker(workerScript, {
      workerData: {
        indexPath,
        autoState: workerOpts.autoState !== false,
      },
    });

    const pw = new PhysicsWorker(worker);
    await pw._send('init', [worldOpts, workerOpts.autoState !== false]);
    return pw;
  }

  /** @private */
  _send(cmd, args = []) {
    return new Promise((resolve, reject) => {
      const id = this._nextId++;
      this._pending.set(id, { resolve, reject });
      // Transferable: if any arg is a Buffer / TypedArray, transfer its buffer
      const transferList = [];
      for (const a of args) {
        if (Buffer.isBuffer(a) || ArrayBuffer.isView(a)) {
          transferList.push(a.buffer);
        }
      }
      this._worker.postMessage({ id, cmd, args }, transferList);
    });
  }

  /**
   * Register a callback for state snapshots (called after each step if autoState=true).
   * @param {(snapshot: Buffer) => void} cb
   */
  onState(cb) {
    this._stateCallbacks.push(cb);
    return this;
  }

  /**
   * Register a callback for physics events (bodyActivation, contact).
   * @param {(kind: string, data: object) => void} cb
   */
  onEvent(cb) {
    this._eventCallbacks.push(cb);
    return this;
  }

  /** Advance simulation by dt seconds. */
  step(dt) { return this._send('step', [dt]); }

  /** Get a manual state snapshot (Buffer). */
  snapshotState() { return this._send('snapshotState'); }

  /** Apply a snapshot Buffer to the worker world. */
  applySnapshot(buf) { return this._send('applySnapshot', [buf]); }

  /** Save full scene (shapes + state). Returns Buffer. */
  saveScene() { return this._send('saveScene'); }

  /** Load bodies from a saveScene() Buffer. Returns body count. */
  loadScene(buf) { return this._send('loadScene', [buf]); }

  // ── Body creation ─────────────────────────────────────────────────────────
  createSphere(opts) { return this._send('createSphere', [opts]); }
  createBox(opts) { return this._send('createBox', [opts]); }
  createCapsule(opts) { return this._send('createCapsule', [opts]); }
  createCylinder(opts) { return this._send('createCylinder', [opts]); }
  createTaperedCapsule(opts) { return this._send('createTaperedCapsule', [opts]); }
  createTaperedCylinder(opts) { return this._send('createTaperedCylinder', [opts]); }
  createConvexHull(opts) { return this._send('createConvexHull', [opts]); }
  createMesh(opts) { return this._send('createMesh', [opts]); }
  createHeightField(opts) { return this._send('createHeightField', [opts]); }
  createStaticCompound(opts) { return this._send('createStaticCompound', [opts]); }
  createMutableCompound(opts) { return this._send('createMutableCompound', [opts]); }
  removeBody(id) { return this._send('removeBody', [id]); }

  // ── Body state ────────────────────────────────────────────────────────────
  getBodyPosition(id) { return this._send('getBodyPosition', [id]); }
  getBodyRotation(id) { return this._send('getBodyRotation', [id]); }
  setBodyPosition(id, pos, activate) { return this._send('setBodyPosition', [id, pos, activate]); }
  setBodyRotation(id, rot, activate) { return this._send('setBodyRotation', [id, rot, activate]); }
  getLinearVelocity(id) { return this._send('getLinearVelocity', [id]); }
  setLinearVelocity(id, v) { return this._send('setLinearVelocity', [id, v]); }
  getAngularVelocity(id) { return this._send('getAngularVelocity', [id]); }
  setAngularVelocity(id, v) { return this._send('setAngularVelocity', [id, v]); }
  applyImpulse(id, impulse) { return this._send('applyImpulse', [id, impulse]); }
  addForce(id, force) { return this._send('addForce', [id, force]); }
  addTorque(id, torque) { return this._send('addTorque', [id, torque]); }

  // ── Body properties ───────────────────────────────────────────────────────
  hasBody(id) { return this._send('hasBody', [id]); }
  isBodyActive(id) { return this._send('isBodyActive', [id]); }
  setFriction(id, v) { return this._send('setFriction', [id, v]); }
  getFriction(id) { return this._send('getFriction', [id]); }
  setRestitution(id, v) { return this._send('setRestitution', [id, v]); }
  getRestitution(id) { return this._send('getRestitution', [id]); }
  setGravityFactor(id, v) { return this._send('setGravityFactor', [id, v]); }
  getGravityFactor(id) { return this._send('getGravityFactor', [id]); }
  setMotionType(id, t, activate) { return this._send('setMotionType', [id, t, activate]); }
  getMotionType(id) { return this._send('getMotionType', [id]); }
  setDamping(id, opts) { return this._send('setDamping', [id, opts]); }
  getDamping(id) { return this._send('getDamping', [id]); }
  activateBody(id) { return this._send('activateBody', [id]); }
  deactivateBody(id) { return this._send('deactivateBody', [id]); }
  setBodySensor(id, v) { return this._send('setBodySensor', [id, v]); }
  isBodySensor(id) { return this._send('isBodySensor', [id]); }
  getCenterOfMassPosition(id) { return this._send('getCenterOfMassPosition', [id]); }

  // ── Queries ───────────────────────────────────────────────────────────────
  rayCastClosest(opts) { return this._send('rayCastClosest', [opts]); }
  rayCastAll(opts) { return this._send('rayCastAll', [opts]); }
  collideSphereAll(opts) { return this._send('collideSphereAll', [opts]); }
  castSphereAll(opts) { return this._send('castSphereAll', [opts]); }
  queryAABB(opts) { return this._send('queryAABB', [opts]); }

  // ── Constraints ───────────────────────────────────────────────────────────
  createFixedConstraint(a, b) { return this._send('createFixedConstraint', [a, b]); }
  createHingeConstraint(a, b, pivot, axis, normal) { return this._send('createHingeConstraint', [a, b, pivot, axis, normal]); }
  createSliderConstraint(a, b, pivot, sliderAxis, normal, min, max) { return this._send('createSliderConstraint', [a, b, pivot, sliderAxis, normal, min, max]); }
  createPointConstraint(a, b, pivot) { return this._send('createPointConstraint', [a, b, pivot]); }
  createDistanceConstraint(a, b, opts) { return this._send('createDistanceConstraint', [a, b, opts]); }
  createConeConstraint(a, b, pivot, axis, halfAngle) { return this._send('createConeConstraint', [a, b, pivot, axis, halfAngle]); }
  createSwingTwistConstraint(a, b, pivot, planeAxis, twistAxis, limits) { return this._send('createSwingTwistConstraint', [a, b, pivot, planeAxis, twistAxis, limits]); }
  createSixDOFConstraint(a, b, pivot, axisX, axisY) { return this._send('createSixDOFConstraint', [a, b, pivot, axisX, axisY]); }
  removeConstraint(id) { return this._send('removeConstraint', [id]); }
  setHingeLimits(id, min, max) { return this._send('setHingeLimits', [id, min, max]); }
  setHingeMotor(id, opts) { return this._send('setHingeMotor', [id, opts]); }
  getHingeAngle(id) { return this._send('getHingeAngle', [id]); }
  setSliderLimits(id, min, max) { return this._send('setSliderLimits', [id, min, max]); }
  setSliderMotor(id, opts) { return this._send('setSliderMotor', [id, opts]); }
  getSliderPosition(id) { return this._send('getSliderPosition', [id]); }
  setSwingTwistLimits(id, opts) { return this._send('setSwingTwistLimits', [id, opts]); }
  setSwingTwistMotor(id, opts) { return this._send('setSwingTwistMotor', [id, opts]); }
  setSixDOFLimits(id, opts) { return this._send('setSixDOFLimits', [id, opts]); }
  setSixDOFMotorState(id, axis, state) { return this._send('setSixDOFMotorState', [id, axis, state]); }

  // ── Gravity ───────────────────────────────────────────────────────────────
  setGravity(g) { return this._send('setGravity', [g]); }

  /**
   * Terminate the worker. After this, no further calls are possible.
   * Sends 'destroy' to the worker world with a 2-second timeout, then
   * forcibly terminates the thread and rejects any still-pending promises.
   */
  async terminate() {
    const DESTROY_TIMEOUT_MS = 2000;
    try {
      await Promise.race([
        this._send('destroy', []),
        new Promise((_, reject) =>
          setTimeout(() => reject(new Error('physics worker destroy timeout')), DESTROY_TIMEOUT_MS)
        ),
      ]);
    } catch (_) {
      // ignore — timeout or error during destroy
    }
    // Reject any commands that are still in-flight so callers don't hang.
    for (const [, p] of this._pending) p.reject(new Error('worker terminated'));
    this._pending.clear();
    await this._worker.terminate();
    this._stateCallbacks = [];
    this._eventCallbacks = [];
  }
}

module.exports = { PhysicsWorker };
