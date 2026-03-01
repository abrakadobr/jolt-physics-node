'use strict';

const { World } = require('..');

const world = new World({ gravity: 9.81 });

// Create a floor box
const floorId = world.createBox({
  position: { x: 0, y: 0, z: 0 },
  halfExtents: { x: 10, y: 0.1, z: 10 },
  dynamic: false,
});

// Create a sphere to hit
const sphereId = world.createSphere({
  position: { x: 0, y: 2, z: 0 },
  radius: 0.5,
  dynamic: false,
});

// Step a bit to settle
for (let i = 0; i < 5; i++) world.step(1 / 60);

// castBoxAll — sweep a small box downward from above
const boxHits = world.castBoxAll({
  origin: { x: 0, y: 5, z: 0 },
  direction: { x: 0, y: -1, z: 0 },
  maxDistance: 10,
  halfExtents: { x: 0.1, y: 0.1, z: 0.1 },
});
console.log('castBoxAll hits:', boxHits.length);
if (boxHits.length === 0) throw new Error('castBoxAll: expected hits');
boxHits.forEach(h => {
  if (typeof h.bodyId !== 'number') throw new Error('castBoxAll: missing bodyId');
  if (typeof h.fraction !== 'number') throw new Error('castBoxAll: missing fraction');
  console.log('  hit bodyId:', h.bodyId, 'fraction:', h.fraction.toFixed(3));
});

// castCapsuleAll — sweep a capsule downward from above
const capsuleHits = world.castCapsuleAll({
  origin: { x: 0, y: 5, z: 0 },
  direction: { x: 0, y: -1, z: 0 },
  maxDistance: 10,
  halfHeight: 0.2,
  radius: 0.1,
});
console.log('castCapsuleAll hits:', capsuleHits.length);
if (capsuleHits.length === 0) throw new Error('castCapsuleAll: expected hits');
capsuleHits.forEach(h => {
  console.log('  hit bodyId:', h.bodyId, 'fraction:', h.fraction.toFixed(3));
});

// castBoxAll with filter — exclude the sphere body
const filteredHits = world.castBoxAll({
  origin: { x: 0, y: 5, z: 0 },
  direction: { x: 0, y: -1, z: 0 },
  maxDistance: 10,
  halfExtents: { x: 0.1, y: 0.1, z: 0.1 },
  filter: { excludeBodyIds: [sphereId] },
});
console.log('castBoxAll filtered hits:', filteredHits.length);
const hasExcluded = filteredHits.some(h => h.bodyId === sphereId);
if (hasExcluded) throw new Error('castBoxAll: excluded body still appeared in results');

console.log('\n✅ cast test passed!');

world.destroy();
