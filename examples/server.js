'use strict';

const http = require('http');
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
  '.png': 'image/png'
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

function readJson(req) {
  return new Promise((resolve, reject) => {
    let raw = '';
    req.on('data', chunk => {
      raw += chunk;
      if (raw.length > 1_000_000) {
        reject(new Error('Body too large'));
        req.destroy();
      }
    });
    req.on('end', () => {
      if (!raw.trim()) {
        resolve({});
        return;
      }
      try {
        resolve(JSON.parse(raw));
      } catch {
        reject(new Error('Invalid JSON'));
      }
    });
    req.on('error', reject);
  });
}

function resolveStaticPath(urlPath) {
  const cleaned = urlPath === '/' ? '/index.html' : urlPath;
  const full = path.normalize(path.join(PUBLIC_DIR, cleaned));
  if (!full.startsWith(PUBLIC_DIR)) return null;
  return full;
}

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

    this.reset(this.exampleId);

    this.timer = setInterval(() => {
      if (!this.running) return;
      if (this.workerInst) {
        this.workerInst.step(1 / 60).then(() => { this.tick++; }).catch(() => {});
      } else if (this.world) {
        this.world.step(1 / 60);
        this.tick += 1;
      }
    }, 1000 / 60);
  }

  destroy() {
    clearInterval(this.timer);
    this._cleanupWorld();
  }

  _cleanupWorld() {
    // Terminate any running worker first
    if (this.workerInst) {
      const w = this.workerInst;
      this.workerInst = null;
      this.latestWorkerSnapshot = null;
      w.terminate().catch(() => {});
    }

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
      {
        kind: 'box',
        dynamic: false,
        halfExtents: { x: 100, y: 1, z: 100 },
        color: '#444'
      }
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

    if (exampleId === 'falling-sphere') {
      this._makeWorld(9.81);
      this._trackBody(
        this.world.createSphere({
          radius: 0.5,
          position: { x: 0, y: 5, z: 0 },
          dynamic: true,
          restitution: 0.2,
          friction: 0.5
        }),
        { kind: 'sphere', radius: 0.5, dynamic: true, color: '#1f77b4' }
      );
      return;
    }

    if (exampleId === 'constraints') {
      this._makeWorld(9.81);
      const a = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.5, y: 0.5, z: 0.5 },
          position: { x: -2, y: 4, z: 0 },
          dynamic: true,
          restitution: 0.05,
          friction: 0.6
        }),
        { kind: 'box', halfExtents: { x: 0.5, y: 0.5, z: 0.5 }, dynamic: true, color: '#ef6c00' }
      );

      const b = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.5, y: 0.5, z: 0.5 },
          position: { x: -2, y: 6, z: 0 },
          dynamic: true,
          restitution: 0.05,
          friction: 0.6
        }),
        { kind: 'box', halfExtents: { x: 0.5, y: 0.5, z: 0.5 }, dynamic: true, color: '#ef6c00' }
      );

      const c = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.6, y: 0.2, z: 0.6 },
          position: { x: 2, y: 4, z: 0 },
          dynamic: true,
          restitution: 0.1,
          friction: 0.5
        }),
        { kind: 'box', halfExtents: { x: 0.6, y: 0.2, z: 0.6 }, dynamic: true, color: '#2e7d32' }
      );

      const d = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.4, y: 0.4, z: 0.4 },
          position: { x: 2, y: 5.8, z: 0 },
          dynamic: true,
          restitution: 0.1,
          friction: 0.5
        }),
        { kind: 'box', halfExtents: { x: 0.4, y: 0.4, z: 0.4 }, dynamic: true, color: '#2e7d32' }
      );

      const fixed = this.world.createFixedConstraint(a, b);
      this.constraints.set(fixed, { type: 'fixed', bodyA: a, bodyB: b, color: '#ff9800' });

      const slider = this.world.createSliderConstraint(
        c,
        d,
        { x: 2, y: 5.0, z: 0 },
        { x: 0, y: 1, z: 0 },
        { x: 1, y: 0, z: 0 },
        -1,
        1
      );
      this.constraints.set(slider, { type: 'slider', bodyA: c, bodyB: d, color: '#66bb6a' });
      return;
    }

    if (exampleId === 'showcase') {
      this._makeWorld(9.81);

      this._trackBody(
        this.world.createCapsule({
          halfHeight: 0.6,
          radius: 0.25,
          position: { x: -2.5, y: 5.5, z: 0 },
          dynamic: true,
          restitution: 0.15,
          friction: 0.5
        }),
        { kind: 'capsule', radius: 0.25, length: 1.2, dynamic: true, color: '#8e24aa' }
      );

      this._trackBody(
        this.world.createConvexHull({
          points: [0, 0.6, 0, -0.5, 0, 0, 0.5, 0, 0, 0, -0.6, 0, 0, 0, 0.5],
          position: { x: 0, y: 6, z: 0 },
          dynamic: true,
          restitution: 0.1,
          friction: 0.5
        }),
        { kind: 'convex', radius: 0.6, dynamic: true, color: '#3949ab' }
      );

      this._trackBody(
        this.world.createMesh({
          vertices: [-1.5, 0, 0, 1.5, 0, 0, 1.5, 0, 1.5, -1.5, 0, 1.5],
          indices: [0, 1, 2, 0, 2, 3],
          position: { x: 3, y: 0.05, z: 0 },
          friction: 0.9
        }),
        { kind: 'mesh', halfExtents: { x: 1.5, y: 0.05, z: 1.5 }, dynamic: false, color: '#546e7a' }
      );
      return;
    }

    if (exampleId === 'ragdoll') {
      this._makeWorld(9.81);
      const skeleton = this.world.createSkeleton();
      skeleton.addJoint('root', -1);
      skeleton.addJoint('spine', 0);
      skeleton.addJoint('head', 1);
      skeleton.finalize();

      const settings = skeleton.createRagdollSettings({
        capsuleHalfHeight: 0.2,
        capsuleRadius: 0.12,
        spacing: 0.45
      });
      const ragdoll = settings.createRagdoll({ collisionGroup: 12, activate: true });
      this.ragdolls.set(ragdoll.id, ragdoll);
      return;
    }

    if (exampleId === 'motors-queries-events') {
      this._makeWorld(9.81);

      const hingeBase = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.2, y: 0.2, z: 0.2 },
          position: { x: -3, y: 4.3, z: 0 },
          dynamic: false,
          friction: 0.6
        }),
        { kind: 'box', halfExtents: { x: 0.2, y: 0.2, z: 0.2 }, dynamic: false, color: '#5d4037' }
      );
      const hingeBody = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.2, y: 0.9, z: 0.2 },
          position: { x: -3, y: 3.2, z: 0 },
          dynamic: true,
          restitution: 0.05,
          friction: 0.5
        }),
        { kind: 'box', halfExtents: { x: 0.2, y: 0.9, z: 0.2 }, dynamic: true, color: '#fb8c00' }
      );
      const hinge = this.world.createHingeConstraint(
        hingeBase,
        hingeBody,
        { x: -3, y: 4.0, z: 0 },
        { x: 0, y: 0, z: 1 },
        { x: 1, y: 0, z: 0 }
      );
      this.world.setHingeLimits(hinge, -0.8, 0.8);
      this.world.setHingeMotor(hinge, { state: 1, targetVelocity: 4.2, targetAngle: 0, maxTorque: 240 });
      this.constraints.set(hinge, { type: 'hinge+motor', bodyA: hingeBase, bodyB: hingeBody, color: '#ff9800' });

      const sliderBase = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.2, y: 0.2, z: 0.2 },
          position: { x: 0, y: 2.8, z: 0 },
          dynamic: false,
          friction: 0.6
        }),
        { kind: 'box', halfExtents: { x: 0.2, y: 0.2, z: 0.2 }, dynamic: false, color: '#37474f' }
      );
      const sliderBody = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.4, y: 0.4, z: 0.4 },
          position: { x: 0, y: 4.0, z: 0 },
          dynamic: true,
          restitution: 0.05,
          friction: 0.5
        }),
        { kind: 'box', halfExtents: { x: 0.4, y: 0.4, z: 0.4 }, dynamic: true, color: '#26a69a' }
      );
      const slider = this.world.createSliderConstraint(
        sliderBase,
        sliderBody,
        { x: 0, y: 3.3, z: 0 },
        { x: 0, y: 1, z: 0 },
        { x: 1, y: 0, z: 0 },
        -0.8,
        0.8
      );
      this.world.setSliderMotor(slider, { state: 1, targetVelocity: 3.0, targetPosition: 0, maxForce: 360 });
      this.constraints.set(slider, { type: 'slider+motor', bodyA: sliderBase, bodyB: sliderBody, color: '#00acc1' });

      const sA = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.25, y: 0.25, z: 0.25 },
          position: { x: 2.8, y: 4.0, z: 0 },
          dynamic: true
        }),
        { kind: 'box', halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, dynamic: true, color: '#7e57c2' }
      );
      const sB = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.25, y: 0.25, z: 0.25 },
          position: { x: 2.8, y: 4.8, z: 0 },
          dynamic: true
        }),
        { kind: 'box', halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, dynamic: true, color: '#7e57c2' }
      );
      const swing = this.world.createSwingTwistConstraint(
        sA,
        sB,
        { x: 2.8, y: 4.4, z: 0 },
        { x: 1, y: 0, z: 0 },
        { x: 0, y: 1, z: 0 },
        { normalHalfCone: 0.9, planeHalfCone: 0.9, twistMin: -0.5, twistMax: 0.5 }
      );
      this.world.setSwingTwistMotor(swing, {
        swingState: 1,
        twistState: 1,
        targetAngularVelocity: { x: 0, y: 3.0, z: 0 },
        targetOrientation: { x: 0, y: 0, z: 0, w: 1 },
        maxTorque: 180
      });
      this.constraints.set(swing, { type: 'swingTwist+motor', bodyA: sA, bodyB: sB, color: '#ab47bc' });

      const sixA = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.22, y: 0.22, z: 0.22 },
          position: { x: 4.8, y: 3.6, z: 0 },
          dynamic: true
        }),
        { kind: 'box', halfExtents: { x: 0.22, y: 0.22, z: 0.22 }, dynamic: true, color: '#ef5350' }
      );
      const sixB = this._trackBody(
        this.world.createBox({
          halfExtents: { x: 0.22, y: 0.22, z: 0.22 },
          position: { x: 4.8, y: 4.4, z: 0 },
          dynamic: true
        }),
        { kind: 'box', halfExtents: { x: 0.22, y: 0.22, z: 0.22 }, dynamic: true, color: '#ef5350' }
      );
      const six = this.world.createSixDOFConstraint(
        sixA,
        sixB,
        { x: 4.8, y: 4.0, z: 0 },
        { x: 1, y: 0, z: 0 },
        { x: 0, y: 1, z: 0 }
      );
      this.world.setSixDOFLimits(six, {
        translationMin: { x: -0.25, y: -0.25, z: -0.25 },
        translationMax: { x: 0.25, y: 0.25, z: 0.25 },
        rotationMin: { x: -0.6, y: -0.6, z: -0.6 },
        rotationMax: { x: 0.6, y: 0.6, z: 0.6 }
      });
      this.world.setSixDOFMotorState(six, 0, 1);
      this.world.setSixDOFTargetVelocity(six, { x: 2.4, y: 0, z: 0 }, { x: 0, y: 0, z: 0 });
      this.constraints.set(six, { type: 'sixDOF+motor', bodyA: sixA, bodyB: sixB, color: '#ef5350' });

      this.motorHandles = {
        hinge,
        slider,
        swing,
        six,
        bodies: [hingeBody, sliderBody, sA, sB, sixA, sixB]
      };
      this._applyMotorPreset('velocity');
      return;
    }

    // ─── Query Filters Demo ─────────────────────────────────────────────────
    if (exampleId === 'query-filters') {
      this._makeWorld(9.81);

      // Two dynamic (MOVING) spheres at different heights
      const dynA = this._trackBody(
        this.world.createSphere({ radius: 0.4, position: { x: -1.5, y: 3.5, z: 0 }, dynamic: true, restitution: 0.25, friction: 0.5 }),
        { kind: 'sphere', radius: 0.4, dynamic: true, color: '#1e88e5' }
      );
      const dynB = this._trackBody(
        this.world.createSphere({ radius: 0.4, position: { x: 0.5, y: 5.5, z: 0 }, dynamic: true, restitution: 0.25, friction: 0.5 }),
        { kind: 'sphere', radius: 0.4, dynamic: true, color: '#43a047' }
      );
      // One static (NON_MOVING) platform above ground
      const statBox = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.7, y: 0.2, z: 0.7 }, position: { x: 0, y: 1.5, z: 0 }, dynamic: false, friction: 0.8 }),
        { kind: 'box', halfExtents: { x: 0.7, y: 0.2, z: 0.7 }, dynamic: false, color: '#e53935' }
      );

      this.filterTargets = { dynA, dynB, statBox };
      this.filterState = { layerMask: 0b11, excludeBodyId: null, hitCount: 0 };
      return;
    }

    // ─── Spring Motors Demo ──────────────────────────────────────────────────
    if (exampleId === 'spring-motors') {
      this._makeWorld(9.81);

      // Hinge pendulum with spring position motor
      const hingeBase = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, position: { x: -2.5, y: 5, z: 0 }, dynamic: false }),
        { kind: 'box', halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, dynamic: false, color: '#5d4037' }
      );
      const hingeArm = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.15, y: 0.9, z: 0.15 }, position: { x: -2.5, y: 3.5, z: 0 }, dynamic: true, restitution: 0.05, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.15, y: 0.9, z: 0.15 }, dynamic: true, color: '#fb8c00' }
      );
      const hinge = this.world.createHingeConstraint(
        hingeBase, hingeArm,
        { x: -2.5, y: 4.8, z: 0 }, { x: 0, y: 0, z: 1 }, { x: 1, y: 0, z: 0 }
      );
      this.world.setHingeLimits(hinge, -1.3, 1.3);
      this.world.setHingeMotorSpring(hinge, { frequency: 4, damping: 0.7, maxTorque: 400 });
      this.world.setHingeMotor(hinge, { state: 2, targetVelocity: 0, targetAngle: 0.9, maxTorque: 400 });
      this.constraints.set(hinge, { type: 'hinge+spring', bodyA: hingeBase, bodyB: hingeArm, color: '#ff9800' });

      // Slider with spring position motor
      const sliderBase = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, position: { x: 2.5, y: 2.5, z: 0 }, dynamic: false }),
        { kind: 'box', halfExtents: { x: 0.25, y: 0.25, z: 0.25 }, dynamic: false, color: '#37474f' }
      );
      const sliderBlock = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.35, y: 0.35, z: 0.35 }, position: { x: 2.5, y: 4.2, z: 0 }, dynamic: true, restitution: 0.05, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.35, y: 0.35, z: 0.35 }, dynamic: true, color: '#26a69a' }
      );
      const slider = this.world.createSliderConstraint(
        sliderBase, sliderBlock,
        { x: 2.5, y: 3.2, z: 0 }, { x: 0, y: 1, z: 0 }, { x: 1, y: 0, z: 0 }, -1.4, 1.4
      );
      this.world.setSliderMotorSpring(slider, { frequency: 4, damping: 0.7, maxForce: 500 });
      this.world.setSliderMotor(slider, { state: 2, targetVelocity: 0, targetPosition: 1.0, maxForce: 500 });
      this.constraints.set(slider, { type: 'slider+spring', bodyA: sliderBase, bodyB: sliderBlock, color: '#00acc1' });

      this.springHandles = { hinge, hingeArm, slider, sliderBlock };
      this.springParams = { frequency: 4, damping: 0.7, hingeTarget: 0.9, sliderTarget: 1.0 };
      return;
    }

    // ─── Height Field Terrain ───────────────────────────────────────────────
    if (exampleId === 'height-field') {
      this._makeWorld(9.81);

      // 16×16 grid → 15m × 15m terrain centered at origin
      // NOTE: Jolt HeightFieldShape requires sampleCount divisible by mBlockSize (default=2)
      const N = 16;
      const samples = [];
      for (let r = 0; r < N; r++) {
        for (let c = 0; c < N; c++) {
          const h = 0.7 * Math.sin(r * 0.6) * Math.cos(c * 0.7)
                  + 0.35 * Math.sin(r * 1.2 + 0.5) * Math.sin(c * 0.9 + 1.2)
                  + 0.9;
          samples.push(Math.max(0.05, h));
        }
      }

      const half = (N - 1) / 2;
      const hfId = this.world.createHeightField({
        samples,
        sampleCount: N,   // must be divisible by 2 (Jolt mBlockSize default)
        offset: { x: -half, y: 0, z: -half },
        scale: { x: 1, y: 1, z: 1 },
        position: { x: 0, y: 0, z: 0 },
        friction: 0.7,
        restitution: 0.25
      });
      this._trackBody(hfId, {
        kind: 'heightfield',
        dynamic: false,
        color: '#5d8a5a',
        samples,
        sampleCount: N,
        cellSize: 1
      });

      // Drop coloured spheres at fixed positions
      const drops = [
        [-4, -4, '#1e88e5'], [0, -5, '#e53935'], [4, -3, '#fb8c00'],
        [-5,  1, '#43a047'], [2,   3, '#8e24aa'], [-2, 5, '#00acc1'],
        [ 5,  4, '#f06292'], [-1,  0, '#ff7043']
      ];
      for (let i = 0; i < drops.length; i++) {
        const [sx, sz, color] = drops[i];
        const radius = 0.22 + (i * 3) % 8 / 30;
        const id = this.world.createSphere({
          radius,
          position: { x: sx, y: 7 + i * 0.5, z: sz },
          dynamic: true,
          restitution: 0.35,
          friction: 0.6
        });
        this._trackBody(id, { kind: 'sphere', radius, dynamic: true, color });
      }
      return;
    }

    // ─── Compound Shapes ────────────────────────────────────────────────────
    if (exampleId === 'compound-shapes') {
      this._makeWorld(9.81);

      // Static L-shaped compound: horizontal shelf + vertical wall
      const lcId = this.world.createStaticCompound({
        shapes: [
          { kind: 'box', halfExtents: { x: 2.0,  y: 0.15, z: 1.0 }, position: { x: 0,     y: 0,    z: 0 } },
          { kind: 'box', halfExtents: { x: 0.15, y: 1.0,  z: 1.0 }, position: { x: -1.85, y: 0.85, z: 0 } }
        ],
        position: { x: -2, y: 2, z: 0 },
        dynamic: false
      });
      this._trackBody(lcId, {
        dynamic: false,
        color: '#6d4c41',
        subShapes: [
          { kind: 'box', halfExtents: { x: 2.0,  y: 0.15, z: 1.0 }, position: { x: 0,     y: 0,    z: 0 }, color: '#6d4c41' },
          { kind: 'box', halfExtents: { x: 0.15, y: 1.0,  z: 1.0 }, position: { x: -1.85, y: 0.85, z: 0 }, color: '#8d6e63' }
        ]
      });

      // Static U/T-shaped compound: horizontal beam + two vertical posts
      const tcId = this.world.createStaticCompound({
        shapes: [
          { kind: 'box', halfExtents: { x: 1.5,  y: 0.15, z: 0.8 },  position: { x: 0,     y: 0,    z: 0 } },
          { kind: 'box', halfExtents: { x: 0.15, y: 0.9,  z: 0.8 },  position: { x:  1.35, y: 0.75, z: 0 } },
          { kind: 'box', halfExtents: { x: 0.15, y: 0.9,  z: 0.8 },  position: { x: -1.35, y: 0.75, z: 0 } }
        ],
        position: { x: 3, y: 2, z: 0 },
        dynamic: false
      });
      this._trackBody(tcId, {
        dynamic: false,
        color: '#37474f',
        subShapes: [
          { kind: 'box', halfExtents: { x: 1.5,  y: 0.15, z: 0.8 }, position: { x: 0,     y: 0,    z: 0 }, color: '#546e7a' },
          { kind: 'box', halfExtents: { x: 0.15, y: 0.9,  z: 0.8 }, position: { x:  1.35, y: 0.75, z: 0 }, color: '#78909c' },
          { kind: 'box', halfExtents: { x: 0.15, y: 0.9,  z: 0.8 }, position: { x: -1.35, y: 0.75, z: 0 }, color: '#78909c' }
        ]
      });

      // Dynamic mutable compound: base box → add sphere on top + side arm
      const mcId = this.world.createMutableCompound({
        shapes: [
          { kind: 'box', halfExtents: { x: 0.45, y: 0.15, z: 0.45 }, position: { x: 0, y: 0, z: 0 } }
        ],
        position: { x: 0.5, y: 6.5, z: 0 },
        dynamic: true,
        restitution: 0.2,
        friction: 0.6
      });
      this.world.addMutableSubShape(mcId, { kind: 'sphere', radius: 0.28, position: { x: 0,    y: 0.43, z: 0 } });
      this.world.addMutableSubShape(mcId, { kind: 'box', halfExtents: { x: 0.45, y: 0.12, z: 0.12 }, position: { x: 0.45, y: 0, z: 0 } });

      this._trackBody(mcId, {
        dynamic: true,
        color: '#e91e63',
        subShapes: [
          { kind: 'box',    halfExtents: { x: 0.45, y: 0.15, z: 0.45 }, position: { x: 0,    y: 0,    z: 0 }, color: '#e91e63' },
          { kind: 'sphere', radius: 0.28,                                 position: { x: 0,    y: 0.43, z: 0 }, color: '#f06292' },
          { kind: 'box',    halfExtents: { x: 0.45, y: 0.12, z: 0.12 }, position: { x: 0.45, y: 0,    z: 0 }, color: '#e91e63' }
        ]
      });

      // Extra dynamic spheres to bounce on the platforms
      const ballCfgs = [
        { x: -1.5, y: 6.5, z:  0.3, r: 0.28, color: '#1e88e5' },
        { x: -2.5, y: 7.5, z: -0.2, r: 0.22, color: '#43a047' },
        { x:  3.5, y: 7.0, z:  0.2, r: 0.25, color: '#fbc02d' },
        { x:  2.5, y: 8.0, z: -0.3, r: 0.20, color: '#ab47bc' }
      ];
      for (const cfg of ballCfgs) {
        const id = this.world.createSphere({ radius: cfg.r, position: { x: cfg.x, y: cfg.y, z: cfg.z }, dynamic: true, restitution: 0.3, friction: 0.5 });
        this._trackBody(id, { kind: 'sphere', radius: cfg.r, dynamic: true, color: cfg.color });
      }
      return;
    }

    // ─── Rack & Pinion + Distance Spring ────────────────────────────────────
    if (exampleId === 'rack-and-pinion') {
      this._makeWorld(9.81);

      // Pinion anchor (static) + pinion disc (dynamic, spins around Y)
      const pinionAnchor = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.15, y: 0.15, z: 0.15 }, position: { x: 0, y: 4.5, z: 0 }, dynamic: false }),
        { kind: 'box', halfExtents: { x: 0.15, y: 0.15, z: 0.15 }, dynamic: false, color: '#37474f' }
      );
      const pinion = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.6, y: 0.1, z: 0.6 }, position: { x: 0, y: 4.5, z: 0 }, dynamic: true, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.6, y: 0.1, z: 0.6 }, dynamic: true, color: '#fb8c00' }
      );
      const hingeC = this.world.createHingeConstraint(
        pinionAnchor, pinion,
        { x: 0, y: 4.5, z: 0 },
        { x: 0, y: 1,   z: 0 },  // hinge axis: Y
        { x: 1, y: 0,   z: 0 }
      );
      this.constraints.set(hingeC, { type: 'hinge (pinion)', bodyA: pinionAnchor, bodyB: pinion, color: '#ff9800' });

      // Rack anchor (static) + rack body (dynamic, slides along X)
      const rackAnchor = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.15, y: 0.15, z: 0.15 }, position: { x: 2.5, y: 4.5, z: 0 }, dynamic: false }),
        { kind: 'box', halfExtents: { x: 0.15, y: 0.15, z: 0.15 }, dynamic: false, color: '#37474f' }
      );
      const rack = this._trackBody(
        this.world.createBox({ halfExtents: { x: 0.85, y: 0.18, z: 0.25 }, position: { x: 2.5, y: 4.5, z: 0 }, dynamic: true, friction: 0.5 }),
        { kind: 'box', halfExtents: { x: 0.85, y: 0.18, z: 0.25 }, dynamic: true, color: '#42a5f5' }
      );
      const sliderC = this.world.createSliderConstraint(
        rackAnchor, rack,
        { x: 2.5, y: 4.5, z: 0 },
        { x: 1,   y: 0,   z: 0 },   // slide along X
        { x: 0,   y: 1,   z: 0 },
        -2.0, 2.0
      );
      this.constraints.set(sliderC, { type: 'slider (rack)', bodyA: rackAnchor, bodyB: rack, color: '#0288d1' });

      // RackAndPinion coupling
      const rapId = this.world.createRackAndPinionConstraint(pinion, rack, {
        hingeAxis:           { x: 0, y: 1, z: 0 },
        sliderAxis:          { x: 1, y: 0, z: 0 },
        ratio:               0.55,
        pinionConstraintId:  hingeC,
        rackConstraintId:    sliderC
      });
      this.constraints.set(rapId, { type: 'rackAndPinion', bodyA: pinion, bodyB: rack, color: '#ffca28' });

      // Give pinion an initial spin
      this.world.setAngularVelocity(pinion, { x: 0, y: 4.5, z: 0 });

      // Distance spring: two balls connected by a springy rope
      const ballA = this._trackBody(
        this.world.createSphere({ radius: 0.3, position: { x: -4, y: 5.5, z: 0 }, dynamic: true, restitution: 0.3, friction: 0.4 }),
        { kind: 'sphere', radius: 0.3, dynamic: true, color: '#e53935' }
      );
      const ballB = this._trackBody(
        this.world.createSphere({ radius: 0.3, position: { x: -4, y: 3.5, z: 0 }, dynamic: true, restitution: 0.3, friction: 0.4 }),
        { kind: 'sphere', radius: 0.3, dynamic: true, color: '#ef9a9a' }
      );
      const distC = this.world.createDistanceConstraint(
        ballA, ballB,
        { x: -4, y: 5.5, z: 0 },
        { x: -4, y: 3.5, z: 0 },
        0.5, 2.8
      );
      this.world.setDistanceLimitsSpring(distC, { frequency: 3, damping: 0.4 });
      this.constraints.set(distC, { type: 'distance+spring', bodyA: ballA, bodyB: ballB, color: '#ef5350' });
      this.world.applyImpulse(ballA, { x: 3, y: 0, z: 0 });

      this.motorHandles = {
        hinge: hingeC, slider: sliderC, swing: null, six: null,
        bodies: [pinion, rack, ballA, ballB]
      };
      this._applyMotorPreset('velocity');
      return;
    }

    // ─── Serialization Demo ──────────────────────────────────────────────────
    if (exampleId === 'serialization') {
      this._makeWorld(9.81);

      // Colorful tower of alternating boxes and spheres
      const towerColors = ['#e53935', '#fb8c00', '#fdd835', '#43a047', '#1e88e5', '#8e24aa', '#00acc1', '#d81b60'];
      for (let i = 0; i < 8; i++) {
        if (i % 2 === 0) {
          const id = this.world.createBox({
            halfExtents: { x: 0.4, y: 0.2, z: 0.4 },
            position: { x: 0, y: 0.5 + i * 0.55, z: 0 },
            dynamic: true, restitution: 0.15, friction: 0.5
          });
          this._trackBody(id, { kind: 'box', halfExtents: { x: 0.4, y: 0.2, z: 0.4 }, dynamic: true, color: towerColors[i] });
        } else {
          const id = this.world.createSphere({
            radius: 0.3, position: { x: 0, y: 0.5 + i * 0.55, z: 0 },
            dynamic: true, restitution: 0.2, friction: 0.5
          });
          this._trackBody(id, { kind: 'sphere', radius: 0.3, dynamic: true, color: towerColors[i] });
        }
      }

      // Extra loose balls
      const loose = [
        { x: -2.5, y: 3.5, r: 0.35, color: '#00bcd4' },
        { x:  2.5, y: 4.5, r: 0.30, color: '#ff5722' },
        { x: -1.5, y: 5.5, r: 0.40, color: '#8bc34a' },
        { x:  1.5, y: 2.5, r: 0.28, color: '#ff9800' }
      ];
      for (const b of loose) {
        const id = this.world.createSphere({
          radius: b.r, position: { x: b.x, y: b.y, z: 0 },
          dynamic: true, restitution: 0.4, friction: 0.4
        });
        this._trackBody(id, { kind: 'sphere', radius: b.r, dynamic: true, color: b.color });
      }

      this.savedSnapshot = null;
      this.snapshotSavedAt = null;
      return;
    }

    // ─── Worker Thread Physics Demo ──────────────────────────────────────────
    if (exampleId === 'worker-physics') {
      // Reset state without creating a synchronous World (worker will own the physics)
      this.entities = new Map();
      this.constraints = new Map();
      this.ragdolls = new Map();
      this.latestWorkerSnapshot = null;
      this._setupWorkerExample();
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
    // If the example was already switched away, abort
    if (this.exampleId !== 'worker-physics') {
      await pw.terminate().catch(() => {});
      return;
    }

    pw.onState((snap) => { this.latestWorkerSnapshot = snap; });

    // Ground
    const groundId = await pw.createBox({
      halfExtents: { x: 100, y: 1, z: 100 },
      position: { x: 0, y: -1, z: 0 }, dynamic: false, friction: 0.8
    });
    this.entities.set(groundId, { bodyId: groundId, kind: 'box', halfExtents: { x: 100, y: 1, z: 100 }, dynamic: false, color: '#444' });

    // Colourful grid of bouncy spheres
    const colors = ['#e53935', '#fb8c00', '#43a047', '#1e88e5', '#9c27b0', '#00bcd4', '#ff5722', '#8bc34a', '#fdd835', '#e91e63'];
    let idx = 0;
    for (let row = 0; row < 2; row++) {
      for (let col = 0; col < 5; col++) {
        const r = 0.28 + (idx % 3) * 0.06;
        const id = await pw.createSphere({
          radius: r,
          position: { x: (col - 2) * 1.5, y: 4 + row * 1.8, z: (row - 0.5) * 0.6 },
          dynamic: true, restitution: 0.55, friction: 0.4
        });
        this.entities.set(id, { bodyId: id, kind: 'sphere', radius: r, dynamic: true, color: colors[idx % colors.length] });
        idx++;
      }
    }

    // Three falling boxes
    const boxColors = ['#5c6bc0', '#26a69a', '#ef5350'];
    for (let i = 0; i < 3; i++) {
      const he = 0.3 + i * 0.08;
      const id = await pw.createBox({
        halfExtents: { x: he, y: he, z: he },
        position: { x: (i - 1) * 1.2, y: 7 + i * 0.5, z: 0.5 },
        dynamic: true, restitution: 0.2, friction: 0.5
      });
      this.entities.set(id, { bodyId: id, kind: 'box', halfExtents: { x: he, y: he, z: he }, dynamic: true, color: boxColors[i] });
    }

    this.workerInst = pw;
  }

  executeCommand(input) {
    const cmd = String(input?.type || '');

    if (cmd === 'pause') {
      this.running = false;
      return;
    }

    if (cmd === 'play') {
      this.running = true;
      return;
    }

    if (cmd === 'step') {
      const count = Math.max(1, Math.min(240, Number(input?.count || 1)));
      for (let i = 0; i < count; i += 1) {
        this.world.step(1 / 60);
        this.tick += 1;
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
      const x = Number(input?.x || 0);
      const y = Number(input?.y || 5);
      const z = Number(input?.z || 0);
      const id = this.world.createSphere({
        radius,
        position: { x, y, z },
        dynamic: true,
        restitution: 0.2,
        friction: 0.6
      });
      this._trackBody(id, { kind: 'sphere', radius, dynamic: true, color: '#0097a7' });
      return;
    }

    if (cmd === 'apply_impulse') {
      const bodyId = Number(input?.bodyId);
      if (!Number.isFinite(bodyId)) throw new Error('bodyId is required');
      this.world.applyImpulse(bodyId, {
        x: Number(input?.x || 0),
        y: Number(input?.y || 0),
        z: Number(input?.z || 0)
      });
      return;
    }

    if (cmd === 'ray_cast') {
      const origin = {
        x: Number(input?.ox || 0),
        y: Number(input?.oy || 0),
        z: Number(input?.oz || 0)
      };
      const direction = {
        x: Number(input?.dx || 0),
        y: Number(input?.dy || -1),
        z: Number(input?.dz || 0)
      };
      const maxDistance = Number(input?.maxDistance || 30);

      const filter = {};
      if (input?.layerMask != null) filter.layerMask = Number(input.layerMask) >>> 0;
      if (input?.excludeBodyId != null) filter.excludeBodyIds = [Number(input.excludeBodyId) >>> 0];
      const hasFilter = Object.keys(filter).length > 0;
      const filterArg = hasFilter ? filter : undefined;

      let hit = null;
      let allHits = null;
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
      const origin = {
        x: Number(input?.ox || 0),
        y: Number(input?.oy || 0),
        z: Number(input?.oz || 0)
      };
      const direction = {
        x: Number(input?.dx || 0),
        y: Number(input?.dy || -1),
        z: Number(input?.dz || 0)
      };
      const maxDistance = Number(input?.maxDistance || 12);
      const radius = Number(input?.radius || 0.3);
      const hits = this.world.castSphereAll({ origin, direction, maxDistance, radius });
      this.lastSphereCast = { atTick: this.tick, origin, direction, maxDistance, radius, hits };
      return;
    }

    if (cmd === 'collide_sphere') {
      const center = {
        x: Number(input?.cx || 0),
        y: Number(input?.cy || 0),
        z: Number(input?.cz || 0)
      };
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
      if (!this.motorHandles) throw new Error('Motor presets are available only in "Motors + Queries + Events" example');
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
      if (!this.savedSnapshot) throw new Error('No snapshot saved yet — press Save Checkpoint first');
      this.world.applySnapshot(this.savedSnapshot);
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
    const bodies = this.motorHandles?.bodies || [];
    for (const bodyId of bodies) {
      try {
        this.world.activateBody(bodyId);
      } catch {}
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
        bodies.push({
          bodyId, ...meta,
          position: { x: px, y: py, z: pz },
          rotation: { x: rx, y: ry, z: rz, w: rw },
          velocity: { x: 0, y: 0, z: 0 }
        });
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
          console.error(`[getState] Body ${bodyId} (kind=${meta?.kind}) removed: ${e.message || e}`);
          this.entities.delete(bodyId);
        }
      }
    }

    const ragdolls = [];
    for (const ragdoll of this.ragdolls.values()) {
      try {
        const count = ragdoll.bodyCount();
        const bones = [];
        for (let i = 0; i < count; i += 1) bones.push(ragdoll.getBoneTransform(i));
        ragdolls.push({ ragdollId: ragdoll.id, bones });
      } catch {
        this.ragdolls.delete(ragdoll.id);
      }
    }

    return {
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
      snapshotSavedAt: this.snapshotSavedAt
    };
  }
}

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
  { id: 'worker-physics', name: 'Worker Thread Physics' }
];

const engine = new DemoEngine();

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);

    if (req.method === 'GET' && url.pathname === '/api/examples') {
      return json(res, 200, { ok: true, examples: EXAMPLES });
    }

    if (req.method === 'GET' && url.pathname === '/api/state') {
      return json(res, 200, { ok: true, state: engine.getState() });
    }

    if (req.method === 'POST' && url.pathname === '/api/example') {
      const body = await readJson(req);
      const exampleId = String(body.exampleId || '');
      if (!EXAMPLES.some(e => e.id === exampleId)) return json(res, 400, { ok: false, error: 'Unknown exampleId' });
      engine.reset(exampleId);
      return json(res, 200, { ok: true, state: engine.getState() });
    }

    if (req.method === 'POST' && url.pathname === '/api/command') {
      const body = await readJson(req);
      engine.executeCommand(body);
      return json(res, 200, { ok: true, state: engine.getState() });
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
      res.writeHead(200, {
        'Content-Type': contentType,
        'Cache-Control': 'no-store'
      });
      res.end(data);
      return;
    }

    json(res, 405, { ok: false, error: 'Method not allowed' });
  } catch (error) {
    json(res, 500, { ok: false, error: error.message || 'Internal error' });
  }
});

server.listen(PORT, HOST, () => {
  console.log(`Examples server: http://${HOST}:${PORT}`);
});

process.on('SIGINT', () => {
  engine.destroy();
  server.close(() => process.exit(0));
});
