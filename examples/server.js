'use strict';

const http = require('http');
const WebSocket = require('ws');
const fs = require('fs');
const path = require('path');
const { World, PhysicsWorker } = require('../index');

const HOST = process.env.HOST || '127.0.0.1';
const PORT = Number(process.env.PORT || 8787);
const PUBLIC_DIR = path.join(__dirname, 'public');

const CONTENT_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.glb': 'model/gltf-binary'
};

function json(res, statusCode, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(statusCode, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
    'Content-Length': Buffer.byteLength(body)
  });
  res.end(body);
}

function resolveStaticPath(urlPath) {
  const cleaned = urlPath === '/' ? '/index.html' : urlPath;
  const full = path.normalize(path.join(PUBLIC_DIR, cleaned));
  if (!full.startsWith(PUBLIC_DIR)) return null;
  return full;
}

// Quaternion from axis-angle (axis must be normalized)
function axisAngleQuat(ax, ay, az, angle) {
  const half = angle / 2;
  const s = Math.sin(half);
  return { x: ax * s, y: ay * s, z: az * s, w: Math.cos(half) };
}

// Quaternion multiplication: a * b
function mulQuat(a, b) {
  return {
    x:  a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
    y:  a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
    z:  a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
    w:  a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
  };
}

// ─────────────────────────────────────────────────────────────────────────────
// DemoEngine
// ─────────────────────────────────────────────────────────────────────────────
class DemoEngine {
  constructor() {
    this.world = null;
    this.running = true;
    this.exampleId = 'falling-sphere';
    this.entities = new Map();
    this.constraints = new Map();
    this.ragdolls = new Map();
    this.lastRay = null;
    this.lastSphereCast = null;
    this.lastSphereCollide = null;
    this.events = [];
    this.motorHandles = null;
    this.motorPreset = null;
    this.filterTargets = null;
    this.filterState = null;
    this.springHandles = null;
    this.springParams = null;
    this.tick = 0;
    this.workerInst = null;
    this.latestWorkerSnapshot = null;
    this.savedSnapshot = null;
    this.snapshotSavedAt = null;

    // skeleton-pose example state
    this.poseRagdoll = null;
    this.pose = null;
    this.poseAnimated = true;
    this.poseInitTrans = null;
    this.posePhase = 0;

    // ragdoll-extended example state
    this.extendedRagdoll = null;
    this.ragdollInPhysics = true;

    // mixamo-erika example state
    this.erikaRagdoll = null;
    this._erikaBoneNames = null;
    this.erikaMode = 'ragdoll';     // 'animated' | 'ragdoll'
    this.erikaPose = null;
    this.erikaPosePhase = 0;
    this.erikaPoseBoneIdx = null;   // Map<name, index>
    this.erikaPoseInitTrans = null; // Array of {translation, rotation} at rest

    // callback invoked after each physics tick (used for WS broadcast)
    this.onTick = null;

    this.reset(this.exampleId);

    this.timer = setInterval(() => this._timerTick(), 1000 / 60);
  }

  _timerTick() {
    if (!this.running) return;
    if (this.workerInst) {
      this.workerInst.step(1 / 60).then(() => {
        this.tick++;
        if (this.tick % 2 === 0) this.onTick?.();
      }).catch(() => {});
    } else if (this.world) {
      this._preTick();
      this.world.step(1 / 60);
      this.tick++;
      if (this.tick % 2 === 0) this.onTick?.();
    }
  }

  // Called just BEFORE world.step() — update kinematic/driven objects here.
  _preTick() {
    // ── Erika idle animation ──────────────────────────────────────────────────
    if (this.exampleId === 'mixamo-erika' &&
        this.erikaMode === 'animated' &&
        this.erikaRagdoll && this.erikaPose &&
        this.erikaPoseBoneIdx && this.erikaPoseInitTrans) {
      this.erikaPosePhase += 1 / 60;
      const t = this.erikaPosePhase;
      const idx = this.erikaPoseBoneIdx;
      const init = this.erikaPoseInitTrans;

      // 1. Reset ALL joints to the captured T-pose (init) every frame.
      //    Without this, undriven joints accumulate drift from physics.
      for (let i = 0; i < init.length; i++) {
        this.erikaPose.setJoint(i, init[i].translation, init[i].rotation);
      }

      // 2. Apply small idle-animation overrides on top of T-pose.
      //    mulQuat(extraRot, initRot) = apply extraRot in the parent's local frame.
      const setJ = (name, rot) => {
        const i = idx.get(name);
        if (i === undefined) return;
        this.erikaPose.setJoint(i, init[i].translation, mulQuat(rot, init[i].rotation));
      };

      setJ('Hips',  axisAngleQuat(0, 0, 1, Math.sin(t * 1.1) * 0.03));
      setJ('Spine', axisAngleQuat(0, 0, 1, Math.sin(t * 1.1) * 0.04));
      setJ('Spine2',axisAngleQuat(1, 0, 0, Math.sin(t * 1.2) * 0.04));
      setJ('Neck',  axisAngleQuat(0, 1, 0, Math.sin(t * 0.6) * 0.12));
      setJ('Head',  mulQuat(
        axisAngleQuat(0, 1, 0, Math.sin(t * 0.6) * 0.22),
        axisAngleQuat(1, 0, 0, Math.sin(t * 0.9 + 1.0) * 0.10)
      ));
      setJ('LeftArm',      axisAngleQuat(1, 0, 0,  Math.sin(t * 1.1 + Math.PI) * 0.10));
      setJ('RightArm',     axisAngleQuat(1, 0, 0,  Math.sin(t * 1.1) * 0.10));
      setJ('LeftForeArm',  axisAngleQuat(1, 0, 0, -0.10 + Math.sin(t * 1.1 + Math.PI) * 0.04));
      setJ('RightForeArm', axisAngleQuat(1, 0, 0, -0.10 + Math.sin(t * 1.1) * 0.04));

      // 3. Use setPose (instant teleport) instead of driveToPoseKinematics.
      //    driveToPoseKinematics fights gravity and constraint limits producing
      //    jitter/breakage at the T-pose extremes (arms spread wide).
      //    setPose is purely kinematic — physics resumes only in ragdoll mode.
      this.erikaPose.calculateJointMatrices();
      this.erikaRagdoll.setPose(this.erikaPose);
      // Zero out all body velocities so gravity doesn't accumulate across frames
      // (otherwise each physics step adds ~0.16 m/s downward, which after N frames
      //  drags bodies through constraints and breaks the animated pose).
      this.erikaRagdoll.setLinearAndAngularVelocity({ x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 0 });
      // Clear warm-start constraint impulses — after a teleport they are stale and
      // the solver would apply large corrective forces on the first iteration.
      this.erikaRagdoll.resetWarmStart();
    }

    if (this.exampleId === 'skeleton-pose' && this.poseRagdoll && this.pose && this.poseAnimated && this.poseInitTrans) {
      this.posePhase += 1 / 60;
      const t = this.posePhase;

      // Root stays identity
      this.pose.setJoint(0, this.poseInitTrans[0], { x: 0, y: 0, z: 0, w: 1 });
      // Spine sways side-to-side (around Z)
      const spineAngle = Math.sin(t * 1.8) * 0.38;
      this.pose.setJoint(1, this.poseInitTrans[1], axisAngleQuat(0, 0, 1, spineAngle));
      // Head nods forward (around X)
      const headAngle = Math.sin(t * 2.6 + 0.8) * 0.5;
      this.pose.setJoint(2, this.poseInitTrans[2], axisAngleQuat(1, 0, 0, headAngle));

      this.pose.calculateJointMatrices();
      this.poseRagdoll.setPose(this.pose);
    }
  }

  destroy() {
    clearInterval(this.timer);
    this._cleanupWorld();
  }

  _cleanupWorld() {
    if (this.workerInst) {
      const w = this.workerInst;
      this.workerInst = null;
      this.latestWorkerSnapshot = null;
      w.terminate().catch(() => {});
    }

    // skeleton-pose cleanup
    if (this.pose) {
      try { this.pose.destroy(); } catch {}
      this.pose = null;
    }
    this.poseRagdoll = null;
    this.poseAnimated = true;
    this.poseInitTrans = null;
    this.posePhase = 0;

    // ragdoll-extended cleanup
    this.extendedRagdoll = null;
    this.ragdollInPhysics = true;

    // mixamo-erika cleanup
    this.erikaRagdoll = null;
    this._erikaBoneNames = null;
    if (this.erikaPose) { try { this.erikaPose.destroy(); } catch {} this.erikaPose = null; }
    this.erikaMode = 'ragdoll';
    this.erikaPosePhase = 0;
    this.erikaPoseBoneIdx = null;
    this.erikaPoseInitTrans = null;

    if (!this.world && this.entities.size === 0) return;

    for (const ragdoll of this.ragdolls.values()) {
      try { ragdoll.destroy(); } catch {}
    }
    this.ragdolls.clear();
    this.entities.clear();
    this.constraints.clear();
    this.lastRay = null;
    this.lastSphereCast = null;
    this.lastSphereCollide = null;
    this.events = [];
    this.motorHandles = null;
    this.motorPreset = null;
    this.filterTargets = null;
    this.filterState = null;
    this.springHandles = null;
    this.springParams = null;
    this.savedSnapshot = null;
    this.snapshotSavedAt = null;

    if (this.world) {
      try { this.world.destroy(); } catch {}
      this.world = null;
    }
  }

  _trackBody(bodyId, data) {
    const { kind, color, radius, halfExtents, length, dynamic: dyn, ...extra } = data;
    this.entities.set(bodyId, {
      bodyId,
      kind,
      color: color || '#2f6fed',
      radius: radius || null,
      halfExtents: halfExtents || null,
      length: length || null,
      dynamic: Boolean(dyn),
      ...extra
    });
    return bodyId;
  }

  _makeWorld(gravity = 9.81) {
    this.world = new World({ gravity });
    this.entities = new Map();
    this.constraints = new Map();
    this.ragdolls = new Map();
    this.lastRay = null;
    this.lastSphereCast = null;
    this.lastSphereCollide = null;
    this.events = [];
    this.motorHandles = null;
    this.motorPreset = null;

    this.world.onBodyActivation((event) => {
      this._pushEvent({ source: 'activation', ...event });
    });
    this.world.onContact((event) => {
      this._pushEvent({ source: 'contact', ...event });
    });

    this._trackBody(
      this.world.createBox({
        halfExtents: { x: 100, y: 1, z: 100 },
        position: { x: 0, y: -1, z: 0 },
        dynamic: false,
        friction: 0.8,
        restitution: 0
      }),
      { kind: 'box', dynamic: false, halfExtents: { x: 100, y: 1, z: 100 }, color: '#444' }
    );
  }

  _pushEvent(event) {
    this.events.push({ atTick: this.tick, ...event });
    if (this.events.length > 60) this.events.shift();
  }

  reset(exampleId) {
    this._cleanupWorld();
    this.tick = 0;
    this.exampleId = exampleId;

    // ─── Falling Sphere ───────────────────────────────────────────────────────
    if (exampleId === 'falling-sphere') {
      this._makeWorld(9.81);
      this._trackBody(
        this.world.createSphere({ radius: 0.5, position: { x: 0, y: 5, z: 0 }, dynamic: true, restitution: 0.2, friction: 0.5 }),
        { kind: 'sphere', radius: 0.5, dynamic: true, color: '#1f77b4' }
      );
      return;
    }

    // ─── Constraints ──────────────────────────────────────────────────────────
    if (exampleId === 'constraints') {
      this._makeWorld(9.81);
      const a = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.5, y: 0.5, z: 0.5 }, position: { x: -2, y: 4, z: 0 }, dynamic: true, restitution: 0.05, friction: 0.6 }),
        { kind: 'box', halfExtents: { x: 0.5, y: 0.5, z: 0.5 }, dynamic: true, color: '#ef6c00' }
      );
      const b = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.5, y: 0.5, z: 0.5 }, position: { x: -2, y: 6, z: 0 }, dynamic: true, restitution: 0.05, friction: 0.6 }),
        { kind: 'box', halfExtents: { x: 0.5, y: 0.5, z: 0.5 }, dynamic: true, color: '#ef6c00' }
      );
      const c = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.6, y: 0.2, z: 0.6 }, position: { x: 2, y: 4, z: 0 }, dynamic: true, restitution: 0.1, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.6, y: 0.2, z: 0.6 }, dynamic: true, color: '#2e7d32' }
      );
      const d = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.4, y: 0.4, z: 0.4 }, position: { x: 2, y: 5.8, z: 0 }, dynamic: true, restitution: 0.1, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.4, y: 0.4, z: 0.4 }, dynamic: true, color: '#2e7d32' }
      );
      const fixed = this.world.createFixedConstraint(a, b);
      this.constraints.set(fixed, { type: 'fixed', bodyA: a, bodyB: b, color: '#ff9800' });
      const slider = this.world.createSliderConstraint(c, d, { x: 2, y: 5.0, z: 0 }, { x: 0, y: 1, z: 0 }, { x: 1, y: 0, z: 0 }, -1, 1);
      this.constraints.set(slider, { type: 'slider', bodyA: c, bodyB: d, color: '#66bb6a' });
      return;
    }

    // ─── Showcase ─────────────────────────────────────────────────────────────
    if (exampleId === 'showcase') {
      this._makeWorld(9.81);
      this._trackBody(
        this.world.createCapsule({ halfHeight: 0.6, radius: 0.25, position: { x: -2.5, y: 5.5, z: 0 }, dynamic: true, restitution: 0.15, friction: 0.5 }),
        { kind: 'capsule', radius: 0.25, length: 1.2, dynamic: true, color: '#8e24aa' }
      );
      this._trackBody(
        this.world.createConvexHull({ points: [0, 0.6, 0, -0.5, 0, 0, 0.5, 0, 0, 0, -0.6, 0, 0, 0, 0.5], position: { x: 0, y: 6, z: 0 }, dynamic: true, restitution: 0.1, friction: 0.5 }),
        { kind: 'convex', radius: 0.6, dynamic: true, color: '#3949ab' }
      );
      this._trackBody(
        this.world.createMesh({ vertices: [-1.5, 0, 0, 1.5, 0, 0, 1.5, 0, 1.5, -1.5, 0, 1.5], indices: [0, 1, 2, 0, 2, 3], position: { x: 3, y: 0.05, z: 0 }, friction: 0.9 }),
        { kind: 'mesh', halfExtents: { x: 1.5, y: 0.05, z: 1.5 }, dynamic: false, color: '#546e7a' }
      );
      return;
    }

    // ─── Ragdoll ──────────────────────────────────────────────────────────────
    if (exampleId === 'ragdoll') {
      this._makeWorld(9.81);
      const skeleton = this.world.createSkeleton();
      skeleton.addJoint('root', -1);
      skeleton.addJoint('spine', 0);
      skeleton.addJoint('head', 1);
      skeleton.finalize();
      const settings = skeleton.createRagdollSettings({ capsuleHalfHeight: 0.2, capsuleRadius: 0.12, spacing: 0.45 });
      const ragdoll = settings.createRagdoll({ collisionGroup: 12, activate: true });
      this.ragdolls.set(ragdoll.id, ragdoll);
      return;
    }

    // ─── Motors + Queries + Events ────────────────────────────────────────────
    if (exampleId === 'motors-queries-events') {
      this._makeWorld(9.81);
      const hingeBase = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.2, y: 0.2, z: 0.2 }, position: { x: -3, y: 4.3, z: 0 }, dynamic: false, friction: 0.6 }),
        { kind: 'box', halfExtents: { x: 0.2, y: 0.2, z: 0.2 }, dynamic: false, color: '#5d4037' }
      );
      const hingeBody = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.2, y: 0.9, z: 0.2 }, position: { x: -3, y: 3.2, z: 0 }, dynamic: true, restitution: 0.05, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.2, y: 0.9, z: 0.2 }, dynamic: true, color: '#fb8c00' }
      );
      const hinge = this.world.createHingeConstraint(hingeBase, hingeBody, { x: -3, y: 4.0, z: 0 }, { x: 0, y: 0, z: 1 }, { x: 1, y: 0, z: 0 });
      this.world.setHingeLimits(hinge, -0.8, 0.8);
      this.world.setHingeMotor(hinge, { state: 1, targetVelocity: 4.2, targetAngle: 0, maxTorque: 240 });
      this.constraints.set(hinge, { type: 'hinge+motor', bodyA: hingeBase, bodyB: hingeBody, color: '#ff9800' });

      const sliderBase = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.2, y: 0.2, z: 0.2 }, position: { x: 0, y: 2.8, z: 0 }, dynamic: false, friction: 0.6 }),
        { kind: 'box', halfExtents: { x: 0.2, y: 0.2, z: 0.2 }, dynamic: false, color: '#37474f' }
      );
      const sliderBody = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.4, y: 0.4, z: 0.4 }, position: { x: 0, y: 4.0, z: 0 }, dynamic: true, restitution: 0.05, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.4, y: 0.4, z: 0.4 }, dynamic: true, color: '#26a69a' }
      );
      const slider = this.world.createSliderConstraint(sliderBase, sliderBody, { x: 0, y: 3.3, z: 0 }, { x: 0, y: 1, z: 0 }, { x: 1, y: 0, z: 0 }, -0.8, 0.8);
      this.world.setSliderMotor(slider, { state: 1, targetVelocity: 3.0, targetPosition: 0, maxForce: 360 });
      this.constraints.set(slider, { type: 'slider+motor', bodyA: sliderBase, bodyB: sliderBody, color: '#00acc1' });

      const sA = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, position: { x: 2.8, y: 4.0, z: 0 }, dynamic: true }),
        { kind: 'box', halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, dynamic: true, color: '#7e57c2' }
      );
      const sB = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, position: { x: 2.8, y: 4.8, z: 0 }, dynamic: true }),
        { kind: 'box', halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, dynamic: true, color: '#7e57c2' }
      );
      const swing = this.world.createSwingTwistConstraint(sA, sB, { x: 2.8, y: 4.4, z: 0 }, { x: 1, y: 0, z: 0 }, { x: 0, y: 1, z: 0 }, { normalHalfCone: 0.9, planeHalfCone: 0.9, twistMin: -0.5, twistMax: 0.5 });
      this.world.setSwingTwistMotor(swing, { swingState: 1, twistState: 1, targetAngularVelocity: { x: 0, y: 3.0, z: 0 }, targetOrientation: { x: 0, y: 0, z: 0, w: 1 }, maxTorque: 180 });
      this.constraints.set(swing, { type: 'swingTwist+motor', bodyA: sA, bodyB: sB, color: '#ab47bc' });

      const sixA = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.22, y: 0.22, z: 0.22 }, position: { x: 4.8, y: 3.6, z: 0 }, dynamic: true }),
        { kind: 'box', halfExtents: { x: 0.22, y: 0.22, z: 0.22 }, dynamic: true, color: '#ef5350' }
      );
      const sixB = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.22, y: 0.22, z: 0.22 }, position: { x: 4.8, y: 4.4, z: 0 }, dynamic: true }),
        { kind: 'box', halfExtents: { x: 0.22, y: 0.22, z: 0.22 }, dynamic: true, color: '#ef5350' }
      );
      const six = this.world.createSixDOFConstraint(sixA, sixB, { x: 4.8, y: 4.0, z: 0 }, { x: 1, y: 0, z: 0 }, { x: 0, y: 1, z: 0 });
      this.world.setSixDOFLimits(six, { translationMin: { x: -0.25, y: -0.25, z: -0.25 }, translationMax: { x: 0.25, y: 0.25, z: 0.25 }, rotationMin: { x: -0.6, y: -0.6, z: -0.6 }, rotationMax: { x: 0.6, y: 0.6, z: 0.6 } });
      this.world.setSixDOFMotorState(six, 0, 1);
      this.world.setSixDOFTargetVelocity(six, { x: 2.4, y: 0, z: 0 }, { x: 0, y: 0, z: 0 });
      this.constraints.set(six, { type: 'sixDOF+motor', bodyA: sixA, bodyB: sixB, color: '#ef5350' });

      this.motorHandles = { hinge, slider, swing, six, bodies: [hingeBody, sliderBody, sA, sB, sixA, sixB] };
      this._applyMotorPreset('velocity');
      return;
    }

    // ─── Query Filters ────────────────────────────────────────────────────────
    if (exampleId === 'query-filters') {
      this._makeWorld(9.81);
      const dynA = this._trackBody(
        this.world.createSphere({ radius: 0.4, position: { x: -1.5, y: 3.5, z: 0 }, dynamic: true, restitution: 0.25, friction: 0.5 }),
        { kind: 'sphere', radius: 0.4, dynamic: true, color: '#1e88e5' }
      );
      const dynB = this._trackBody(
        this.world.createSphere({ radius: 0.4, position: { x: 0.5, y: 5.5, z: 0 }, dynamic: true, restitution: 0.25, friction: 0.5 }),
        { kind: 'sphere', radius: 0.4, dynamic: true, color: '#43a047' }
      );
      const statBox = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.7, y: 0.2, z: 0.7 }, position: { x: 0, y: 1.5, z: 0 }, dynamic: false, friction: 0.8 }),
        { kind: 'box', halfExtents: { x: 0.7, y: 0.2, z: 0.7 }, dynamic: false, color: '#e53935' }
      );
      this.filterTargets = { dynA, dynB, statBox };
      this.filterState = { layerMask: 0b11, excludeBodyId: null, hitCount: 0 };
      return;
    }

    // ─── Spring Motors ────────────────────────────────────────────────────────
    if (exampleId === 'spring-motors') {
      this._makeWorld(9.81);
      const hingeBase = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, position: { x: -2.5, y: 5, z: 0 }, dynamic: false }),
        { kind: 'box', halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, dynamic: false, color: '#5d4037' }
      );
      const hingeArm = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.15, y: 0.9, z: 0.15 }, position: { x: -2.5, y: 3.5, z: 0 }, dynamic: true, restitution: 0.05, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.15, y: 0.9, z: 0.15 }, dynamic: true, color: '#fb8c00' }
      );
      const hinge = this.world.createHingeConstraint(hingeBase, hingeArm, { x: -2.5, y: 4.8, z: 0 }, { x: 0, y: 0, z: 1 }, { x: 1, y: 0, z: 0 });
      this.world.setHingeLimits(hinge, -1.3, 1.3);
      this.world.setHingeMotorSpring(hinge, { frequency: 4, damping: 0.7, maxTorque: 400 });
      this.world.setHingeMotor(hinge, { state: 2, targetVelocity: 0, targetAngle: 0.9, maxTorque: 400 });
      this.constraints.set(hinge, { type: 'hinge+spring', bodyA: hingeBase, bodyB: hingeArm, color: '#ff9800' });

      const sliderBase = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, position: { x: 2.5, y: 2.5, z: 0 }, dynamic: false }),
        { kind: 'box', halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, dynamic: false, color: '#37474f' }
      );
      const sliderBlock = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.35, y: 0.35, z: 0.35 }, position: { x: 2.5, y: 4.2, z: 0 }, dynamic: true, restitution: 0.05, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.35, y: 0.35, z: 0.35 }, dynamic: true, color: '#26a69a' }
      );
      const slider = this.world.createSliderConstraint(sliderBase, sliderBlock, { x: 2.5, y: 3.2, z: 0 }, { x: 0, y: 1, z: 0 }, { x: 1, y: 0, z: 0 }, -1.4, 1.4);
      this.world.setSliderMotorSpring(slider, { frequency: 4, damping: 0.7, maxForce: 500 });
      this.world.setSliderMotor(slider, { state: 2, targetVelocity: 0, targetPosition: 1.0, maxForce: 500 });
      this.constraints.set(slider, { type: 'slider+spring', bodyA: sliderBase, bodyB: sliderBlock, color: '#00acc1' });

      this.springHandles = { hinge, hingeArm, slider, sliderBlock };
      this.springParams = { frequency: 4, damping: 0.7, hingeTarget: 0.9, sliderTarget: 1.0 };
      return;
    }

    // ─── Height Field ─────────────────────────────────────────────────────────
    if (exampleId === 'height-field') {
      this._makeWorld(9.81);
      const N = 16;
      const samples = [];
      for (let r = 0; r < N; r++) {
        for (let c = 0; c < N; c++) {
          const h = 0.7 * Math.sin(r * 0.6) * Math.cos(c * 0.7) + 0.35 * Math.sin(r * 1.2 + 0.5) * Math.sin(c * 0.9 + 1.2) + 0.9;
          samples.push(Math.max(0.05, h));
        }
      }
      const half = (N - 1) / 2;
      const hfId = this.world.createHeightField({ samples, sampleCount: N, offset: { x: -half, y: 0, z: -half }, scale: { x: 1, y: 1, z: 1 }, position: { x: 0, y: 0, z: 0 }, friction: 0.7, restitution: 0.25 });
      this._trackBody(hfId, { kind: 'heightfield', dynamic: false, color: '#5d8a5a', samples, sampleCount: N, cellSize: 1 });
      const drops = [[-4, -4, '#1e88e5'], [0, -5, '#e53935'], [4, -3, '#fb8c00'], [-5, 1, '#43a047'], [2, 3, '#8e24aa'], [-2, 5, '#00acc1'], [5, 4, '#f06292'], [-1, 0, '#ff7043']];
      for (let i = 0; i < drops.length; i++) {
        const [sx, sz, color] = drops[i];
        const radius = 0.22 + (i * 3) % 8 / 30;
        const id = this.world.createSphere({ radius, position: { x: sx, y: 7 + i * 0.5, z: sz }, dynamic: true, restitution: 0.35, friction: 0.6 });
        this._trackBody(id, { kind: 'sphere', radius, dynamic: true, color });
      }
      return;
    }

    // ─── Compound Shapes ──────────────────────────────────────────────────────
    if (exampleId === 'compound-shapes') {
      this._makeWorld(9.81);
      const lcId = this.world.createStaticCompound({
        shapes: [
          { kind: 'box', halfExtents: { x: 2.0, y: 0.15, z: 1.0 }, position: { x: 0, y: 0, z: 0 } },
          { kind: 'box', halfExtents: { x: 0.15, y: 1.0, z: 1.0 }, position: { x: -1.85, y: 0.85, z: 0 } }
        ],
        position: { x: -2, y: 2, z: 0 }, dynamic: false
      });
      this._trackBody(lcId, { dynamic: false, color: '#6d4c41', subShapes: [
        { kind: 'box', halfExtents: { x: 2.0, y: 0.15, z: 1.0 }, position: { x: 0, y: 0, z: 0 }, color: '#6d4c41' },
        { kind: 'box', halfExtents: { x: 0.15, y: 1.0, z: 1.0 }, position: { x: -1.85, y: 0.85, z: 0 }, color: '#8d6e63' }
      ] });
      const tcId = this.world.createStaticCompound({
        shapes: [
          { kind: 'box', halfExtents: { x: 1.5, y: 0.15, z: 0.8 }, position: { x: 0, y: 0, z: 0 } },
          { kind: 'box', halfExtents: { x: 0.15, y: 0.9, z: 0.8 }, position: { x: 1.35, y: 0.75, z: 0 } },
          { kind: 'box', halfExtents: { x: 0.15, y: 0.9, z: 0.8 }, position: { x: -1.35, y: 0.75, z: 0 } }
        ],
        position: { x: 3, y: 2, z: 0 }, dynamic: false
      });
      this._trackBody(tcId, { dynamic: false, color: '#37474f', subShapes: [
        { kind: 'box', halfExtents: { x: 1.5, y: 0.15, z: 0.8 }, position: { x: 0, y: 0, z: 0 }, color: '#546e7a' },
        { kind: 'box', halfExtents: { x: 0.15, y: 0.9, z: 0.8 }, position: { x: 1.35, y: 0.75, z: 0 }, color: '#78909c' },
        { kind: 'box', halfExtents: { x: 0.15, y: 0.9, z: 0.8 }, position: { x: -1.35, y: 0.75, z: 0 }, color: '#78909c' }
      ] });
      const mcId = this.world.createMutableCompound({
        shapes: [{ kind: 'box', halfExtents: { x: 0.45, y: 0.15, z: 0.45 }, position: { x: 0, y: 0, z: 0 } }],
        position: { x: 0.5, y: 6.5, z: 0 }, dynamic: true, restitution: 0.2, friction: 0.6
      });
      this.world.addMutableSubShape(mcId, { kind: 'sphere', radius: 0.28, position: { x: 0, y: 0.43, z: 0 } });
      this.world.addMutableSubShape(mcId, { kind: 'box', halfExtents: { x: 0.45, y: 0.12, z: 0.12 }, position: { x: 0.45, y: 0, z: 0 } });
      this._trackBody(mcId, { dynamic: true, color: '#e91e63', subShapes: [
        { kind: 'box', halfExtents: { x: 0.45, y: 0.15, z: 0.45 }, position: { x: 0, y: 0, z: 0 }, color: '#e91e63' },
        { kind: 'sphere', radius: 0.28, position: { x: 0, y: 0.43, z: 0 }, color: '#f06292' },
        { kind: 'box', halfExtents: { x: 0.45, y: 0.12, z: 0.12 }, position: { x: 0.45, y: 0, z: 0 }, color: '#e91e63' }
      ] });
      const ballCfgs = [
        { x: -1.5, y: 6.5, z: 0.3, r: 0.28, color: '#1e88e5' }, { x: -2.5, y: 7.5, z: -0.2, r: 0.22, color: '#43a047' },
        { x: 3.5, y: 7.0, z: 0.2, r: 0.25, color: '#fbc02d' }, { x: 2.5, y: 8.0, z: -0.3, r: 0.20, color: '#ab47bc' }
      ];
      for (const cfg of ballCfgs) {
        const id = this.world.createSphere({ radius: cfg.r, position: { x: cfg.x, y: cfg.y, z: cfg.z }, dynamic: true, restitution: 0.3, friction: 0.5 });
        this._trackBody(id, { kind: 'sphere', radius: cfg.r, dynamic: true, color: cfg.color });
      }
      return;
    }

    // ─── Rack & Pinion ────────────────────────────────────────────────────────
    if (exampleId === 'rack-and-pinion') {
      this._makeWorld(9.81);
      const pinionAnchor = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.15, y: 0.15, z: 0.15 }, position: { x: 0, y: 4.5, z: 0 }, dynamic: false }),
        { kind: 'box', halfExtents: { x: 0.15, y: 0.15, z: 0.15 }, dynamic: false, color: '#37474f' }
      );
      const pinion = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.6, y: 0.1, z: 0.6 }, position: { x: 0, y: 4.5, z: 0 }, dynamic: true, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.6, y: 0.1, z: 0.6 }, dynamic: true, color: '#fb8c00' }
      );
      const hingeC = this.world.createHingeConstraint(pinionAnchor, pinion, { x: 0, y: 4.5, z: 0 }, { x: 0, y: 1, z: 0 }, { x: 1, y: 0, z: 0 });
      this.constraints.set(hingeC, { type: 'hinge (pinion)', bodyA: pinionAnchor, bodyB: pinion, color: '#ff9800' });
      const rackAnchor = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.15, y: 0.15, z: 0.15 }, position: { x: 2.5, y: 4.5, z: 0 }, dynamic: false }),
        { kind: 'box', halfExtents: { x: 0.15, y: 0.15, z: 0.15 }, dynamic: false, color: '#37474f' }
      );
      const rack = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.85, y: 0.18, z: 0.25 }, position: { x: 2.5, y: 4.5, z: 0 }, dynamic: true, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.85, y: 0.18, z: 0.25 }, dynamic: true, color: '#42a5f5' }
      );
      const sliderC = this.world.createSliderConstraint(rackAnchor, rack, { x: 2.5, y: 4.5, z: 0 }, { x: 1, y: 0, z: 0 }, { x: 0, y: 1, z: 0 }, -2.0, 2.0);
      this.constraints.set(sliderC, { type: 'slider (rack)', bodyA: rackAnchor, bodyB: rack, color: '#0288d1' });
      const rapId = this.world.createRackAndPinionConstraint(pinion, rack, { hingeAxis: { x: 0, y: 1, z: 0 }, sliderAxis: { x: 1, y: 0, z: 0 }, ratio: 0.55, pinionConstraintId: hingeC, rackConstraintId: sliderC });
      this.constraints.set(rapId, { type: 'rackAndPinion', bodyA: pinion, bodyB: rack, color: '#ffca28' });
      this.world.setAngularVelocity(pinion, { x: 0, y: 4.5, z: 0 });
      const ballA = this._trackBody(
        this.world.createSphere({ radius: 0.3, position: { x: -4, y: 5.5, z: 0 }, dynamic: true, restitution: 0.3, friction: 0.4 }),
        { kind: 'sphere', radius: 0.3, dynamic: true, color: '#e53935' }
      );
      const ballB = this._trackBody(
        this.world.createSphere({ radius: 0.3, position: { x: -4, y: 3.5, z: 0 }, dynamic: true, restitution: 0.3, friction: 0.4 }),
        { kind: 'sphere', radius: 0.3, dynamic: true, color: '#ef9a9a' }
      );
      const distC = this.world.createDistanceConstraint(ballA, ballB, { x: -4, y: 5.5, z: 0 }, { x: -4, y: 3.5, z: 0 }, 0.5, 2.8);
      this.world.setDistanceLimitsSpring(distC, { frequency: 3, damping: 0.4 });
      this.constraints.set(distC, { type: 'distance+spring', bodyA: ballA, bodyB: ballB, color: '#ef5350' });
      this.world.applyImpulse(ballA, { x: 3, y: 0, z: 0 });
      this.motorHandles = { hinge: hingeC, slider: sliderC, swing: null, six: null, bodies: [pinion, rack, ballA, ballB] };
      this._applyMotorPreset('velocity');
      return;
    }

    // ─── Serialization ────────────────────────────────────────────────────────
    if (exampleId === 'serialization') {
      this._makeWorld(9.81);
      const towerColors = ['#e53935', '#fb8c00', '#fdd835', '#43a047', '#1e88e5', '#8e24aa', '#00acc1', '#d81b60'];
      for (let i = 0; i < 8; i++) {
        if (i % 2 === 0) {
          const id = this.world.createBox({ halfExtents: { x: 0.4, y: 0.2, z: 0.4 }, position: { x: 0, y: 0.5 + i * 0.55, z: 0 }, dynamic: true, restitution: 0.15, friction: 0.5 });
          this._trackBody(id, { kind: 'box', halfExtents: { x: 0.4, y: 0.2, z: 0.4 }, dynamic: true, color: towerColors[i] });
        } else {
          const id = this.world.createSphere({ radius: 0.3, position: { x: 0, y: 0.5 + i * 0.55, z: 0 }, dynamic: true, restitution: 0.2, friction: 0.5 });
          this._trackBody(id, { kind: 'sphere', radius: 0.3, dynamic: true, color: towerColors[i] });
        }
      }
      const loose = [{ x: -2.5, y: 3.5, r: 0.35, color: '#00bcd4' }, { x: 2.5, y: 4.5, r: 0.30, color: '#ff5722' }, { x: -1.5, y: 5.5, r: 0.40, color: '#8bc34a' }, { x: 1.5, y: 2.5, r: 0.28, color: '#ff9800' }];
      for (const b of loose) {
        const id = this.world.createSphere({ radius: b.r, position: { x: b.x, y: b.y, z: 0 }, dynamic: true, restitution: 0.4, friction: 0.4 });
        this._trackBody(id, { kind: 'sphere', radius: b.r, dynamic: true, color: b.color });
      }
      this.savedSnapshot = null;
      this.snapshotSavedAt = null;
      return;
    }

    // ─── Worker Physics ───────────────────────────────────────────────────────
    if (exampleId === 'worker-physics') {
      this.entities = new Map();
      this.constraints = new Map();
      this.ragdolls = new Map();
      this.latestWorkerSnapshot = null;
      this._setupWorkerExample();
      return;
    }

    // ─── Skeleton Pose (NEW) ──────────────────────────────────────────────────
    // Demonstrates: createPose, setJoint, calculateJointMatrices, setPose, getPose
    if (exampleId === 'skeleton-pose') {
      this._makeWorld(9.81);

      // Small platform for the ragdoll to stand on
      this._trackBody(
        this.world.createBox({ halfExtents: { x: 2.5, y: 0.15, z: 2.5 }, position: { x: 0, y: 0, z: 0 }, dynamic: false, friction: 0.8 }),
        { kind: 'box', halfExtents: { x: 2.5, y: 0.15, z: 2.5 }, dynamic: false, color: '#546e7a' }
      );

      // 3-joint skeleton: pelvis → spine → head
      const skel = this.world.createSkeleton();
      skel.addJoint('pelvis', -1);
      skel.addJoint('spine', 0);
      skel.addJoint('head', 1);
      skel.finalize();

      const settings = skel.createRagdollSettings({ capsuleHalfHeight: 0.26, capsuleRadius: 0.13, spacing: 0.50 });
      settings.setJointConstraint(1, { type: 'swingTwist', normalHalfCone: 1.3, planeHalfCone: 1.3, twistMin: -0.6, twistMax: 0.6 });
      settings.setJointConstraint(2, { type: 'swingTwist', normalHalfCone: 1.3, planeHalfCone: 1.3, twistMin: -0.6, twistMax: 0.6 });

      const ragdoll = settings.createRagdoll({ collisionGroup: 5, activate: true });
      this.ragdolls.set(ragdoll.id, ragdoll);
      this.poseRagdoll = ragdoll;

      // Create pose and read initial local-space transforms
      this.pose = ragdoll.createPose();
      ragdoll.getPose(this.pose);
      const cnt = this.pose.getJointCount();
      this.poseInitTrans = [];
      for (let i = 0; i < cnt; i++) {
        const j = this.pose.getJoint(i);
        this.poseInitTrans.push({ ...j.translation });
      }

      this.poseAnimated = true;
      this.posePhase = 0;
      return;
    }

    // ─── Ragdoll Extended (NEW) ───────────────────────────────────────────────
    // Demonstrates: isActive, getRootTransform, getWorldSpaceBounds,
    //               addImpulse, setLinearVelocity, activate,
    //               addToPhysicsSystem, removeFromPhysicsSystem, stabilize
    if (exampleId === 'ragdoll-extended') {
      this._makeWorld(9.81);

      // 4-joint skeleton: pelvis → spine → l_arm → r_arm (branching)
      // Note: Jolt skeleton is a tree, so l_arm and r_arm both hang off spine (index 1)
      const skel = this.world.createSkeleton();
      skel.addJoint('pelvis', -1);
      skel.addJoint('spine', 0);
      skel.addJoint('l_arm', 1);
      skel.addJoint('r_arm', 1);
      skel.finalize();

      const settings = skel.createRagdollSettings({ capsuleHalfHeight: 0.22, capsuleRadius: 0.11, spacing: 0.44 });
      settings.setJointConstraint(1, { type: 'swingTwist', normalHalfCone: 0.8, planeHalfCone: 0.8, twistMin: -0.4, twistMax: 0.4 });
      settings.setJointConstraint(2, { type: 'swingTwist', normalHalfCone: 1.1, planeHalfCone: 1.1, twistMin: -0.5, twistMax: 0.5 });
      settings.setJointConstraint(3, { type: 'swingTwist', normalHalfCone: 1.1, planeHalfCone: 1.1, twistMin: -0.5, twistMax: 0.5 });

      const ragdoll = settings.createRagdoll({ collisionGroup: 6, activate: true });
      ragdoll.stabilize();
      this.ragdolls.set(ragdoll.id, ragdoll);
      this.extendedRagdoll = ragdoll;
      this.ragdollInPhysics = true;
      return;
    }

    // ─── Mixamo Erika Ragdoll ──────────────────────────────────────────────────
    // The skinned mesh is loaded on the frontend (erika.glb).
    // The frontend sends bone world transforms (positions+quats from the GLB
    // T-pose bind) via 'setup_erika_ragdoll' command, and the server builds
    // the Jolt ragdoll with those exact bone positions.
    if (exampleId === 'mixamo-erika') {
      this._makeWorld(9.81);
      // Physics floor — not tracked so it won't appear in state.bodies / be rendered.
      // The Three.js GridHelper provides the visual floor reference.
      this.world.createBox({
        halfExtents: { x: 40, y: 0.15, z: 40 },
        position: { x: 0, y: -0.15, z: 0 },
        dynamic: false,
        friction: 0.9
      });
      // Ragdoll will be created when the client sends 'setup_erika_ragdoll'
      return;
    }

    this._makeWorld(9.81);
  }

  async _setupWorkerExample() {
    let pw;
    try {
      pw = await PhysicsWorker.create({ gravity: 9.81 });
    } catch (e) {
      console.error('[worker-physics] PhysicsWorker.create failed:', e);
      return;
    }
    if (this.exampleId !== 'worker-physics') {
      await pw.terminate().catch(() => {});
      return;
    }
    pw.onState((snap) => { this.latestWorkerSnapshot = snap; });
    const groundId = await pw.createBox({ halfExtents: { x: 100, y: 1, z: 100 }, position: { x: 0, y: -1, z: 0 }, dynamic: false, friction: 0.8 });
    this.entities.set(groundId, { bodyId: groundId, kind: 'box', halfExtents: { x: 100, y: 1, z: 100 }, dynamic: false, color: '#444' });
    const colors = ['#e53935', '#fb8c00', '#43a047', '#1e88e5', '#9c27b0', '#00bcd4', '#ff5722', '#8bc34a', '#fdd835', '#e91e63'];
    let idx = 0;
    for (let row = 0; row < 2; row++) {
      for (let col = 0; col < 5; col++) {
        const r = 0.28 + (idx % 3) * 0.06;
        const id = await pw.createSphere({ radius: r, position: { x: (col - 2) * 1.5, y: 4 + row * 1.8, z: (row - 0.5) * 0.6 }, dynamic: true, restitution: 0.55, friction: 0.4 });
        this.entities.set(id, { bodyId: id, kind: 'sphere', radius: r, dynamic: true, color: colors[idx % colors.length] });
        idx++;
      }
    }
    const boxColors = ['#5c6bc0', '#26a69a', '#ef5350'];
    for (let i = 0; i < 3; i++) {
      const he = 0.3 + i * 0.08;
      const id = await pw.createBox({ halfExtents: { x: he, y: he, z: he }, position: { x: (i - 1) * 1.2, y: 7 + i * 0.5, z: 0.5 }, dynamic: true, restitution: 0.2, friction: 0.5 });
      this.entities.set(id, { bodyId: id, kind: 'box', halfExtents: { x: he, y: he, z: he }, dynamic: true, color: boxColors[i] });
    }
    this.workerInst = pw;
  }

  executeCommand(input) {
    const cmd = String(input?.type || '');

    if (cmd === 'pause') { this.running = false; return; }
    if (cmd === 'play') { this.running = true; return; }

    if (cmd === 'step') {
      const count = Math.max(1, Math.min(240, Number(input?.count || 1)));
      for (let i = 0; i < count; i++) {
        this._preTick();
        this.world.step(1 / 60);
        this.tick++;
      }
      return;
    }

    if (cmd === 'set_gravity') {
      const g = Number(input?.gravity);
      if (!Number.isFinite(g)) throw new Error('gravity must be a number');
      this.world.setGravity(g);
      return;
    }

    if (cmd === 'spawn_sphere') {
      const radius = Math.max(0.1, Number(input?.radius || 0.4));
      const x = Number(input?.x || 0), y = Number(input?.y || 5), z = Number(input?.z || 0);
      const id = this.world.createSphere({ radius, position: { x, y, z }, dynamic: true, restitution: 0.2, friction: 0.6 });
      this._trackBody(id, { kind: 'sphere', radius, dynamic: true, color: '#0097a7' });
      return;
    }

    if (cmd === 'apply_impulse') {
      const bodyId = Number(input?.bodyId);
      if (!Number.isFinite(bodyId)) throw new Error('bodyId is required');
      this.world.applyImpulse(bodyId, { x: Number(input?.x || 0), y: Number(input?.y || 0), z: Number(input?.z || 0) });
      return;
    }

    if (cmd === 'ray_cast') {
      const origin = { x: Number(input?.ox || 0), y: Number(input?.oy || 0), z: Number(input?.oz || 0) };
      const direction = { x: Number(input?.dx || 0), y: Number(input?.dy ?? -1), z: Number(input?.dz || 0) };
      const maxDistance = Number(input?.maxDistance || 30);
      const filter = {};
      if (input?.layerMask != null) filter.layerMask = Number(input.layerMask) >>> 0;
      if (input?.excludeBodyId != null) filter.excludeBodyIds = [Number(input.excludeBodyId) >>> 0];
      const hasFilter = Object.keys(filter).length > 0;
      const filterArg = hasFilter ? filter : undefined;
      let hit = null, allHits = null;
      if (input?.allHits) {
        allHits = this.world.rayCastAll({ origin, direction, maxDistance, filter: filterArg });
        hit = allHits[0] ?? null;
      } else {
        hit = this.world.rayCastClosest({ origin, direction, maxDistance, filter: filterArg });
      }
      this.lastRay = { atTick: this.tick, origin, direction, maxDistance, hit, allHits };
      if (this.filterState) {
        this.filterState.layerMask = hasFilter ? (filter.layerMask ?? 0b11) : 0b11;
        this.filterState.excludeBodyId = filter.excludeBodyIds?.[0] ?? null;
        this.filterState.hitCount = allHits ? allHits.length : (hit ? 1 : 0);
      }
      return;
    }

    if (cmd === 'cast_sphere') {
      const origin = { x: Number(input?.ox || 0), y: Number(input?.oy || 0), z: Number(input?.oz || 0) };
      const direction = { x: Number(input?.dx || 0), y: Number(input?.dy ?? -1), z: Number(input?.dz || 0) };
      const maxDistance = Number(input?.maxDistance || 12);
      const radius = Number(input?.radius || 0.3);
      const hits = this.world.castSphereAll({ origin, direction, maxDistance, radius });
      this.lastSphereCast = { atTick: this.tick, origin, direction, maxDistance, radius, hits };
      return;
    }

    if (cmd === 'collide_sphere') {
      const center = { x: Number(input?.cx || 0), y: Number(input?.cy || 0), z: Number(input?.cz || 0) };
      const radius = Number(input?.radius || 0.6);
      const maxSeparation = Number(input?.maxSeparation || 0.1);
      const hits = this.world.collideSphereAll({ center, radius, maxSeparation });
      this.lastSphereCollide = { atTick: this.tick, center, radius, maxSeparation, hits };
      return;
    }

    if (cmd === 'set_spring_params') {
      if (!this.springHandles) throw new Error('Spring params only available in "Spring Motors" example');
      const { hinge, slider, hingeArm, sliderBlock } = this.springHandles;
      const freq = Math.max(0.1, Number(input?.frequency ?? this.springParams.frequency));
      const damp = Math.max(0, Number(input?.damping ?? this.springParams.damping));
      const hingeTarget = Number(input?.hingeTarget ?? this.springParams.hingeTarget);
      const sliderTarget = Number(input?.sliderTarget ?? this.springParams.sliderTarget);
      this.world.setHingeMotorSpring(hinge, { frequency: freq, damping: damp, maxTorque: 400 });
      this.world.setHingeMotor(hinge, { state: 2, targetVelocity: 0, targetAngle: hingeTarget, maxTorque: 400 });
      this.world.setSliderMotorSpring(slider, { frequency: freq, damping: damp, maxForce: 500 });
      this.world.setSliderMotor(slider, { state: 2, targetVelocity: 0, targetPosition: sliderTarget, maxForce: 500 });
      this.springParams = { frequency: freq, damping: damp, hingeTarget, sliderTarget };
      try { this.world.activateBody(hingeArm); } catch {}
      try { this.world.activateBody(sliderBlock); } catch {}
      return;
    }

    if (cmd === 'set_motor_preset') {
      if (!this.motorHandles) throw new Error('Motor presets only available in "Motors + Queries + Events" example');
      const preset = String(input?.preset || '').toLowerCase();
      this._applyMotorPreset(preset);
      return;
    }

    if (cmd === 'save_snapshot') {
      if (!this.world) throw new Error('save_snapshot only works in synchronous-world examples');
      this.savedSnapshot = this.world.snapshotState();
      this.snapshotSavedAt = this.tick;
      return;
    }

    if (cmd === 'restore_snapshot') {
      if (!this.world) throw new Error('restore_snapshot only works in synchronous-world examples');
      if (!this.savedSnapshot) throw new Error('No snapshot saved yet');
      this.world.applySnapshot(this.savedSnapshot);
      return;
    }

    // ─── Skeleton Pose commands ───────────────────────────────────────────────
    if (cmd === 'pose_toggle_animated') {
      if (!this.poseRagdoll) throw new Error('Only available in skeleton-pose example');
      this.poseAnimated = !this.poseAnimated;
      if (!this.poseAnimated) {
        // Re-activate ragdoll so gravity takes over
        try { this.poseRagdoll.activate(); } catch {}
      }
      return;
    }

    if (cmd === 'pose_reset') {
      if (!this.poseRagdoll || !this.pose) throw new Error('Only available in skeleton-pose example');
      this.posePhase = 0;
      // Teleport back to upright rest pose
      if (this.poseInitTrans) {
        this.pose.setJoint(0, this.poseInitTrans[0], { x: 0, y: 0, z: 0, w: 1 });
        this.pose.setJoint(1, this.poseInitTrans[1], { x: 0, y: 0, z: 0, w: 1 });
        this.pose.setJoint(2, this.poseInitTrans[2], { x: 0, y: 0, z: 0, w: 1 });
        this.pose.calculateJointMatrices();
        this.poseRagdoll.setPose(this.pose);
        this.poseRagdoll.resetWarmStart();
      }
      return;
    }

    if (cmd === 'pose_get_info') {
      // Read current pose from ragdoll (for display)
      if (!this.poseRagdoll || !this.pose) throw new Error('Only available in skeleton-pose example');
      this.poseRagdoll.getPose(this.pose);
      return;
    }

    // ─── Ragdoll Extended commands ────────────────────────────────────────────
    if (cmd === 'ragdoll_add_impulse') {
      if (!this.extendedRagdoll) throw new Error('Only available in ragdoll-extended example');
      const x = Number(input?.x || 0), y = Number(input?.y || 0), z = Number(input?.z || 0);
      this.extendedRagdoll.addImpulse({ x, y, z });
      return;
    }

    if (cmd === 'ragdoll_set_velocity') {
      if (!this.extendedRagdoll) throw new Error('Only available in ragdoll-extended example');
      const x = Number(input?.x || 0), y = Number(input?.y || 0), z = Number(input?.z || 0);
      this.extendedRagdoll.setLinearVelocity({ x, y, z });
      return;
    }

    if (cmd === 'ragdoll_activate') {
      if (!this.extendedRagdoll) throw new Error('Only available in ragdoll-extended example');
      this.extendedRagdoll.activate();
      return;
    }

    if (cmd === 'ragdoll_toggle_physics') {
      if (!this.extendedRagdoll) throw new Error('Only available in ragdoll-extended example');
      if (this.ragdollInPhysics) {
        this.extendedRagdoll.removeFromPhysicsSystem();
        this.ragdollInPhysics = false;
      } else {
        this.extendedRagdoll.addToPhysicsSystem(true);
        this.ragdollInPhysics = true;
      }
      return;
    }

    if (cmd === 'ragdoll_reset_warm_start') {
      if (!this.extendedRagdoll) throw new Error('Only available in ragdoll-extended example');
      this.extendedRagdoll.resetWarmStart();
      return;
    }

    // ─── Mixamo Erika commands ────────────────────────────────────────────────
    if (cmd === 'setup_erika_ragdoll') {
      if (this.exampleId !== 'mixamo-erika') throw new Error('Only available in mixamo-erika example');
      if (!input?.bones?.length) throw new Error('Missing bones data');

      // Destroy previous ragdoll if any (re-setup on page refresh)
      if (this.erikaRagdoll) {
        try { this.ragdolls.delete(this.erikaRagdoll.id); this.erikaRagdoll.destroy(); } catch {}
        this.erikaRagdoll = null;
        this._erikaBoneNames = null;
      }

      const bonesData = input.bones;
      const nameToIdx = new Map();
      const skel = this.world.createSkeleton();

      for (let i = 0; i < bonesData.length; i++) {
        const b = bonesData[i];
        const parentIdx = b.parentName != null ? (nameToIdx.get(b.parentName) ?? -1) : -1;
        skel.addJoint(b.name, parentIdx);
        nameToIdx.set(b.name, i);
      }
      skel.finalize();

      const settings = skel.createRagdollSettings({
        capsuleHalfHeight: 0.12,
        capsuleRadius: 0.06,
        spacing: 0.25
      });

      // Per-bone capsule sizes (keyed by Mixamo bone name without mixamorig prefix)
      const BONE_SHAPES = {
        Hips:          { kind: 'capsule', halfHeight: 0.10, radius: 0.11 },
        Spine:         { kind: 'capsule', halfHeight: 0.08, radius: 0.09 },
        Spine1:        { kind: 'capsule', halfHeight: 0.08, radius: 0.09 },
        Spine2:        { kind: 'capsule', halfHeight: 0.09, radius: 0.11 },
        Neck:          { kind: 'capsule', halfHeight: 0.05, radius: 0.04 },
        Head:          { kind: 'capsule', halfHeight: 0.12, radius: 0.11 },
        LeftShoulder:  { kind: 'capsule', halfHeight: 0.05, radius: 0.04 },
        LeftArm:       { kind: 'capsule', halfHeight: 0.14, radius: 0.05 },
        LeftForeArm:   { kind: 'capsule', halfHeight: 0.12, radius: 0.04 },
        RightShoulder: { kind: 'capsule', halfHeight: 0.05, radius: 0.04 },
        RightArm:      { kind: 'capsule', halfHeight: 0.14, radius: 0.05 },
        RightForeArm:  { kind: 'capsule', halfHeight: 0.12, radius: 0.04 },
        LeftUpLeg:     { kind: 'capsule', halfHeight: 0.18, radius: 0.07 },
        LeftLeg:       { kind: 'capsule', halfHeight: 0.17, radius: 0.05 },
        LeftFoot:      { kind: 'capsule', halfHeight: 0.07, radius: 0.04 },
        RightUpLeg:    { kind: 'capsule', halfHeight: 0.18, radius: 0.07 },
        RightLeg:      { kind: 'capsule', halfHeight: 0.17, radius: 0.05 },
        RightFoot:     { kind: 'capsule', halfHeight: 0.07, radius: 0.04 },
      };

      // Per-bone constraints
      const BONE_CONSTRAINTS = {
        Spine:         { type: 'swingTwist', normalHalfCone: 0.5, planeHalfCone: 0.3, twistMin: -0.3, twistMax: 0.3 },
        Spine1:        { type: 'swingTwist', normalHalfCone: 0.4, planeHalfCone: 0.3, twistMin: -0.3, twistMax: 0.3 },
        Spine2:        { type: 'swingTwist', normalHalfCone: 0.4, planeHalfCone: 0.3, twistMin: -0.3, twistMax: 0.3 },
        Neck:          { type: 'swingTwist', normalHalfCone: 0.6, planeHalfCone: 0.5, twistMin: -0.5, twistMax: 0.5 },
        Head:          { type: 'swingTwist', normalHalfCone: 0.5, planeHalfCone: 0.5, twistMin: -0.3, twistMax: 0.3 },
        LeftShoulder:  { type: 'swingTwist', normalHalfCone: 0.4, planeHalfCone: 0.3, twistMin: -0.3, twistMax: 0.3 },
        LeftArm:       { type: 'swingTwist', normalHalfCone: 1.4, planeHalfCone: 1.0, twistMin: -1.0, twistMax: 1.0 },
        LeftForeArm:   { type: 'hinge', minAngle: -2.8, maxAngle: 0.05 },
        RightShoulder: { type: 'swingTwist', normalHalfCone: 0.4, planeHalfCone: 0.3, twistMin: -0.3, twistMax: 0.3 },
        RightArm:      { type: 'swingTwist', normalHalfCone: 1.4, planeHalfCone: 1.0, twistMin: -1.0, twistMax: 1.0 },
        RightForeArm:  { type: 'hinge', minAngle: -2.8, maxAngle: 0.05 },
        LeftUpLeg:     { type: 'swingTwist', normalHalfCone: 1.2, planeHalfCone: 0.8, twistMin: -0.8, twistMax: 0.8 },
        LeftLeg:       { type: 'hinge', minAngle: -2.8, maxAngle: 0.05 },
        LeftFoot:      { type: 'swingTwist', normalHalfCone: 0.5, planeHalfCone: 0.3, twistMin: -0.3, twistMax: 0.3 },
        RightUpLeg:    { type: 'swingTwist', normalHalfCone: 1.2, planeHalfCone: 0.8, twistMin: -0.8, twistMax: 0.8 },
        RightLeg:      { type: 'hinge', minAngle: -2.8, maxAngle: 0.05 },
        RightFoot:     { type: 'swingTwist', normalHalfCone: 0.5, planeHalfCone: 0.3, twistMin: -0.3, twistMax: 0.3 },
      };

      for (let i = 0; i < bonesData.length; i++) {
        const b = bonesData[i];
        settings.setJointShape(i, BONE_SHAPES[b.name] || { kind: 'capsule', halfHeight: 0.08, radius: 0.05 });
        settings.setJointTransform(i, { x: b.x, y: b.y, z: b.z }, { x: b.qx, y: b.qy, z: b.qz, w: b.qw });
        if (i > 0 && BONE_CONSTRAINTS[b.name]) {
          settings.setJointConstraint(i, BONE_CONSTRAINTS[b.name]);
        }
      }

      const ragdoll = settings.createRagdoll({ collisionGroup: 7, activate: true });
      this.ragdolls.set(ragdoll.id, ragdoll);
      this.erikaRagdoll = ragdoll;
      this._erikaBoneNames = bonesData.map(b => b.name);

      // Capture T-pose BEFORE stabilize() — immediately after creation, bodies are
      // at the exact positions from setJointTransform (GLB T-pose).
      // stabilize() runs physics steps that can flip/topple the ragdoll, corrupting the reference pose.
      if (this.erikaPose) { try { this.erikaPose.destroy(); } catch {} }
      this.erikaPose = ragdoll.createPose();
      ragdoll.getPose(this.erikaPose);
      const cnt = this.erikaPose.getJointCount();
      this.erikaPoseBoneIdx = new Map(this._erikaBoneNames.map((name, i) => [name, i]));
      this.erikaPoseInitTrans = [];
      for (let i = 0; i < cnt; i++) this.erikaPoseInitTrans.push(this.erikaPose.getJoint(i));
      this.erikaMode = 'animated';
      // Stabilize after capturing T-pose (for physics/ragdoll mode stability)
      ragdoll.stabilize();
      this.erikaPosePhase = 0;
      return;
    }

    if (cmd === 'erika_toggle_mode') {
      if (!this.erikaRagdoll) throw new Error('Only available in mixamo-erika example');
      if (this.erikaMode === 'animated') {
        this.erikaMode = 'ragdoll';
        try { this.erikaRagdoll.activate(); } catch {}
      } else {
        this.erikaMode = 'animated';
        this.erikaPosePhase = 0;
      }
      return;
    }

    if (cmd === 'erika_push') {
      if (!this.erikaRagdoll) throw new Error('Only available in mixamo-erika example');
      // Any push switches to ragdoll mode so physics reacts
      if (this.erikaMode === 'animated') {
        this.erikaMode = 'ragdoll';
        try { this.erikaRagdoll.activate(); } catch {}
      }
      this.erikaRagdoll.addImpulse({
        x: Number(input?.x || 0),
        y: Number(input?.y || 0),
        z: Number(input?.z || 0)
      });
      return;
    }

    if (cmd === 'erika_impulse') {
      if (!this.erikaRagdoll) throw new Error('Only available in mixamo-erika example');
      this.erikaRagdoll.addImpulse({
        x: Number(input?.x || 0),
        y: Number(input?.y || 0),
        z: Number(input?.z || 0)
      });
      return;
    }

    if (cmd === 'erika_activate') {
      if (!this.erikaRagdoll) throw new Error('Only available in mixamo-erika example');
      this.erikaRagdoll.activate();
      return;
    }

    if (cmd === 'erika_reset') {
      if (this.exampleId !== 'mixamo-erika') throw new Error('Only available in mixamo-erika example');
      this.reset('mixamo-erika');
      return;
    }

    throw new Error(`Unknown command: ${cmd}`);
  }

  _applyMotorPreset(preset) {
    const { hinge, slider, swing, six } = this.motorHandles || {};
    if (!hinge) return;
    this.motorPreset = preset;
    if (preset === 'off') {
      if (hinge)  this.world.setHingeMotor(hinge, { state: 0, targetVelocity: 0, targetAngle: 0, maxTorque: 240 });
      if (slider) this.world.setSliderMotor(slider, { state: 0, targetVelocity: 0, targetPosition: 0, maxForce: 360 });
      if (swing)  this.world.setSwingTwistMotor(swing, { swingState: 0, twistState: 0, targetAngularVelocity: { x: 0, y: 0, z: 0 }, targetOrientation: { x: 0, y: 0, z: 0, w: 1 }, maxTorque: 180 });
      if (six)    { this.world.setSixDOFMotorState(six, 0, 0); this.world.setSixDOFTargetVelocity(six, { x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 0 }); }
      this._wakeMotorBodies();
      return;
    }
    if (preset === 'position') {
      if (hinge)  this.world.setHingeMotor(hinge, { state: 2, targetVelocity: 0, targetAngle: 0.75, maxTorque: 420 });
      if (slider) this.world.setSliderMotor(slider, { state: 2, targetVelocity: 0, targetPosition: 0.6, maxForce: 660 });
      if (swing)  this.world.setSwingTwistMotor(swing, { swingState: 2, twistState: 2, targetAngularVelocity: { x: 0, y: 0, z: 0 }, targetOrientation: { x: 0, y: 0.5, z: 0, w: 0.866025404 }, maxTorque: 420 });
      if (six)    { this.world.setSixDOFMotorState(six, 0, 2); this.world.setSixDOFTargetPose(six, { x: 0.24, y: 0, z: 0 }, { x: 0, y: 0, z: 0, w: 1 }); }
      this._wakeMotorBodies();
      return;
    }
    if (preset === 'velocity') {
      if (hinge)  this.world.setHingeMotor(hinge, { state: 1, targetVelocity: 7.2, targetAngle: 0, maxTorque: 420 });
      if (slider) this.world.setSliderMotor(slider, { state: 1, targetVelocity: 5.4, targetPosition: 0, maxForce: 660 });
      if (swing)  this.world.setSwingTwistMotor(swing, { swingState: 1, twistState: 1, targetAngularVelocity: { x: 0, y: 5.4, z: 0 }, targetOrientation: { x: 0, y: 0, z: 0, w: 1 }, maxTorque: 420 });
      if (six)    { this.world.setSixDOFMotorState(six, 0, 1); this.world.setSixDOFTargetVelocity(six, { x: 5.4, y: 0, z: 0 }, { x: 0, y: 0, z: 0 }); }
      this._wakeMotorBodies();
      return;
    }
    throw new Error('Unknown preset. Use: off | velocity | position');
  }

  _wakeMotorBodies() {
    for (const bodyId of this.motorHandles?.bodies || []) {
      try { this.world.activateBody(bodyId); } catch {}
    }
  }

  _parseWorkerSnapshot() {
    const snap = this.latestWorkerSnapshot;
    if (!snap || snap.byteLength === 0) return [];
    const stride = 56;
    const count = Math.floor(snap.byteLength / stride);
    const bodies = [];
    for (let i = 0; i < count; i++) {
      const off = i * stride;
      const bodyId = snap.readUInt32LE(off);
      const px = snap.readFloatLE(off + 4), py = snap.readFloatLE(off + 8), pz = snap.readFloatLE(off + 12);
      const rx = snap.readFloatLE(off + 16), ry = snap.readFloatLE(off + 20), rz = snap.readFloatLE(off + 24), rw = snap.readFloatLE(off + 28);
      const meta = this.entities.get(bodyId);
      if (meta) {
        bodies.push({ bodyId, ...meta, position: { x: px, y: py, z: pz }, rotation: { x: rx, y: ry, z: rz, w: rw }, velocity: { x: 0, y: 0, z: 0 } });
      }
    }
    return bodies;
  }

  getState() {
    let bodies = [];
    if (this.workerInst || this.exampleId === 'worker-physics') {
      bodies = this._parseWorkerSnapshot();
    } else {
      for (const [bodyId, meta] of this.entities.entries()) {
        try {
          const position = this.world.getBodyPosition(bodyId);
          const rotation = this.world.getBodyRotation(bodyId);
          const velocity = this.world.getLinearVelocity(bodyId);
          bodies.push({ bodyId, ...meta, position, rotation, velocity });
        } catch (e) {
          console.error(`[getState] Body ${bodyId} removed: ${e.message || e}`);
          this.entities.delete(bodyId);
        }
      }
    }

    const ragdolls = [];
    for (const ragdoll of this.ragdolls.values()) {
      try {
        const count = ragdoll.bodyCount();
        const bones = [];
        for (let i = 0; i < count; i++) bones.push(ragdoll.getBoneTransform(i));
        ragdolls.push({ ragdollId: ragdoll.id, bones });
      } catch {
        this.ragdolls.delete(ragdoll.id);
      }
    }

    const result = {
      running: this.running,
      tick: this.tick,
      exampleId: this.exampleId,
      bodies,
      constraints: [...this.constraints.entries()].map(([constraintId, c]) => ({ constraintId, ...c })),
      ragdolls,
      lastRay: this.lastRay,
      lastSphereCast: this.lastSphereCast,
      lastSphereCollide: this.lastSphereCollide,
      events: this.events.slice(-20),
      motorPreset: this.motorPreset,
      filterState: this.filterState,
      springParams: this.springParams,
      workerMode: !!this.workerInst || this.exampleId === 'worker-physics',
      snapshotSavedAt: this.snapshotSavedAt,
      poseAnimated: this.poseAnimated
    };

    // skeleton-pose: expose extended pose state
    if (this.exampleId === 'skeleton-pose' && this.poseRagdoll) {
      result.skeletonPose = { animated: this.poseAnimated };
    }

    // ragdoll-extended: expose live ragdoll metrics
    if (this.exampleId === 'ragdoll-extended' && this.extendedRagdoll) {
      try {
        result.extendedRagdoll = {
          isActive: this.extendedRagdoll.isActive(),
          rootTransform: this.extendedRagdoll.getRootTransform(),
          worldSpaceBounds: this.extendedRagdoll.getWorldSpaceBounds(),
          inPhysics: this.ragdollInPhysics
        };
      } catch { /* ragdoll removed from physics */ }
    }

    // mixamo-erika: expose per-bone world transforms for skinning
    if (this.exampleId === 'mixamo-erika' && this.erikaRagdoll && this._erikaBoneNames) {
      try {
        const count = this.erikaRagdoll.bodyCount();
        const bones = [];
        for (let i = 0; i < count; i++) {
          const t = this.erikaRagdoll.getBoneTransform(i);
          bones.push({ name: this._erikaBoneNames[i], position: t.position, rotation: t.rotation });
        }
        result.erikaRagdoll = { id: this.erikaRagdoll.id, mode: this.erikaMode, bones };
      } catch {}
    }

    return result;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Examples list
// ─────────────────────────────────────────────────────────────────────────────
const EXAMPLES = [
  { id: 'falling-sphere', name: 'Falling Sphere' },
  { id: 'constraints', name: 'Constraints (Fixed + Slider)' },
  { id: 'showcase', name: 'Capsule / ConvexHull / Mesh' },
  { id: 'ragdoll', name: 'Skeleton + Ragdoll' },
  { id: 'motors-queries-events', name: 'Motors + Queries + Events' },
  { id: 'query-filters', name: 'Query Filters (Layer + Exclude)' },
  { id: 'spring-motors', name: 'Spring Motors (Soft Servo)' },
  { id: 'height-field', name: 'Height Field Terrain' },
  { id: 'compound-shapes', name: 'Compound Shapes (Static + Mutable)' },
  { id: 'rack-and-pinion', name: 'Rack & Pinion + Distance Spring' },
  { id: 'serialization', name: 'Serialization (Snapshot / Scene)' },
  { id: 'worker-physics', name: 'Worker Thread Physics' },
  { id: 'skeleton-pose', name: 'SkeletonPose API (NEW)' },
  { id: 'ragdoll-extended', name: 'Ragdoll Extended API (NEW)' },
  { id: 'mixamo-erika', name: 'Mixamo Erika Ragdoll' }
];

// ─────────────────────────────────────────────────────────────────────────────
// Server setup
// ─────────────────────────────────────────────────────────────────────────────
const engine = new DemoEngine();

const httpServer = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);

    // Keep /api/examples for initial HTTP load before WS connects
    if (req.method === 'GET' && url.pathname === '/api/examples') {
      return json(res, 200, { ok: true, examples: EXAMPLES });
    }

    if (req.method === 'GET') {
      const staticPath = resolveStaticPath(url.pathname);
      if (!staticPath || !fs.existsSync(staticPath) || fs.statSync(staticPath).isDirectory()) {
        res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
        res.end('Not found');
        return;
      }
      const ext = path.extname(staticPath).toLowerCase();
      const contentType = CONTENT_TYPES[ext] || 'application/octet-stream';
      const data = fs.readFileSync(staticPath);
      res.writeHead(200, { 'Content-Type': contentType, 'Cache-Control': 'no-store' });
      res.end(data);
      return;
    }

    json(res, 405, { ok: false, error: 'Method not allowed' });
  } catch (error) {
    json(res, 500, { ok: false, error: error.message || 'Internal error' });
  }
});

// ─────────────────────────────────────────────────────────────────────────────
// WebSocket server
// ─────────────────────────────────────────────────────────────────────────────
const wss = new WebSocket.Server({ server: httpServer });

function broadcast(msg) {
  const payload = JSON.stringify(msg);
  for (const ws of wss.clients) {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(payload);
    }
  }
}

// Push state to all clients on every 2nd physics tick (~30 Hz)
engine.onTick = () => broadcast({ type: 'state', data: engine.getState() });

wss.on('connection', (ws) => {
  // Greet new client with examples list + current state
  ws.send(JSON.stringify({ type: 'examples', data: EXAMPLES }));
  ws.send(JSON.stringify({ type: 'state', data: engine.getState() }));

  ws.on('message', (raw) => {
    let msg;
    try {
      msg = JSON.parse(raw);
    } catch {
      ws.send(JSON.stringify({ type: 'error', message: 'Invalid JSON' }));
      return;
    }

    try {
      if (msg.type === 'load_example') {
        const id = String(msg.id || '');
        if (!EXAMPLES.some(e => e.id === id)) {
          ws.send(JSON.stringify({ type: 'error', message: `Unknown example: ${id}` }));
          return;
        }
        engine.reset(id);
        // Broadcast immediately so all clients see the new scene
        broadcast({ type: 'state', data: engine.getState() });

      } else if (msg.type === 'command') {
        engine.executeCommand(msg.payload);
        // Send immediate state update after command
        broadcast({ type: 'state', data: engine.getState() });

      } else {
        ws.send(JSON.stringify({ type: 'error', message: `Unknown message type: ${msg.type}` }));
      }
    } catch (err) {
      ws.send(JSON.stringify({ type: 'error', message: err.message || String(err) }));
    }
  });

  ws.on('error', (err) => {
    if (err.code !== 'ECONNRESET') console.error('[ws] client error:', err.message);
  });
});

httpServer.listen(PORT, HOST, () => {
  console.log(`Examples server: http://${HOST}:${PORT}`);
  console.log(`WebSocket: ws://${HOST}:${PORT}`);
});

process.on('SIGINT', () => {
  engine.destroy();
  wss.close();
  httpServer.close(() => process.exit(0));
});
