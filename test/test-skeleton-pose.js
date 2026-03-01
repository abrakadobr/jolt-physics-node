'use strict';

const { World, Skeleton } = require('..');

function assert(cond, msg) {
  if (!cond) throw new Error('FAIL: ' + msg);
}

function approxEq(a, b, eps = 1e-5) {
  return Math.abs(a - b) < eps;
}

const world = new World({ gravity: 9.81 });

// Build a 3-joint skeleton: root → bone1 → bone2
const skeleton = new Skeleton(world);
skeleton.addJoint('root', -1);
skeleton.addJoint('bone1', 0);
skeleton.addJoint('bone2', 1);
assert(skeleton.finalize(), 'finalize');
assert(skeleton.getJointCount() === 3, 'jointCount');

// Build ragdoll settings with default shapes
const settings = skeleton.createRagdollSettings({
  capsuleHalfHeight: 0.2,
  capsuleRadius: 0.1,
  spacing: 0.4,
});

// Set constraints (bone1 swingTwist from root, bone2 hinge from bone1)
settings.setJointConstraint(1, { type: 'swingTwist', normalHalfCone: 0.5, planeHalfCone: 0.3, twistMin: -0.5, twistMax: 0.5 });
settings.setJointConstraint(2, { type: 'hinge', minAngle: -1.0, maxAngle: 1.0 });

// Create ragdoll
const ragdoll = settings.createRagdoll({ activate: true });
assert(ragdoll.bodyCount() === 3, 'bodyCount');

// Test createPose
const pose = ragdoll.createPose();
assert(pose.id > 0, 'pose.id');
assert(pose.getJointCount() === 3, 'pose.getJointCount');

// Test setJoint / getJoint
const t = { x: 0.1, y: 0.2, z: 0.3 };
const r = { x: 0, y: 0, z: 0, w: 1 };
pose.setJoint(1, t, r);
const jt = pose.getJoint(1);
assert(approxEq(jt.translation.x, 0.1), 'joint.translation.x');
assert(approxEq(jt.translation.y, 0.2), 'joint.translation.y');
assert(approxEq(jt.translation.z, 0.3), 'joint.translation.z');
assert(approxEq(jt.rotation.w, 1), 'joint.rotation.w');

// Test setRootOffset / getRootOffset
pose.setRootOffset({ x: 1, y: 2, z: 3 });
const off = pose.getRootOffset();
assert(approxEq(off.x, 1), 'rootOffset.x');
assert(approxEq(off.y, 2), 'rootOffset.y');
assert(approxEq(off.z, 3), 'rootOffset.z');

// Reset offset
pose.setRootOffset({ x: 0, y: 0, z: 0 });

// Test calculateJointMatrices (should not throw)
pose.calculateJointMatrices();

// Test getPose (fill pose from current ragdoll state)
ragdoll.getPose(pose);

// Test setPose (teleport ragdoll to pose)
ragdoll.setPose(pose);

// Test driveToPoseKinematics
ragdoll.driveToPoseKinematics(pose, 1 / 60);

// Step world a few times
for (let i = 0; i < 10; i++) world.step(1 / 60);

// Test activate / isActive
ragdoll.activate();
const active = ragdoll.isActive();
assert(typeof active === 'boolean', 'isActive returns boolean');

// Test getRootTransform
const rt = ragdoll.getRootTransform();
assert(typeof rt.position.x === 'number', 'rootTransform.position.x');
assert(typeof rt.rotation.w === 'number', 'rootTransform.rotation.w');

// Test getWorldSpaceBounds
const bounds = ragdoll.getWorldSpaceBounds();
assert(typeof bounds.min.x === 'number', 'bounds.min.x');
assert(typeof bounds.max.y === 'number', 'bounds.max.y');
assert(bounds.max.y >= bounds.min.y, 'bounds.max.y >= min.y');

// Test resetWarmStart (after setPose)
ragdoll.setPose(pose);
ragdoll.resetWarmStart();

// Test setLinearVelocity
ragdoll.setLinearVelocity({ x: 0, y: 0, z: 0 });

// Test addLinearVelocity
ragdoll.addLinearVelocity({ x: 0, y: 1, z: 0 });

// Test setLinearAndAngularVelocity
ragdoll.setLinearAndAngularVelocity({ x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 0 });

// Test addImpulse
ragdoll.addImpulse({ x: 0, y: 10, z: 0 });

// Test setGroupID
ragdoll.setGroupID(2);

// Test stabilize
const stabilized = ragdoll.stabilize();
assert(typeof stabilized === 'boolean', 'stabilize returns boolean');

// Test removeFromPhysicsSystem / addToPhysicsSystem
ragdoll.removeFromPhysicsSystem();
ragdoll.addToPhysicsSystem(true);

// Destroy pose
pose.destroy();
assert(pose._destroyed, 'pose destroyed');

// Test double-destroy safety
pose.destroy(); // should not throw

// Destroy ragdoll
ragdoll.destroy();

world.destroy();

console.log('test-skeleton-pose: all tests passed');
