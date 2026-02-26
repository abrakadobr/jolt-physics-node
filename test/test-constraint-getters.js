const { World } = require('../index.js');

console.log('Testing constraint state getters...\n');

const world = new World();

// Create two bodies
const bodyA = world.createBox({ position: {x: 0, y: 5, z: 0}, halfExtents: {x: 0.5, y: 0.5, z: 0.5}, dynamic: true });
const bodyB = world.createBox({ position: {x: 0, y: 3, z: 0}, halfExtents: {x: 0.5, y: 0.5, z: 0.5}, dynamic: true });

console.log('Created bodies:', bodyA, bodyB);

// Test Hinge constraint
console.log('\n1. Testing Hinge constraint getters:');
const hingeId = world.createHingeConstraint(bodyA, bodyB, {x: 0, y: 4, z: 0}, {x: 1, y: 0, z: 0}, {x: 0, y: 1, z: 0});
console.log('Created hinge constraint:', hingeId);

world.setHingeLimits(hingeId, -1.5, 1.5);
world.setHingeMotor(hingeId, { state: 1, targetVelocity: 0.5, targetAngle: 0, maxTorque: 100 });

const hingeAngle = world.getHingeAngle(hingeId);
const hingeMotorState = world.getHingeMotorState(hingeId);
console.log('  Hinge angle:', hingeAngle);
console.log('  Hinge motor state:', hingeMotorState, '(1=Velocity)');

// Test Slider constraint
console.log('\n2. Testing Slider constraint getters:');
const bodyC = world.createBox({ position: {x: 3, y: 5, z: 0}, halfExtents: {x: 0.5, y: 0.5, z: 0.5}, dynamic: true });
const bodyD = world.createBox({ position: {x: 3, y: 3, z: 0}, halfExtents: {x: 0.5, y: 0.5, z: 0.5}, dynamic: true });
const sliderId = world.createSliderConstraint(bodyC, bodyD, {x: 3, y: 4, z: 0}, {x: 0, y: 1, z: 0}, {x: 1, y: 0, z: 0}, -2, 2);
console.log('Created slider constraint:', sliderId);

world.setSliderMotor(sliderId, { state: 1, targetVelocity: 0.2, targetPosition: 0, maxForce: 50 });

const sliderPos = world.getSliderPosition(sliderId);
const sliderMotorState = world.getSliderMotorState(sliderId);
console.log('  Slider position:', sliderPos);
console.log('  Slider motor state:', sliderMotorState, '(1=Velocity)');

// Test SixDOF constraint
console.log('\n3. Testing SixDOF constraint getters:');
const bodyE = world.createBox({ position: {x: -3, y: 5, z: 0}, halfExtents: {x: 0.5, y: 0.5, z: 0.5}, dynamic: true });
const bodyF = world.createBox({ position: {x: -3, y: 3, z: 0}, halfExtents: {x: 0.5, y: 0.5, z: 0.5}, dynamic: true });
const sixDofId = world.createSixDOFConstraint(bodyE, bodyF, {x: -3, y: 4, z: 0}, {x: 1, y: 0, z: 0}, {x: 0, y: 1, z: 0});
console.log('Created SixDOF constraint:', sixDofId);

world.setSixDOFLimits(sixDofId, {
  translationMin: {x: -1, y: -1, z: -1},
  translationMax: {x: 1, y: 1, z: 1},
  rotationMin: {x: -0.5, y: -0.5, z: -0.5},
  rotationMax: {x: 0.5, y: 0.5, z: 0.5}
});
world.setSixDOFMotorState(sixDofId, 0, 1); // TranslationX = Velocity mode

const sixDofRotation = world.getSixDOFRotation(sixDofId);
const sixDofLimits = world.getSixDOFLimits(sixDofId);
const sixDofMotorState = world.getSixDOFMotorState(sixDofId, 0);
console.log('  SixDOF rotation:', sixDofRotation);
console.log('  SixDOF limits:', sixDofLimits);
console.log('  SixDOF motor state (axis 0):', sixDofMotorState, '(1=Velocity)');

// Test SwingTwist constraint
console.log('\n4. Testing SwingTwist constraint getters:');
const bodyG = world.createBox({ position: {x: 0, y: 5, z: 3}, halfExtents: {x: 0.5, y: 0.5, z: 0.5}, dynamic: true });
const bodyH = world.createBox({ position: {x: 0, y: 3, z: 3}, halfExtents: {x: 0.5, y: 0.5, z: 0.5}, dynamic: true });
const swingTwistId = world.createSwingTwistConstraint(bodyG, bodyH, {x: 0, y: 4, z: 3}, {x: 0, y: 1, z: 0}, {x: 1, y: 0, z: 0}, {
  normalHalfCone: 0.5,
  planeHalfCone: 0.5,
  twistMin: -0.3,
  twistMax: 0.3
});
console.log('Created SwingTwist constraint:', swingTwistId);

world.setSwingTwistMotor(swingTwistId, {
  swingState: 1,
  twistState: 1,
  targetAngularVelocity: {x: 0, y: 0.1, z: 0},
  targetOrientation: {x: 0, y: 0, z: 0, w: 1},
  maxTorque: 100
});

const swingTwistRotation = world.getSwingTwistRotation(swingTwistId);
const swingTwistMotorState = world.getSwingTwistMotorState(swingTwistId);
console.log('  SwingTwist rotation:', swingTwistRotation);
console.log('  SwingTwist motor state:', swingTwistMotorState);

// Simulate a few steps
console.log('\n5. Simulating a few steps to test runtime state:');
for (let i = 0; i < 5; i++) {
  world.step(1 / 60);
}

console.log('  Hinge angle after simulation:', world.getHingeAngle(hingeId));
console.log('  Slider position after simulation:', world.getSliderPosition(sliderId));

console.log('\n✅ All constraint getter tests passed!');
