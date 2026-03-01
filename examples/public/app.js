import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

// ─── DOM refs ─────────────────────────────────────────────────────────────────
const exampleSelect = document.getElementById('exampleSelect');
const statusEl = document.getElementById('status');
const wsBadge = document.getElementById('wsBadge');
const canvas = document.getElementById('scene');

// ─── State (mirror of server) ─────────────────────────────────────────────────
let state = null;

// ─── Three.js setup ───────────────────────────────────────────────────────────
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
renderer.setSize(canvas.clientWidth, canvas.clientHeight, false);
renderer.shadowMap.enabled = true;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0xdde8f2);
scene.fog = new THREE.Fog(0xdde8f2, 20, 70);

const camera = new THREE.PerspectiveCamera(55, canvas.clientWidth / canvas.clientHeight, 0.1, 500);
camera.position.set(10, 9, 12);

const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(0, 2, 0);
controls.enableDamping = true;
controls.dampingFactor = 0.08;

const hemi = new THREE.HemisphereLight(0xffffff, 0x7c8da4, 0.7);
scene.add(hemi);
const dir = new THREE.DirectionalLight(0xffffff, 0.9);
dir.position.set(10, 20, 8);
dir.castShadow = true;
dir.shadow.mapSize.set(1024, 1024);
scene.add(dir);

const grid = new THREE.GridHelper(80, 80, 0x69809a, 0x9db2c8);
grid.position.y = 0.02;
scene.add(grid);

const axes = new THREE.AxesHelper(1.4);
axes.position.set(-6.5, -1, -6.5);
scene.add(axes);

// ─── Scene object maps ────────────────────────────────────────────────────────
const bodyMeshes = new Map();
const constraintLines = new Map();
const ragdollGroups = new Map();

let rayLine = null, rayHitSphere = null, rayAllHitsGroup = null;
let sphereCastGroup = null, sphereCollideGroup = null;
let boundsBox = null;  // wireframe AABB for ragdoll-extended

// ─── Mixamo Erika ──────────────────────────────────────────────────────────────
// Major Mixamo bones we drive with Jolt. Must be a subset of what's in the GLB.
const ERIKA_BONE_NAMES = [
  'Hips',
  'Spine', 'Spine1', 'Spine2',
  'Neck', 'Head',
  'LeftShoulder', 'LeftArm', 'LeftForeArm',
  'RightShoulder', 'RightArm', 'RightForeArm',
  'LeftUpLeg', 'LeftLeg', 'LeftFoot',
  'RightUpLeg', 'RightLeg', 'RightFoot'
];
const ERIKA_BONE_SET = new Set(ERIKA_BONE_NAMES);

// Parent hierarchy for local-transform computation
const ERIKA_PARENTS = {
  Hips: null,
  Spine: 'Hips',      Spine1: 'Spine',   Spine2: 'Spine1',
  Neck: 'Spine2',     Head: 'Neck',
  LeftShoulder: 'Spine2',  LeftArm: 'LeftShoulder',  LeftForeArm: 'LeftArm',
  RightShoulder: 'Spine2', RightArm: 'RightShoulder', RightForeArm: 'RightArm',
  LeftUpLeg: 'Hips',  LeftLeg: 'LeftUpLeg',  LeftFoot: 'LeftLeg',
  RightUpLeg: 'Hips', RightLeg: 'RightUpLeg', RightFoot: 'RightLeg'
};

// Topological order (parent always before child)
const ERIKA_TOPO = [
  'Hips',
  'Spine', 'LeftUpLeg', 'RightUpLeg',
  'Spine1', 'LeftLeg', 'RightLeg',
  'Spine2', 'LeftFoot', 'RightFoot',
  'Neck', 'LeftShoulder', 'RightShoulder',
  'Head', 'LeftArm', 'RightArm',
  'LeftForeArm', 'RightForeArm'
];

let erikaGltf = null;
let erikaMesh = null;
const erikaBoneMap = new Map();   // normalized name → THREE.Bone
let erikaSetupSent = false;
// Bind-pose (T-pose) local quaternions from the GLB, stored once on load.
// Used in animated mode to drive the Three.js skeleton from bind pose + deltas
// instead of converting Jolt body world rotations (which has coordinate issues).
let erikaBindQuats = null; // Map<name, THREE.Quaternion>
let erikaAnimClock = 0;    // client-side animation phase (seconds)

// Strip "mixamorig" prefix if present
function normBone(name) {
  return name.startsWith('mixamorig') ? name.slice(9) : name;
}

// Load the character model
const gltfLoader = new GLTFLoader();
gltfLoader.load('/erika.glb', (gltf) => {
  erikaGltf = gltf;
  gltf.scene.traverse(o => {
    if (o.isSkinnedMesh && !erikaMesh) {
      erikaMesh = o;
      o.frustumCulled = false;
    }
  });
  if (erikaMesh) {
    gltf.scene.updateWorldMatrix(true, true);
    erikaMesh.skeleton.bones.forEach(b => {
      erikaBoneMap.set(normBone(b.name), b);
    });
  }
  // Store bind-pose (T-pose) local quaternions for each tracked bone.
  // Must be done AFTER updateWorldMatrix so skeleton is in its rest position.
  erikaBindQuats = new Map();
  for (const [name, bone] of erikaBoneMap.entries()) {
    erikaBindQuats.set(name, bone.quaternion.clone());
  }

  gltf.scene.visible = false;
  scene.add(gltf.scene);
  // If mixamo-erika is already the active example, trigger setup now
  if (state?.exampleId === 'mixamo-erika') maybeSetupErika();
}, undefined, (err) => console.warn('erika.glb load error:', err));

function maybeSetupErika() {
  if (erikaSetupSent || !erikaMesh || state?.exampleId !== 'mixamo-erika') return;
  const bones = [];
  const wp = new THREE.Vector3(), wq = new THREE.Quaternion();
  erikaGltf.scene.updateWorldMatrix(true, true);
  for (const name of ERIKA_TOPO) {
    const bone = erikaBoneMap.get(name);
    if (!bone) continue;
    bone.getWorldPosition(wp);
    bone.getWorldQuaternion(wq);
    bones.push({
      name,
      parentName: (ERIKA_PARENTS[name] && ERIKA_BONE_SET.has(ERIKA_PARENTS[name])) ? ERIKA_PARENTS[name] : null,
      x: wp.x, y: wp.y, z: wp.z,
      qx: wq.x, qy: wq.y, qz: wq.z, qw: wq.w
    });
  }
  if (bones.length > 0) {
    command({ type: 'setup_erika_ragdoll', bones });
    erikaSetupSent = true;
  }
}

// Reusable temporaries for updateErikaModel (avoid per-frame allocation)
const _eq = new THREE.Quaternion();
const _eq2 = new THREE.Quaternion();
const _em = new THREE.Matrix4();
const _ev = new THREE.Vector3();
const _ax = new THREE.Vector3();

function updateErikaModel(erikaData) {
  if (!erikaGltf) return;
  const active = state?.exampleId === 'mixamo-erika';
  erikaGltf.scene.visible = active;
  if (!active || !erikaData || !erikaMesh) return;

  if (erikaData.mode === 'animated') {
    // ── Animated mode ────────────────────────────────────────────────────────
    // Jolt body rotations can't be reliably converted to Three.js bone local
    // space due to armature/coordinate system differences.
    // Instead: drive the skeleton from the stored GLB bind-pose quaternions +
    // small client-side idle animation deltas, exactly like the server does in
    // SkeletonPose local space.  Jolt is only used for the root (Hips) position
    // so the ragdoll stays grounded correctly.
    if (!erikaBindQuats) return;

    erikaAnimClock += 1 / 60;
    const t = erikaAnimClock;

    // 1. Reset all bones to bind pose
    for (const [name, bone] of erikaBoneMap.entries()) {
      const bq = erikaBindQuats.get(name);
      if (bq) bone.quaternion.copy(bq);
    }

    // 2. Apply small idle-sway overrides (in bone LOCAL space, premultiply = parent-frame delta)
    const rotLocal = (name, ax, ay, az, angle) => {
      const bone = erikaBoneMap.get(name);
      const bq = erikaBindQuats.get(name);
      if (!bone || !bq) return;
      _ax.set(ax, ay, az);
      _eq.setFromAxisAngle(_ax, angle);
      bone.quaternion.copy(bq).premultiply(_eq);
    };
    rotLocal('Hips',        0, 0, 1, Math.sin(t * 1.1) * 0.03);
    rotLocal('Spine',       0, 0, 1, Math.sin(t * 1.1) * 0.04);
    rotLocal('Spine2',      1, 0, 0, Math.sin(t * 1.2) * 0.04);
    rotLocal('Neck',        0, 1, 0, Math.sin(t * 0.6) * 0.12);
    // Head: combine two rotations
    _ax.set(0, 1, 0); _eq.setFromAxisAngle(_ax, Math.sin(t * 0.6) * 0.22);
    _ax.set(1, 0, 0); _eq2.setFromAxisAngle(_ax, Math.sin(t * 0.9 + 1.0) * 0.10);
    _eq.multiply(_eq2);
    const headBone = erikaBoneMap.get('Head'), headBQ = erikaBindQuats.get('Head');
    if (headBone && headBQ) headBone.quaternion.copy(headBQ).premultiply(_eq);

    rotLocal('LeftArm',      1, 0, 0, Math.sin(t * 1.1 + Math.PI) * 0.10);
    rotLocal('RightArm',     1, 0, 0, Math.sin(t * 1.1) * 0.10);
    rotLocal('LeftForeArm',  1, 0, 0, -0.10 + Math.sin(t * 1.1 + Math.PI) * 0.04);
    rotLocal('RightForeArm', 1, 0, 0, -0.10 + Math.sin(t * 1.1) * 0.04);

    // 3. Place Hips at the Jolt body position so the character stays grounded
    const rootBone = erikaBoneMap.get('Hips');
    const rootData = erikaData.bones.find(b => ERIKA_PARENTS[b.name] == null);
    if (rootBone && rootData) {
      const armature = rootBone.parent;
      if (armature && armature.isObject3D) {
        armature.updateWorldMatrix(true, false);
        _em.copy(armature.matrixWorld).invert();
        _ev.set(rootData.position.x, rootData.position.y, rootData.position.z);
        _ev.applyMatrix4(_em);
        rootBone.position.copy(_ev);
      }
    }
    return;
  }

  // ── Ragdoll mode ─────────────────────────────────────────────────────────
  // Drive the Three.js skeleton from Jolt body world rotations.
  const worldQuat = new Map();
  const rootWP = new THREE.Vector3();
  for (const b of erikaData.bones) {
    worldQuat.set(b.name, new THREE.Quaternion(b.rotation.x, b.rotation.y, b.rotation.z, b.rotation.w));
    if (ERIKA_PARENTS[b.name] == null) rootWP.set(b.position.x, b.position.y, b.position.z);
  }

  for (const name of ERIKA_TOPO) {
    const bone = erikaBoneMap.get(name);
    if (!bone) continue;
    const wq = worldQuat.get(name);
    if (!wq) continue;

    if (ERIKA_PARENTS[name] == null) {
      const armature = bone.parent;
      if (armature && armature.isObject3D) {
        armature.updateWorldMatrix(true, false);
        _em.copy(armature.matrixWorld).invert();
        _ev.copy(rootWP).applyMatrix4(_em);
        bone.position.copy(_ev);
        armature.getWorldQuaternion(_eq).invert();
        bone.quaternion.copy(_eq).multiply(wq);
      } else {
        bone.position.copy(rootWP);
        bone.quaternion.copy(wq);
      }
    } else {
      const parentWQ = worldQuat.get(ERIKA_PARENTS[name]);
      if (parentWQ) {
        _eq.copy(parentWQ).invert();
        bone.quaternion.copy(_eq).multiply(wq);
      }
    }
  }
}

// ─── Helpers ──────────────────────────────────────────────────────────────────
function hexToColor(hex) { return new THREE.Color(hex || '#4477aa'); }

function disposeObject(obj) {
  if (!obj) return;
  obj.traverse(o => {
    o.geometry?.dispose?.();
    if (Array.isArray(o.material)) o.material.forEach(m => m.dispose?.());
    else o.material?.dispose?.();
  });
}

// ─── Body mesh builders ───────────────────────────────────────────────────────
function makeHeightFieldGeometry(body) {
  const { samples, sampleCount, cellSize = 1 } = body;
  if (!samples || !sampleCount) throw new Error(`makeHeightFieldGeometry: missing data`);
  const N = sampleCount;
  const positions = [];
  const indices = [];
  for (let r = 0; r < N; r++) {
    for (let c = 0; c < N; c++) {
      positions.push((c - (N - 1) / 2) * cellSize, samples[r * N + c], (r - (N - 1) / 2) * cellSize);
    }
  }
  for (let r = 0; r < N - 1; r++) {
    for (let c = 0; c < N - 1; c++) {
      const a = r * N + c, b = r * N + c + 1, d = (r + 1) * N + c, e = (r + 1) * N + c + 1;
      indices.push(a, d, b, b, d, e);
    }
  }
  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
  geom.setIndex(indices);
  geom.computeVertexNormals();
  return geom;
}

function makeSubMesh(sub, parentDynamic) {
  let geom;
  if (sub.kind === 'sphere') {
    geom = new THREE.SphereGeometry(Math.max(sub.radius || 0.2, 0.04), 20, 14);
  } else if (sub.kind === 'capsule') {
    geom = new THREE.CapsuleGeometry(Math.max(sub.radius || 0.15, 0.03), Math.max((sub.halfHeight || 0.2) * 2, 0.04), 8, 14);
  } else {
    const he = sub.halfExtents || { x: 0.25, y: 0.25, z: 0.25 };
    geom = new THREE.BoxGeometry(Math.max(he.x * 2, 0.01), Math.max(he.y * 2, 0.01), Math.max(he.z * 2, 0.01));
  }
  const mat = new THREE.MeshStandardMaterial({ color: hexToColor(sub.color), roughness: 0.55, metalness: parentDynamic ? 0.1 : 0, transparent: !parentDynamic, opacity: parentDynamic ? 1 : 0.82 });
  const mesh = new THREE.Mesh(geom, mat);
  mesh.castShadow = Boolean(parentDynamic);
  mesh.receiveShadow = true;
  if (sub.position) mesh.position.set(sub.position.x, sub.position.y, sub.position.z);
  if (sub.rotation) mesh.quaternion.set(sub.rotation.x, sub.rotation.y, sub.rotation.z, sub.rotation.w);
  return mesh;
}

function makeBodyMesh(body) {
  if (body.kind === 'heightfield') {
    const mat = new THREE.MeshStandardMaterial({ color: hexToColor(body.color || '#5d8a5a'), roughness: 0.85, metalness: 0, side: THREE.DoubleSide });
    const mesh = new THREE.Mesh(makeHeightFieldGeometry(body), mat);
    mesh.receiveShadow = true;
    return mesh;
  }
  if (body.subShapes && body.subShapes.length > 0) {
    const group = new THREE.Group();
    for (const sub of body.subShapes) group.add(makeSubMesh(sub, body.dynamic));
    return group;
  }
  let geometry;
  if (body.kind === 'sphere') {
    geometry = new THREE.SphereGeometry(Math.max(body.radius || 0.3, 0.08), 24, 18);
  } else if (body.kind === 'capsule') {
    geometry = new THREE.CapsuleGeometry(Math.max(body.radius || 0.2, 0.05), Math.max(body.length || 0.8, 0.05), 10, 18);
  } else {
    const hx = Math.max(body.halfExtents?.x || 0.3, 0.05);
    const hy = Math.max(body.halfExtents?.y || 0.3, 0.05);
    const hz = Math.max(body.halfExtents?.z || 0.3, 0.05);
    geometry = new THREE.BoxGeometry(hx * 2, hy * 2, hz * 2);
  }
  const material = new THREE.MeshStandardMaterial({ color: hexToColor(body.color), roughness: 0.55, metalness: body.dynamic ? 0.1 : 0, transparent: !body.dynamic, opacity: body.dynamic ? 1 : 0.8 });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.castShadow = Boolean(body.dynamic);
  mesh.receiveShadow = true;
  return mesh;
}

// ─── Scene update functions ───────────────────────────────────────────────────
// These are called on every incoming WebSocket state message.
// The 3D world is a strict reflection of what the server sends — no local physics.

function updateBodyMeshes() {
  if (!state) return;
  const seen = new Set();
  for (const body of state.bodies) {
    seen.add(body.bodyId);
    let mesh = bodyMeshes.get(body.bodyId);
    if (!mesh) {
      try { mesh = makeBodyMesh(body); }
      catch (err) { console.error(`makeBodyMesh failed body ${body.bodyId}:`, err); continue; }
      bodyMeshes.set(body.bodyId, mesh);
      scene.add(mesh);
    }
    mesh.position.set(body.position.x, body.position.y, body.position.z);
    mesh.quaternion.set(body.rotation.x, body.rotation.y, body.rotation.z, body.rotation.w);
  }
  for (const [bodyId, mesh] of bodyMeshes.entries()) {
    if (!seen.has(bodyId)) {
      scene.remove(mesh);
      disposeObject(mesh);
      bodyMeshes.delete(bodyId);
    }
  }
}

function updateConstraints() {
  if (!state) return;
  const bodyMap = new Map(state.bodies.map(b => [b.bodyId, b]));
  const seen = new Set();
  for (const c of state.constraints) {
    seen.add(c.constraintId);
    const a = bodyMap.get(c.bodyA), b = bodyMap.get(c.bodyB);
    if (!a || !b) continue;
    let line = constraintLines.get(c.constraintId);
    if (!line) {
      const geom = new THREE.BufferGeometry();
      geom.setAttribute('position', new THREE.Float32BufferAttribute([0, 0, 0, 0, 0, 0], 3));
      line = new THREE.Line(geom, new THREE.LineBasicMaterial({ color: hexToColor(c.color) }));
      constraintLines.set(c.constraintId, line);
      scene.add(line);
    }
    const pos = line.geometry.attributes.position.array;
    pos[0] = a.position.x; pos[1] = a.position.y; pos[2] = a.position.z;
    pos[3] = b.position.x; pos[4] = b.position.y; pos[5] = b.position.z;
    line.geometry.attributes.position.needsUpdate = true;
  }
  for (const [id, line] of constraintLines.entries()) {
    if (!seen.has(id)) { scene.remove(line); disposeObject(line); constraintLines.delete(id); }
  }
}

function updateRagdolls() {
  if (!state) return;
  // In mixamo-erika the skinned mesh handles visuals; clear any debug geometry
  if (state.exampleId === 'mixamo-erika') {
    for (const [id, group] of ragdollGroups.entries()) {
      scene.remove(group); disposeObject(group); ragdollGroups.delete(id);
    }
    return;
  }
  const seen = new Set();
  for (const rag of state.ragdolls) {
    seen.add(rag.ragdollId);
    let group = ragdollGroups.get(rag.ragdollId);
    if (!group) {
      group = new THREE.Group();
      ragdollGroups.set(rag.ragdollId, group);
      scene.add(group);
    }
    while (group.children.length > 0) {
      const child = group.children.pop();
      disposeObject(child);
    }
    for (let i = 0; i < rag.bones.length; i++) {
      const bone = rag.bones[i];
      const mesh = new THREE.Mesh(
        new THREE.SphereGeometry(0.12, 16, 12),
        new THREE.MeshStandardMaterial({ color: 0xffd54f, roughness: 0.35, metalness: 0.2 })
      );
      mesh.position.set(bone.position.x, bone.position.y, bone.position.z);
      group.add(mesh);
      if (i > 0) {
        const prev = rag.bones[i - 1];
        const geom = new THREE.BufferGeometry().setFromPoints([
          new THREE.Vector3(prev.position.x, prev.position.y, prev.position.z),
          new THREE.Vector3(bone.position.x, bone.position.y, bone.position.z)
        ]);
        group.add(new THREE.Line(geom, new THREE.LineBasicMaterial({ color: 0x333333 })));
      }
    }
  }
  for (const [id, group] of ragdollGroups.entries()) {
    if (!seen.has(id)) { scene.remove(group); disposeObject(group); ragdollGroups.delete(id); }
  }
}

function updateRay() {
  if (rayLine) { scene.remove(rayLine); disposeObject(rayLine); rayLine = null; }
  if (rayHitSphere) { scene.remove(rayHitSphere); disposeObject(rayHitSphere); rayHitSphere = null; }
  if (rayAllHitsGroup) { scene.remove(rayAllHitsGroup); disposeObject(rayAllHitsGroup); rayAllHitsGroup = null; }
  if (!state?.lastRay) return;
  const ray = state.lastRay;
  const end = { x: ray.origin.x + ray.direction.x * ray.maxDistance, y: ray.origin.y + ray.direction.y * ray.maxDistance, z: ray.origin.z + ray.direction.z * ray.maxDistance };
  rayLine = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(ray.origin.x, ray.origin.y, ray.origin.z), new THREE.Vector3(end.x, end.y, end.z)]),
    new THREE.LineDashedMaterial({ color: 0xd50000, dashSize: 0.25, gapSize: 0.18 })
  );
  rayLine.computeLineDistances();
  scene.add(rayLine);
  if (ray.allHits && ray.allHits.length > 0) {
    rayAllHitsGroup = new THREE.Group();
    ray.allHits.forEach((hit, i) => {
      const hp = { x: ray.origin.x + ray.direction.x * ray.maxDistance * hit.fraction, y: ray.origin.y + ray.direction.y * ray.maxDistance * hit.fraction, z: ray.origin.z + ray.direction.z * ray.maxDistance * hit.fraction };
      const color = new THREE.Color().setHSL(0.08 + i / Math.max(ray.allHits.length - 1, 1) * 0.55, 1, 0.55);
      const marker = new THREE.Mesh(new THREE.SphereGeometry(0.14, 14, 10), new THREE.MeshBasicMaterial({ color }));
      marker.position.set(hp.x, hp.y, hp.z);
      rayAllHitsGroup.add(marker);
    });
    scene.add(rayAllHitsGroup);
  } else if (ray.hit) {
    const hp = { x: ray.origin.x + ray.direction.x * ray.maxDistance * ray.hit.fraction, y: ray.origin.y + ray.direction.y * ray.maxDistance * ray.hit.fraction, z: ray.origin.z + ray.direction.z * ray.maxDistance * ray.hit.fraction };
    rayHitSphere = new THREE.Mesh(new THREE.SphereGeometry(0.13, 14, 10), new THREE.MeshBasicMaterial({ color: 0xd50000 }));
    rayHitSphere.position.set(hp.x, hp.y, hp.z);
    scene.add(rayHitSphere);
  }
}

function updateSphereQueries() {
  if (sphereCastGroup) { scene.remove(sphereCastGroup); disposeObject(sphereCastGroup); sphereCastGroup = null; }
  if (sphereCollideGroup) { scene.remove(sphereCollideGroup); disposeObject(sphereCollideGroup); sphereCollideGroup = null; }

  if (state?.lastSphereCast) {
    const q = state.lastSphereCast;
    sphereCastGroup = new THREE.Group();
    const start = new THREE.Vector3(q.origin.x, q.origin.y, q.origin.z);
    const end = new THREE.Vector3(q.origin.x + q.direction.x * q.maxDistance, q.origin.y + q.direction.y * q.maxDistance, q.origin.z + q.direction.z * q.maxDistance);
    const line = new THREE.Line(new THREE.BufferGeometry().setFromPoints([start, end]), new THREE.LineDashedMaterial({ color: 0x8e24aa, dashSize: 0.2, gapSize: 0.14 }));
    line.computeLineDistances();
    sphereCastGroup.add(line);
    const startSphere = new THREE.Mesh(new THREE.SphereGeometry(q.radius, 18, 12), new THREE.MeshBasicMaterial({ color: 0x8e24aa, transparent: true, opacity: 0.18 }));
    startSphere.position.copy(start);
    sphereCastGroup.add(startSphere);
    for (const hit of q.hits || []) {
      const hp = new THREE.Vector3(q.origin.x + q.direction.x * q.maxDistance * hit.fraction, q.origin.y + q.direction.y * q.maxDistance * hit.fraction, q.origin.z + q.direction.z * q.maxDistance * hit.fraction);
      const marker = new THREE.Mesh(new THREE.SphereGeometry(0.1, 14, 10), new THREE.MeshBasicMaterial({ color: 0xce93d8 }));
      marker.position.copy(hp);
      sphereCastGroup.add(marker);
    }
    scene.add(sphereCastGroup);
  }

  if (state?.lastSphereCollide) {
    const q = state.lastSphereCollide;
    sphereCollideGroup = new THREE.Group();
    const center = new THREE.Vector3(q.center.x, q.center.y, q.center.z);
    const sphere = new THREE.Mesh(new THREE.SphereGeometry(q.radius, 22, 16), new THREE.MeshBasicMaterial({ color: 0x00acc1, transparent: true, opacity: 0.16 }));
    sphere.position.copy(center);
    sphereCollideGroup.add(sphere);
    for (const hit of q.hits || []) {
      const marker = new THREE.Mesh(new THREE.SphereGeometry(0.09, 14, 10), new THREE.MeshBasicMaterial({ color: 0x26c6da }));
      marker.position.set(hit.point.x, hit.point.y, hit.point.z);
      sphereCollideGroup.add(marker);
    }
    scene.add(sphereCollideGroup);
  }
}

// Wireframe AABB for ragdoll-extended worldSpaceBounds
function updateBoundsBox() {
  if (boundsBox) { scene.remove(boundsBox); disposeObject(boundsBox); boundsBox = null; }
  const bounds = state?.extendedRagdoll?.worldSpaceBounds;
  if (!bounds) return;
  const sx = bounds.max.x - bounds.min.x;
  const sy = bounds.max.y - bounds.min.y;
  const sz = bounds.max.z - bounds.min.z;
  if (sx <= 0 || sy <= 0 || sz <= 0) return;
  const geom = new THREE.BoxGeometry(sx, sy, sz);
  const edges = new THREE.EdgesGeometry(geom);
  geom.dispose();
  boundsBox = new THREE.LineSegments(edges, new THREE.LineBasicMaterial({ color: 0x00ff88 }));
  boundsBox.position.set(
    (bounds.min.x + bounds.max.x) / 2,
    (bounds.min.y + bounds.max.y) / 2,
    (bounds.min.z + bounds.max.z) / 2
  );
  scene.add(boundsBox);
}

function updateStatus() {
  if (!state) return;
  const eventLines = (state.events || []).slice(-8).map(e => {
    const pair = e.bodyB != null ? `${e.bodyA}/${e.bodyB}` : `${e.bodyA}`;
    return `  [${e.atTick}] ${e.source}:${e.type} ${pair}`;
  });

  const extra = [];
  if (state.filterState) {
    const { layerMask, excludeBodyId, hitCount } = state.filterState;
    extra.push(`filter: layerMask=0b${(layerMask >>> 0).toString(2).padStart(2, '0')} exclude=${excludeBodyId ?? '-'} hits=${hitCount}`);
  }
  if (state.springParams) {
    const { frequency, damping, hingeTarget, sliderTarget } = state.springParams;
    extra.push(`spring: freq=${frequency} damp=${damping} hingeAngle=${hingeTarget} sliderPos=${sliderTarget}`);
  }
  if (state.workerMode) extra.push('physics: Worker Thread');
  if (state.snapshotSavedAt != null) extra.push(`checkpoint: saved at tick ${state.snapshotSavedAt}`);
  if (state.skeletonPose) extra.push(`pose mode: ${state.skeletonPose.animated ? 'ANIMATED (setPose each tick)' : 'PHYSICS (gravity)'}`);
  if (state.exampleId === 'mixamo-erika') {
    if (state.erikaRagdoll) {
      const mode = state.erikaRagdoll.mode === 'animated' ? '🎭 Animated (R = ragdoll)' : '💀 Ragdoll (R = animate)';
      extra.push(`erika mode: ${mode}`);
      extra.push(`erika bones: ${state.erikaRagdoll.bones.length}  WASD/↑↓←→ push  Space jump`);
    } else {
      extra.push('erika: waiting for GLB load...');
    }
  }
  if (state.extendedRagdoll) {
    const rd = state.extendedRagdoll;
    const rt = rd.rootTransform;
    const bb = rd.worldSpaceBounds;
    extra.push(`ragdoll active: ${rd.isActive}`);
    extra.push(`ragdoll in physics: ${rd.inPhysics}`);
    if (rt) extra.push(`root pos: (${rt.position.x.toFixed(2)}, ${rt.position.y.toFixed(2)}, ${rt.position.z.toFixed(2)})`);
    if (bb) extra.push(`AABB: (${bb.min.x.toFixed(2)},${bb.min.y.toFixed(2)},${bb.min.z.toFixed(2)}) → (${bb.max.x.toFixed(2)},${bb.max.y.toFixed(2)},${bb.max.z.toFixed(2)})`);
  }

  statusEl.textContent = [
    `example: ${state.exampleId}`,
    `running: ${state.running}  tick: ${state.tick}`,
    `bodies: ${state.bodies.length}  constraints: ${state.constraints.length}  ragdolls: ${state.ragdolls.length}`,
    ...extra,
    '',
    'Bodies:',
    ...state.bodies.map(b => `  [${b.bodyId}] ${b.kind || '?'} dyn=${b.dynamic} y=${b.position?.y?.toFixed(2) ?? '?'}`),
    '',
    'Recent events:',
    ...(eventLines.length ? eventLines : ['  -'])
  ].join('\n');
}

// Master update — called on every state push from server
function applyState(newState) {
  const prevExample = state?.exampleId;
  state = newState;

  // Reset erika setup flag when leaving (or re-entering) the example
  if (state.exampleId !== prevExample) {
    if (state.exampleId !== 'mixamo-erika') erikaSetupSent = false;
    if (state.exampleId === 'mixamo-erika') {
      erikaSetupSent = false;
      maybeSetupErika();
    }
  }
  // Also trigger if we're already in mixamo-erika but ragdoll isn't created yet
  if (state.exampleId === 'mixamo-erika' && !state.erikaRagdoll && !erikaSetupSent) {
    maybeSetupErika();
  }

  updateBodyMeshes();
  updateConstraints();
  updateRagdolls();
  updateRay();
  updateSphereQueries();
  updateBoundsBox();
  updateErikaModel(state.erikaRagdoll);
  updateStatus();
}

// ─── Three.js render loop (runs at vsync; camera is always smooth) ─────────────
function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}

function onResize() {
  const w = canvas.clientWidth, h = canvas.clientHeight;
  renderer.setSize(w, h, false);
  camera.aspect = w / Math.max(h, 1);
  camera.updateProjectionMatrix();
}
window.addEventListener('resize', onResize);
onResize();
animate();

// ─── WebSocket transport ──────────────────────────────────────────────────────
let ws = null;

function connect() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(`${proto}//${location.host}`);

  ws.onopen = () => {
    wsBadge.textContent = '● Connected';
    wsBadge.className = 'ws-connected';
  };

  ws.onclose = () => {
    wsBadge.textContent = '○ Disconnected';
    wsBadge.className = 'ws-disconnected';
    ws = null;
    setTimeout(connect, 2000);
  };

  ws.onerror = () => {};

  ws.onmessage = (event) => {
    let msg;
    try { msg = JSON.parse(event.data); } catch { return; }

    if (msg.type === 'state') {
      applyState(msg.data);
    } else if (msg.type === 'examples') {
      exampleSelect.innerHTML = '';
      for (const ex of msg.data) {
        const opt = document.createElement('option');
        opt.value = ex.id;
        opt.textContent = ex.name;
        exampleSelect.appendChild(opt);
      }
    } else if (msg.type === 'error') {
      console.error('[server]', msg.message);
      statusEl.textContent = `Server error: ${msg.message}`;
    }
  };
}

function send(msg) {
  if (ws?.readyState === WebSocket.OPEN) ws.send(JSON.stringify(msg));
}

function command(payload) { send({ type: 'command', payload }); }
function loadExample(id) { erikaSetupSent = false; erikaAnimClock = 0; send({ type: 'load_example', id }); }

// ─── UI event handlers ────────────────────────────────────────────────────────
document.getElementById('resetBtn').addEventListener('click', () => {
  loadExample(exampleSelect.value);
});
document.getElementById('playBtn').addEventListener('click', () => command({ type: 'play' }));
document.getElementById('pauseBtn').addEventListener('click', () => command({ type: 'pause' }));
document.getElementById('stepBtn').addEventListener('click', () => command({ type: 'step', count: 10 }));

document.getElementById('spawnBtn').addEventListener('click', () => command({
  type: 'spawn_sphere',
  x: Number(document.getElementById('spawnX').value),
  y: Number(document.getElementById('spawnY').value),
  z: 0,
  radius: Number(document.getElementById('spawnR').value)
}));

document.getElementById('impulseBtn').addEventListener('click', () => command({
  type: 'apply_impulse',
  bodyId: Number(document.getElementById('bodyId').value),
  x: Number(document.getElementById('impX').value),
  y: Number(document.getElementById('impY').value),
  z: Number(document.getElementById('impZ').value)
}));

document.getElementById('rayBtn').addEventListener('click', () => command({
  type: 'ray_cast',
  ox: Number(document.getElementById('rayOX').value), oy: Number(document.getElementById('rayOY').value), oz: Number(document.getElementById('rayOZ').value),
  dx: Number(document.getElementById('rayDX').value), dy: Number(document.getElementById('rayDY').value), dz: Number(document.getElementById('rayDZ').value),
  maxDistance: Number(document.getElementById('rayMax').value)
}));

document.getElementById('sphereCastBtn').addEventListener('click', () => command({
  type: 'cast_sphere',
  ox: Number(document.getElementById('scOX').value), oy: Number(document.getElementById('scOY').value), oz: Number(document.getElementById('scOZ').value),
  dx: Number(document.getElementById('scDX').value), dy: Number(document.getElementById('scDY').value), dz: Number(document.getElementById('scDZ').value),
  maxDistance: Number(document.getElementById('scMax').value),
  radius: Number(document.getElementById('scR').value)
}));

document.getElementById('sphereCollideBtn').addEventListener('click', () => command({
  type: 'collide_sphere',
  cx: Number(document.getElementById('coX').value), cy: Number(document.getElementById('coY').value), cz: Number(document.getElementById('coZ').value),
  radius: Number(document.getElementById('coR').value),
  maxSeparation: Number(document.getElementById('coSep').value)
}));

document.getElementById('saveSnapBtn').addEventListener('click', () => command({ type: 'save_snapshot' }));
document.getElementById('restoreSnapBtn').addEventListener('click', () => command({ type: 'restore_snapshot' }));

document.getElementById('motorOffBtn').addEventListener('click', () => command({ type: 'set_motor_preset', preset: 'off' }));
document.getElementById('motorVelBtn').addEventListener('click', () => command({ type: 'set_motor_preset', preset: 'velocity' }));
document.getElementById('motorPosBtn').addEventListener('click', () => command({ type: 'set_motor_preset', preset: 'position' }));

document.getElementById('spApplyBtn').addEventListener('click', () => command({
  type: 'set_spring_params',
  frequency: Number(document.getElementById('spFreq').value),
  damping: Number(document.getElementById('spDamp').value),
  hingeTarget: Number(document.getElementById('spHingeTarget').value),
  sliderTarget: Number(document.getElementById('spSliderTarget').value)
}));

function getRayOriginFromInputs() {
  return {
    ox: Number(document.getElementById('rayOX').value), oy: Number(document.getElementById('rayOY').value), oz: Number(document.getElementById('rayOZ').value),
    dx: Number(document.getElementById('rayDX').value), dy: Number(document.getElementById('rayDY').value), dz: Number(document.getElementById('rayDZ').value),
    maxDistance: Number(document.getElementById('rayMax').value)
  };
}
document.getElementById('qfAllBtn').addEventListener('click', () => {
  const base = getRayOriginFromInputs();
  const lm = document.getElementById('qfLayerMask').value.trim();
  const ex = document.getElementById('qfExclude').value.trim();
  const extra = { allHits: true };
  if (lm !== '') extra.layerMask = Number(lm);
  if (ex !== '') extra.excludeBodyId = Number(ex);
  command({ type: 'ray_cast', ...base, ...extra });
});
document.getElementById('qfBothBtn').addEventListener('click', () => command({ type: 'ray_cast', ...getRayOriginFromInputs(), allHits: true }));
document.getElementById('qfMovingBtn').addEventListener('click', () => command({ type: 'ray_cast', ...getRayOriginFromInputs(), allHits: true, layerMask: 0b10 }));
document.getElementById('qfStaticBtn').addEventListener('click', () => command({ type: 'ray_cast', ...getRayOriginFromInputs(), allHits: true, layerMask: 0b01 }));

// ─── Skeleton Pose controls ───────────────────────────────────────────────────
document.getElementById('poseToggleBtn').addEventListener('click', () => command({ type: 'pose_toggle_animated' }));
document.getElementById('poseResetBtn').addEventListener('click', () => command({ type: 'pose_reset' }));

// ─── Ragdoll Extended controls ────────────────────────────────────────────────
document.getElementById('rdImpulseBtn').addEventListener('click', () => command({
  type: 'ragdoll_add_impulse',
  x: Number(document.getElementById('rdIX').value),
  y: Number(document.getElementById('rdIY').value),
  z: Number(document.getElementById('rdIZ').value)
}));
document.getElementById('rdVelocityBtn').addEventListener('click', () => command({
  type: 'ragdoll_set_velocity',
  x: Number(document.getElementById('rdVX').value),
  y: Number(document.getElementById('rdVY').value),
  z: Number(document.getElementById('rdVZ').value)
}));
document.getElementById('rdActivateBtn').addEventListener('click', () => command({ type: 'ragdoll_activate' }));
document.getElementById('rdTogglePhysicsBtn').addEventListener('click', () => command({ type: 'ragdoll_toggle_physics' }));
document.getElementById('rdWarmStartBtn').addEventListener('click', () => command({ type: 'ragdoll_reset_warm_start' }));

// ─── Mixamo Erika controls ────────────────────────────────────────────────────
document.getElementById('erikaImpulseBtn').addEventListener('click', () => command({
  type: 'erika_impulse',
  x: Number(document.getElementById('erikaIX').value),
  y: Number(document.getElementById('erikaIY').value),
  z: Number(document.getElementById('erikaIZ').value)
}));
document.getElementById('erikaActivateBtn').addEventListener('click', () => command({ type: 'erika_activate' }));
document.getElementById('erikaResetBtn').addEventListener('click', () => command({ type: 'erika_reset' }));
document.getElementById('erikaToggleModeBtn').addEventListener('click', () => command({ type: 'erika_toggle_mode' }));

// ── Keyboard control (WASD / arrows / Space / R) ──────────────────────────────
// Active only when mixamo-erika example is loaded.
const ERIKA_KEYS = new Set(['KeyW','KeyA','KeyS','KeyD','ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Space']);
const keysDown = new Set();
let erikaKeyTimer = null;
const PUSH_F = 15; // impulse magnitude per axis (Ns per body)

function flushErikaKeys() {
  if (!keysDown.size || state?.exampleId !== 'mixamo-erika') return;
  let x = 0, y = 0, z = 0;
  if (keysDown.has('KeyW')    || keysDown.has('ArrowUp'))    z -= PUSH_F;
  if (keysDown.has('KeyS')    || keysDown.has('ArrowDown'))  z += PUSH_F;
  if (keysDown.has('KeyA')    || keysDown.has('ArrowLeft'))  x -= PUSH_F;
  if (keysDown.has('KeyD')    || keysDown.has('ArrowRight')) x += PUSH_F;
  if (keysDown.has('Space'))                                  y += PUSH_F * 1.8;
  if (x || y || z) command({ type: 'erika_push', x, y, z });
}

window.addEventListener('keydown', e => {
  if (state?.exampleId !== 'mixamo-erika') return;
  if (e.code === 'KeyR') { command({ type: 'erika_toggle_mode' }); return; }
  if (!ERIKA_KEYS.has(e.code)) return;
  e.preventDefault();
  if (!keysDown.has(e.code)) {
    keysDown.add(e.code);
    flushErikaKeys(); // immediate first impulse on press
    if (!erikaKeyTimer) erikaKeyTimer = setInterval(flushErikaKeys, 100);
  }
});

window.addEventListener('keyup', e => {
  keysDown.delete(e.code);
  if (!keysDown.size && erikaKeyTimer) {
    clearInterval(erikaKeyTimer);
    erikaKeyTimer = null;
  }
});

// ─── Boot ─────────────────────────────────────────────────────────────────────
connect();
