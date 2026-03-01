'use strict';

const { World } = require('..');

const world = new World({ gravity: 9.81 });

// Floor
world.createBox({
  position: { x: 0, y: 0, z: 0 },
  halfExtents: { x: 10, y: 0.1, z: 10 },
  dynamic: false,
});

// Create character above floor
const char = world.createCharacter({
  halfHeight: 0.5,
  radius: 0.3,
  position: { x: 0, y: 2, z: 0 },
});

console.log('character id:', char.id);
if (!char.id) throw new Error('invalid character id');

// Step until character lands
for (let i = 0; i < 120; i++) {
  char.update(1 / 60);
  world.step(1 / 60);
}

const groundState = char.getGroundState();
console.log('groundState after landing:', groundState);
if (!char.isOnGround()) throw new Error(`expected onGround, got state ${groundState}`);
console.log('isOnGround:', char.isOnGround());

const pos = char.getPosition();
console.log('position after landing:', pos);
if (pos.y > 1.5) throw new Error(`expected char near floor, got y=${pos.y}`);

// Test setLinearVelocity (jump)
char.setLinearVelocity({ x: 0, y: 6, z: 0 });
// After just one update, character should be in air (or at least rising)
char.update(1 / 60);
world.step(1 / 60);
char.update(1 / 60);
world.step(1 / 60);
char.update(1 / 60);
world.step(1 / 60);

const jumpState = char.getGroundState();
console.log('groundState after jump:', jumpState);
// After a jump up, should be in air (state 3) or not-supported (state 2)
if (char.isOnGround()) throw new Error('expected not on ground after jump');
console.log('isInAir:', char.isInAir());

// Test getLinearVelocity
const vel = char.getLinearVelocity();
console.log('velocity during jump:', vel);

// Test getRotation / setRotation
const rot = char.getRotation();
console.log('rotation:', rot);
if (typeof rot.x !== 'number') throw new Error('getRotation: invalid');
char.setRotation({ x: 0, y: 0, z: 0, w: 1 });

// Test setPosition
char.setPosition({ x: 1, y: 3, z: 0 });
const newPos = char.getPosition();
console.log('position after setPosition:', newPos);
if (Math.abs(newPos.x - 1) > 0.01) throw new Error('setPosition: x mismatch');

// Test getGroundNormal (in air → should return something or null)
const normal = char.getGroundNormal();
console.log('groundNormal:', normal);

// Test getGroundBodyId
const groundBodyId = char.getGroundBodyId();
console.log('groundBodyId:', groundBodyId);

// Test destroy
char.destroy();
try {
  char.update(1 / 60);
  throw new Error('should have thrown after destroy');
} catch (e) {
  if (!e.message.includes('destroyed')) throw e;
  console.log('destroy guard works:', e.message);
}

console.log('\n✅ character test passed!');

world.destroy();
