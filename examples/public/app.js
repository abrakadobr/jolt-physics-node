import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const exampleSelect = document.getElementById('exampleSelect');
const statusEl = document.getElementById('status');
const canvas = document.getElementById('scene');

let state = null;

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
grid.position.y = -1;
scene.add(grid);

const axes = new THREE.AxesHelper(1.4);
axes.position.set(-6.5, -1, -6.5);
scene.add(axes);

const bodyMeshes = new Map();
const constraintLines = new Map();
const ragdollGroups = new Map();

let rayLine = null;
let rayHitSphere = null;
let rayAllHitsGroup = null;
let sphereCastGroup = null;
let sphereCollideGroup = null;

function hexToColor(hex) {
  return new THREE.Color(hex || '#4477aa');
}

function makeHeightFieldGeometry(body) {
  const { samples, sampleCount, cellSize = 1 } = body;
  console.log(`[makeHeightFieldGeometry] N=${sampleCount} samples=${samples?.length} cellSize=${cellSize}`);
  if (!samples || !sampleCount) throw new Error(`makeHeightFieldGeometry: missing samples (${samples?.length}) or sampleCount (${sampleCount})`);
  const N = sampleCount;
  const positions = [];
  const indices = [];

  for (let r = 0; r < N; r++) {
    for (let c = 0; c < N; c++) {
      const x = (c - (N - 1) / 2) * cellSize;
      const y = samples[r * N + c];
      const z = (r - (N - 1) / 2) * cellSize;
      positions.push(x, y, z);
    }
  }
  for (let r = 0; r < N - 1; r++) {
    for (let c = 0; c < N - 1; c++) {
      const a = r * N + c;
      const b = r * N + c + 1;
      const d = (r + 1) * N + c;
      const e = (r + 1) * N + c + 1;
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
    const r = Math.max(sub.radius || 0.15, 0.03);
    const l = Math.max((sub.halfHeight || 0.2) * 2, 0.04);
    geom = new THREE.CapsuleGeometry(r, l, 8, 14);
  } else {
    const he = sub.halfExtents || { x: 0.25, y: 0.25, z: 0.25 };
    geom = new THREE.BoxGeometry(Math.max(he.x * 2, 0.01), Math.max(he.y * 2, 0.01), Math.max(he.z * 2, 0.01));
  }
  const mat = new THREE.MeshStandardMaterial({
    color: hexToColor(sub.color),
    roughness: 0.55,
    metalness: parentDynamic ? 0.1 : 0,
    transparent: !parentDynamic,
    opacity: parentDynamic ? 1 : 0.82
  });
  const mesh = new THREE.Mesh(geom, mat);
  mesh.castShadow = Boolean(parentDynamic);
  mesh.receiveShadow = true;
  if (sub.position) mesh.position.set(sub.position.x, sub.position.y, sub.position.z);
  if (sub.rotation) mesh.quaternion.set(sub.rotation.x, sub.rotation.y, sub.rotation.z, sub.rotation.w);
  return mesh;
}

function makeBodyMesh(body) {
  console.log(`[makeBodyMesh] id=${body.bodyId} kind=${body.kind || '?'} sub=${body.subShapes?.length || 0} smp=${body.samples?.length || 0}`);
  // Height field — custom terrain geometry
  if (body.kind === 'heightfield') {
    const geom = makeHeightFieldGeometry(body);
    const mat = new THREE.MeshStandardMaterial({
      color: hexToColor(body.color || '#5d8a5a'),
      roughness: 0.85,
      metalness: 0,
      side: THREE.DoubleSide
    });
    const mesh = new THREE.Mesh(geom, mat);
    mesh.receiveShadow = true;
    return mesh;
  }

  // Compound body — group of sub-meshes
  if (body.subShapes && body.subShapes.length > 0) {
    console.log(`[makeBodyMesh] compound: ${body.subShapes.length} sub-shapes, dynamic=${body.dynamic}`);
    const group = new THREE.Group();
    for (const sub of body.subShapes) {
      group.add(makeSubMesh(sub, body.dynamic));
    }
    return group;
  }

  let geometry;

  if (body.kind === 'sphere') {
    geometry = new THREE.SphereGeometry(Math.max(body.radius || 0.3, 0.08), 24, 18);
  } else if (body.kind === 'capsule') {
    const radius = Math.max(body.radius || 0.2, 0.05);
    const length = Math.max(body.length || 0.8, 0.05);
    geometry = new THREE.CapsuleGeometry(radius, length, 10, 18);
  } else {
    const hx = Math.max(body.halfExtents?.x || 0.3, 0.05);
    const hy = Math.max(body.halfExtents?.y || 0.3, 0.05);
    const hz = Math.max(body.halfExtents?.z || 0.3, 0.05);
    geometry = new THREE.BoxGeometry(hx * 2, hy * 2, hz * 2);
  }

  const material = new THREE.MeshStandardMaterial({
    color: hexToColor(body.color),
    roughness: 0.55,
    metalness: body.dynamic ? 0.1 : 0,
    transparent: !body.dynamic,
    opacity: body.dynamic ? 1 : 0.8
  });

  const mesh = new THREE.Mesh(geometry, material);
  mesh.castShadow = Boolean(body.dynamic);
  mesh.receiveShadow = true;
  return mesh;
}

function updateBodyMeshes() {
  if (!state) return;
  const seen = new Set();

  for (const body of state.bodies) {
    seen.add(body.bodyId);
    let mesh = bodyMeshes.get(body.bodyId);
    if (!mesh) {
      try {
        mesh = makeBodyMesh(body);
      } catch (err) {
        console.error(`makeBodyMesh failed for body ${body.bodyId} (kind=${body.kind}):`, err);
        continue;
      }
      bodyMeshes.set(body.bodyId, mesh);
      scene.add(mesh);
    }

    mesh.position.set(body.position.x, body.position.y, body.position.z);
    mesh.quaternion.set(body.rotation.x, body.rotation.y, body.rotation.z, body.rotation.w);
  }

  for (const [bodyId, mesh] of bodyMeshes.entries()) {
    if (!seen.has(bodyId)) {
      scene.remove(mesh);
      if (mesh.isGroup) {
        mesh.traverse(obj => { obj.geometry?.dispose(); obj.material?.dispose(); });
      } else {
        mesh.geometry.dispose();
        mesh.material.dispose();
      }
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
    const a = bodyMap.get(c.bodyA);
    const b = bodyMap.get(c.bodyB);
    if (!a || !b) continue;

    let line = constraintLines.get(c.constraintId);
    if (!line) {
      const geom = new THREE.BufferGeometry();
      geom.setAttribute('position', new THREE.Float32BufferAttribute([0, 0, 0, 0, 0, 0], 3));
      const mat = new THREE.LineBasicMaterial({ color: hexToColor(c.color) });
      line = new THREE.Line(geom, mat);
      constraintLines.set(c.constraintId, line);
      scene.add(line);
    }

    const pos = line.geometry.attributes.position.array;
    pos[0] = a.position.x;
    pos[1] = a.position.y;
    pos[2] = a.position.z;
    pos[3] = b.position.x;
    pos[4] = b.position.y;
    pos[5] = b.position.z;
    line.geometry.attributes.position.needsUpdate = true;
  }

  for (const [id, line] of constraintLines.entries()) {
    if (!seen.has(id)) {
      scene.remove(line);
      line.geometry.dispose();
      line.material.dispose();
      constraintLines.delete(id);
    }
  }
}

function updateRagdolls() {
  if (!state) return;
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
      child.geometry?.dispose?.();
      child.material?.dispose?.();
    }

    for (let i = 0; i < rag.bones.length; i += 1) {
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
    if (!seen.has(id)) {
      group.traverse(obj => {
        obj.geometry?.dispose?.();
        obj.material?.dispose?.();
      });
      scene.remove(group);
      ragdollGroups.delete(id);
    }
  }
}

function updateRay() {
  if (rayLine) {
    scene.remove(rayLine);
    rayLine.geometry.dispose();
    rayLine.material.dispose();
    rayLine = null;
  }
  if (rayHitSphere) {
    scene.remove(rayHitSphere);
    rayHitSphere.geometry.dispose();
    rayHitSphere.material.dispose();
    rayHitSphere = null;
  }
  if (rayAllHitsGroup) {
    rayAllHitsGroup.traverse(o => { o.geometry?.dispose?.(); o.material?.dispose?.(); });
    scene.remove(rayAllHitsGroup);
    rayAllHitsGroup = null;
  }

  if (!state?.lastRay) return;

  const ray = state.lastRay;
  const end = {
    x: ray.origin.x + ray.direction.x * ray.maxDistance,
    y: ray.origin.y + ray.direction.y * ray.maxDistance,
    z: ray.origin.z + ray.direction.z * ray.maxDistance
  };

  rayLine = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(ray.origin.x, ray.origin.y, ray.origin.z),
      new THREE.Vector3(end.x, end.y, end.z)
    ]),
    new THREE.LineDashedMaterial({ color: 0xd50000, dashSize: 0.25, gapSize: 0.18 })
  );
  rayLine.computeLineDistances();
  scene.add(rayLine);

  if (ray.allHits && ray.allHits.length > 0) {
    rayAllHitsGroup = new THREE.Group();
    ray.allHits.forEach((hit, i) => {
      const hp = {
        x: ray.origin.x + ray.direction.x * ray.maxDistance * hit.fraction,
        y: ray.origin.y + ray.direction.y * ray.maxDistance * hit.fraction,
        z: ray.origin.z + ray.direction.z * ray.maxDistance * hit.fraction
      };
      const t = i / Math.max(ray.allHits.length - 1, 1);
      const color = new THREE.Color().setHSL(0.08 + t * 0.55, 1, 0.55);
      const marker = new THREE.Mesh(
        new THREE.SphereGeometry(0.14, 14, 10),
        new THREE.MeshBasicMaterial({ color })
      );
      marker.position.set(hp.x, hp.y, hp.z);
      rayAllHitsGroup.add(marker);
    });
    scene.add(rayAllHitsGroup);
  } else if (ray.hit) {
    const hp = {
      x: ray.origin.x + ray.direction.x * ray.maxDistance * ray.hit.fraction,
      y: ray.origin.y + ray.direction.y * ray.maxDistance * ray.hit.fraction,
      z: ray.origin.z + ray.direction.z * ray.maxDistance * ray.hit.fraction
    };
    rayHitSphere = new THREE.Mesh(new THREE.SphereGeometry(0.13, 14, 10), new THREE.MeshBasicMaterial({ color: 0xd50000 }));
    rayHitSphere.position.set(hp.x, hp.y, hp.z);
    scene.add(rayHitSphere);
  }
}

function clearGroup(groupRefName) {
  const current = groupRefName === 'sphereCastGroup' ? sphereCastGroup : sphereCollideGroup;
  if (!current) return;
  current.traverse((obj) => {
    obj.geometry?.dispose?.();
    if (Array.isArray(obj.material)) obj.material.forEach((m) => m.dispose?.());
    else obj.material?.dispose?.();
  });
  scene.remove(current);
  if (groupRefName === 'sphereCastGroup') sphereCastGroup = null;
  else sphereCollideGroup = null;
}

function updateSphereQueries() {
  clearGroup('sphereCastGroup');
  clearGroup('sphereCollideGroup');

  if (state?.lastSphereCast) {
    const q = state.lastSphereCast;
    const group = new THREE.Group();
    sphereCastGroup = group;

    const start = new THREE.Vector3(q.origin.x, q.origin.y, q.origin.z);
    const end = new THREE.Vector3(
      q.origin.x + q.direction.x * q.maxDistance,
      q.origin.y + q.direction.y * q.maxDistance,
      q.origin.z + q.direction.z * q.maxDistance
    );
    const line = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints([start, end]),
      new THREE.LineDashedMaterial({ color: 0x8e24aa, dashSize: 0.2, gapSize: 0.14 })
    );
    line.computeLineDistances();
    group.add(line);

    const startSphere = new THREE.Mesh(
      new THREE.SphereGeometry(q.radius, 18, 12),
      new THREE.MeshBasicMaterial({ color: 0x8e24aa, transparent: true, opacity: 0.18 })
    );
    startSphere.position.copy(start);
    group.add(startSphere);

    for (const hit of q.hits || []) {
      const hp = new THREE.Vector3(
        q.origin.x + q.direction.x * q.maxDistance * hit.fraction,
        q.origin.y + q.direction.y * q.maxDistance * hit.fraction,
        q.origin.z + q.direction.z * q.maxDistance * hit.fraction
      );
      const marker = new THREE.Mesh(
        new THREE.SphereGeometry(0.1, 14, 10),
        new THREE.MeshBasicMaterial({ color: 0xce93d8 })
      );
      marker.position.copy(hp);
      group.add(marker);
    }
    scene.add(group);
  }

  if (state?.lastSphereCollide) {
    const q = state.lastSphereCollide;
    const group = new THREE.Group();
    sphereCollideGroup = group;

    const center = new THREE.Vector3(q.center.x, q.center.y, q.center.z);
    const sphere = new THREE.Mesh(
      new THREE.SphereGeometry(q.radius, 22, 16),
      new THREE.MeshBasicMaterial({ color: 0x00acc1, transparent: true, opacity: 0.16 })
    );
    sphere.position.copy(center);
    group.add(sphere);

    for (const hit of q.hits || []) {
      const marker = new THREE.Mesh(
        new THREE.SphereGeometry(0.09, 14, 10),
        new THREE.MeshBasicMaterial({ color: 0x26c6da })
      );
      marker.position.set(hit.point.x, hit.point.y, hit.point.z);
      group.add(marker);
    }
    scene.add(group);
  }
}

async function api(path, options = {}) {
  const res = await fetch(path, {
    headers: { 'Content-Type': 'application/json' },
    ...options
  });
  const data = await res.json();
  if (!data.ok) throw new Error(data.error || 'API error');
  return data;
}

async function refreshExamples() {
  const data = await api('/api/examples');
  exampleSelect.innerHTML = '';
  for (const example of data.examples) {
    const opt = document.createElement('option');
    opt.value = example.id;
    opt.textContent = example.name;
    exampleSelect.appendChild(opt);
  }
}

function updateStatus() {
  if (!state) return;
  const eventLines = (state.events || []).slice(-8).map((e) => {
    const pair = e.bodyB != null ? `${e.bodyA}/${e.bodyB}` : `${e.bodyA}`;
    return `  [${e.atTick}] ${e.source}:${e.type} ${pair}`;
  });
  const motorHint =
    state.exampleId === 'motors-queries-events'
      ? `motor presets: Off / Velocity / Position (active: ${state.motorPreset || '-'})`
      : 'motor presets work in "Motors + Queries + Events" example';

  const extra = [];
  if (state.filterState) {
    const { layerMask, excludeBodyId, hitCount } = state.filterState;
    extra.push(`filter: layerMask=0b${(layerMask >>> 0).toString(2).padStart(2, '0')} exclude=${excludeBodyId ?? '-'} hits=${hitCount}`);
  }
  if (state.springParams) {
    const { frequency, damping, hingeTarget, sliderTarget } = state.springParams;
    extra.push(`spring: freq=${frequency} damp=${damping} hingeAngle=${hingeTarget} sliderPos=${sliderTarget}`);
  }
  if (state.workerMode) {
    extra.push('physics: Worker Thread');
  }
  if (state.snapshotSavedAt != null) {
    extra.push(`checkpoint: saved at tick ${state.snapshotSavedAt}`);
  }

  statusEl.textContent = [
    `example: ${state.exampleId}`,
    `running: ${state.running}`,
    `tick: ${state.tick}`,
    `bodies: ${state.bodies.length}`,
    `constraints: ${state.constraints.length}`,
    `ragdolls: ${state.ragdolls.length}`,
    `rayAllHits: ${(state.lastRay?.allHits || []).length}`,
    `sphereCastHits: ${(state.lastSphereCast?.hits || []).length}`,
    `sphereCollideHits: ${(state.lastSphereCollide?.hits || []).length}`,
    `${motorHint}`,
    ...extra,
    '',
    'All bodies:',
    ...state.bodies.map(b => `  [${b.bodyId}] ${b.kind || '?'} dyn=${b.dynamic} sub=${b.subShapes?.length || 0} smp=${b.samples?.length || 0} y=${b.position?.y?.toFixed(2) ?? '?'}`),
    '',
    'Recent events:',
    ...(eventLines.length ? eventLines : ['  -'])
  ].join('\n');
}

async function pullState() {
  const data = await api('/api/state');
  state = data.state;
  updateBodyMeshes();
  updateConstraints();
  updateRagdolls();
  updateRay();
  updateSphereQueries();
  updateStatus();
}

function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}

function onResize() {
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  renderer.setSize(w, h, false);
  camera.aspect = w / Math.max(h, 1);
  camera.updateProjectionMatrix();
}

window.addEventListener('resize', onResize);
onResize();
animate();

async function command(payload) {
  await api('/api/command', {
    method: 'POST',
    body: JSON.stringify(payload)
  });
  await pullState();
}

document.getElementById('resetBtn').addEventListener('click', async () => {
  try {
    await api('/api/example', {
      method: 'POST',
      body: JSON.stringify({ exampleId: exampleSelect.value })
    });
    await pullState();
  } catch (err) {
    statusEl.textContent = `Load example error:\n${err.message}`;
    console.error('Load example error:', err);
  }
});

document.getElementById('playBtn').addEventListener('click', () => command({ type: 'play' }));
document.getElementById('pauseBtn').addEventListener('click', () => command({ type: 'pause' }));
document.getElementById('stepBtn').addEventListener('click', () => command({ type: 'step', count: 10 }));

document.getElementById('spawnBtn').addEventListener('click', () => {
  command({
    type: 'spawn_sphere',
    x: Number(document.getElementById('spawnX').value),
    y: Number(document.getElementById('spawnY').value),
    z: 0,
    radius: Number(document.getElementById('spawnR').value)
  });
});

document.getElementById('impulseBtn').addEventListener('click', () => {
  command({
    type: 'apply_impulse',
    bodyId: Number(document.getElementById('bodyId').value),
    x: Number(document.getElementById('impX').value),
    y: Number(document.getElementById('impY').value),
    z: Number(document.getElementById('impZ').value)
  });
});

document.getElementById('rayBtn').addEventListener('click', () => {
  command({
    type: 'ray_cast',
    ox: Number(document.getElementById('rayOX').value),
    oy: Number(document.getElementById('rayOY').value),
    oz: Number(document.getElementById('rayOZ').value),
    dx: Number(document.getElementById('rayDX').value),
    dy: Number(document.getElementById('rayDY').value),
    dz: Number(document.getElementById('rayDZ').value),
    maxDistance: Number(document.getElementById('rayMax').value)
  });
});

document.getElementById('sphereCastBtn').addEventListener('click', () => {
  command({
    type: 'cast_sphere',
    ox: Number(document.getElementById('scOX').value),
    oy: Number(document.getElementById('scOY').value),
    oz: Number(document.getElementById('scOZ').value),
    dx: Number(document.getElementById('scDX').value),
    dy: Number(document.getElementById('scDY').value),
    dz: Number(document.getElementById('scDZ').value),
    maxDistance: Number(document.getElementById('scMax').value),
    radius: Number(document.getElementById('scR').value)
  });
});

document.getElementById('sphereCollideBtn').addEventListener('click', () => {
  command({
    type: 'collide_sphere',
    cx: Number(document.getElementById('coX').value),
    cy: Number(document.getElementById('coY').value),
    cz: Number(document.getElementById('coZ').value),
    radius: Number(document.getElementById('coR').value),
    maxSeparation: Number(document.getElementById('coSep').value)
  });
});

document.getElementById('saveSnapBtn').addEventListener('click', () => {
  command({ type: 'save_snapshot' });
});

document.getElementById('restoreSnapBtn').addEventListener('click', () => {
  command({ type: 'restore_snapshot' });
});

document.getElementById('motorOffBtn').addEventListener('click', () => {
  command({ type: 'set_motor_preset', preset: 'off' });
});

document.getElementById('motorVelBtn').addEventListener('click', () => {
  command({ type: 'set_motor_preset', preset: 'velocity' });
});

document.getElementById('motorPosBtn').addEventListener('click', () => {
  command({ type: 'set_motor_preset', preset: 'position' });
});

function getRayOriginFromInputs() {
  return {
    ox: Number(document.getElementById('rayOX').value),
    oy: Number(document.getElementById('rayOY').value),
    oz: Number(document.getElementById('rayOZ').value),
    dx: Number(document.getElementById('rayDX').value),
    dy: Number(document.getElementById('rayDY').value),
    dz: Number(document.getElementById('rayDZ').value),
    maxDistance: Number(document.getElementById('rayMax').value)
  };
}

document.getElementById('qfAllBtn').addEventListener('click', () => {
  const base = getRayOriginFromInputs();
  const layerMaskVal = document.getElementById('qfLayerMask').value.trim();
  const excludeVal = document.getElementById('qfExclude').value.trim();
  const extra = { allHits: true };
  if (layerMaskVal !== '') extra.layerMask = Number(layerMaskVal);
  if (excludeVal !== '') extra.excludeBodyId = Number(excludeVal);
  command({ type: 'ray_cast', ...base, ...extra });
});

document.getElementById('qfBothBtn').addEventListener('click', () => {
  command({ type: 'ray_cast', ...getRayOriginFromInputs(), allHits: true });
});

document.getElementById('qfMovingBtn').addEventListener('click', () => {
  command({ type: 'ray_cast', ...getRayOriginFromInputs(), allHits: true, layerMask: 0b10 });
});

document.getElementById('qfStaticBtn').addEventListener('click', () => {
  command({ type: 'ray_cast', ...getRayOriginFromInputs(), allHits: true, layerMask: 0b01 });
});

document.getElementById('spApplyBtn').addEventListener('click', () => {
  command({
    type: 'set_spring_params',
    frequency: Number(document.getElementById('spFreq').value),
    damping: Number(document.getElementById('spDamp').value),
    hingeTarget: Number(document.getElementById('spHingeTarget').value),
    sliderTarget: Number(document.getElementById('spSliderTarget').value)
  });
});

(async () => {
  statusEl.textContent = 'Loading scene...';
  await refreshExamples();
  await pullState();

  setInterval(async () => {
    try {
      await pullState();
    } catch (err) {
      statusEl.textContent = `Connection error:\n${err.message}`;
    }
  }, 140);
})();
