'use strict';

const { World } = require('../index');

const world = new World({ gravity: 9.81 });
const activationEvents = [];
const contactEvents = [];
world.onBodyActivation((ev) => activationEvents.push(ev));
world.onContact((ev) => contactEvents.push(ev));

const ballId = world.createSphere({
  radius: 0.5,
  position: { x: 0, y: 5, z: 0 },
  dynamic: true,
  restitution: 0.2,
  friction: 0.5
});

const before = world.getBodyPosition(ballId);
for (let i = 0; i < 120; i += 1) world.step(1 / 60);
const after = world.getBodyPosition(ballId);

console.log('before:', before);
console.log('after:', after);
if (!(after.y < before.y)) throw new Error('Expected sphere to move down due to gravity');

const hit = world.rayCastClosest({
  origin: { x: 0, y: 10, z: 0 },
  direction: { x: 0, y: -1, z: 0 },
  maxDistance: 30
});
if (!hit) throw new Error('Expected ray cast hit');
console.log('ray hit:', hit);

const hits = world.rayCastAll({
  origin: { x: 0, y: 10, z: 0 },
  direction: { x: 0, y: -1, z: 0 },
  maxDistance: 30
});
if (!Array.isArray(hits) || hits.length < 1) throw new Error('Expected rayCastAll hits');

const broadHits = world.queryAABB({
  min: { x: -5, y: -5, z: -5 },
  max: { x: 5, y: 10, z: 5 }
});
if (!Array.isArray(broadHits) || broadHits.length < 1) throw new Error('Expected queryAABB hits');

const sphereOverlapHits = world.collideSphereAll({
  center: { x: 0, y: 0.2, z: 0 },
  radius: 0.5,
  maxSeparation: 0.1
});
if (!Array.isArray(sphereOverlapHits) || sphereOverlapHits.length < 1) throw new Error('Expected collideSphereAll hits');

const sphereCastHits = world.castSphereAll({
  origin: { x: 0, y: 4, z: 0 },
  direction: { x: 0, y: -1, z: 0 },
  maxDistance: 10,
  radius: 0.2
});
if (!Array.isArray(sphereCastHits) || sphereCastHits.length < 1) throw new Error('Expected castSphereAll hits');

const boxA = world.createBox({
  halfExtents: { x: 0.5, y: 0.5, z: 0.5 },
  position: { x: 2, y: 2, z: 0 },
  dynamic: true
});
const boxB = world.createBox({
  halfExtents: { x: 0.5, y: 0.5, z: 0.5 },
  position: { x: 2, y: 3.5, z: 0 },
  dynamic: true
});

const fixedId = world.createFixedConstraint(boxA, boxB);
if (!fixedId) throw new Error('Failed to create fixed constraint');

world.setAngularVelocity(boxA, { x: 0, y: 1, z: 0 });
const av = world.getAngularVelocity(boxA);
if (typeof av.y !== 'number') throw new Error('getAngularVelocity failed');

world.setFriction(boxA, 0.33);
if (Math.abs(world.getFriction(boxA) - 0.33) > 1e-5) throw new Error('getFriction mismatch');
world.setRestitution(boxA, 0.12);
if (Math.abs(world.getRestitution(boxA) - 0.12) > 1e-5) throw new Error('getRestitution mismatch');
world.setGravityFactor(boxA, 0.9);
if (Math.abs(world.getGravityFactor(boxA) - 0.9) > 1e-5) throw new Error('getGravityFactor mismatch');
world.setMotionType(boxA, 2, true);
if (world.getMotionType(boxA) !== 2) throw new Error('getMotionType mismatch');
world.setMotionQuality(boxA, 1);
if (world.getMotionQuality(boxA) !== 1) throw new Error('getMotionQuality mismatch');
world.setObjectLayer(boxA, 1);
if (world.getObjectLayer(boxA) !== 1) throw new Error('getObjectLayer mismatch');
world.setDamping(boxA, { linear: 0.07, angular: 0.08 });
const damping = world.getDamping(boxA);
if (Math.abs(damping.linear - 0.07) > 1e-5 || Math.abs(damping.angular - 0.08) > 1e-5) throw new Error('getDamping mismatch');
if (!world.hasBody(boxA)) throw new Error('hasBody failed');
if (!world.isBodyActive(boxA)) throw new Error('isBodyActive failed');
const com = world.getCenterOfMassPosition(boxA);
if (typeof com.x !== 'number' || typeof com.y !== 'number' || typeof com.z !== 'number') throw new Error('getCenterOfMassPosition failed');
world.setBodySensor(boxA, true);
if (!world.isBodySensor(boxA)) throw new Error('isBodySensor true mismatch');
world.setBodySensor(boxA, false);
if (world.isBodySensor(boxA)) throw new Error('isBodySensor false mismatch');

const taperedCapsule = world.createTaperedCapsule({
  halfHeight: 0.25,
  topRadius: 0.15,
  bottomRadius: 0.08,
  position: { x: -1, y: 3, z: 0 },
  dynamic: true
});
const taperedCylinder = world.createTaperedCylinder({
  halfHeight: 0.35,
  topRadius: 0.18,
  bottomRadius: 0.09,
  position: { x: -2, y: 3, z: 0 },
  dynamic: true
});
if (!world.hasBody(taperedCapsule) || !world.hasBody(taperedCylinder)) throw new Error('tapered shapes creation failed');

world.step(1 / 60);
world.removeConstraint(fixedId);

const pointConstraintId = world.createPointConstraint(boxA, boxB, { x: 2, y: 2.8, z: 0 });
if (!pointConstraintId) throw new Error('Failed to create point constraint');

const coneConstraintId = world.createConeConstraint(boxA, boxB, { x: 2, y: 2.8, z: 0 }, { x: 0, y: 1, z: 0 }, 0.7);
if (!coneConstraintId) throw new Error('Failed to create cone constraint');
world.setConeHalfAngle(coneConstraintId, 0.6);

const swingConstraintId = world.createSwingTwistConstraint(
  boxA,
  boxB,
  { x: 2, y: 2.8, z: 0 },
  { x: 1, y: 0, z: 0 },
  { x: 0, y: 1, z: 0 },
  { normalHalfCone: 0.8, planeHalfCone: 0.8, twistMin: -0.5, twistMax: 0.5 }
);
if (!swingConstraintId) throw new Error('Failed to create swing twist constraint');
world.setSwingTwistLimits(swingConstraintId, { normalHalfCone: 0.6, planeHalfCone: 0.7, twistMin: -0.4, twistMax: 0.4 });
world.setSwingTwistMotor(swingConstraintId, {
  swingState: 1,
  twistState: 1,
  targetAngularVelocity: { x: 0, y: 0.2, z: 0 },
  targetOrientation: { x: 0, y: 0, z: 0, w: 1 },
  maxTorque: 10
});

const sixDofId = world.createSixDOFConstraint(boxA, boxB, { x: 2, y: 2.8, z: 0 }, { x: 1, y: 0, z: 0 }, { x: 0, y: 1, z: 0 });
if (!sixDofId) throw new Error('Failed to create six dof constraint');
world.setSixDOFLimits(sixDofId, {
  translationMin: { x: -0.2, y: -0.2, z: -0.2 },
  translationMax: { x: 0.2, y: 0.2, z: 0.2 },
  rotationMin: { x: -0.4, y: -0.4, z: -0.4 },
  rotationMax: { x: 0.4, y: 0.4, z: 0.4 }
});
world.setSixDOFMotorState(sixDofId, 0, 1);
world.setSixDOFTargetVelocity(sixDofId, { x: 0, y: 0, z: 0 }, { x: 0, y: 0.1, z: 0 });
world.setSixDOFTargetPose(sixDofId, { x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 0, w: 1 });

const hingeBase = world.createBox({ halfExtents: { x: 0.2, y: 0.2, z: 0.2 }, position: { x: 4, y: 3, z: 0 }, dynamic: false });
const hingeBody = world.createBox({ halfExtents: { x: 0.2, y: 0.8, z: 0.2 }, position: { x: 4, y: 2, z: 0 }, dynamic: true });
const hingeId = world.createHingeConstraint(hingeBase, hingeBody, { x: 4, y: 2.6, z: 0 }, { x: 0, y: 0, z: 1 }, { x: 1, y: 0, z: 0 });
if (!hingeId) throw new Error('Failed to create hinge constraint');
world.setHingeLimits(hingeId, -0.6, 0.6);
world.setHingeMotor(hingeId, { state: 1, targetVelocity: 0.2, targetAngle: 0.1, maxTorque: 10 });

const sliderBase = world.createBox({ halfExtents: { x: 0.2, y: 0.2, z: 0.2 }, position: { x: 6, y: 2, z: 0 }, dynamic: false });
const sliderBody = world.createBox({ halfExtents: { x: 0.3, y: 0.3, z: 0.3 }, position: { x: 6, y: 2.6, z: 0 }, dynamic: true });
const sliderId = world.createSliderConstraint(
  sliderBase,
  sliderBody,
  { x: 6, y: 2.3, z: 0 },
  { x: 0, y: 1, z: 0 },
  { x: 1, y: 0, z: 0 },
  -0.5,
  0.5
);
if (!sliderId) throw new Error('Failed to create slider constraint');
world.setSliderLimits(sliderId, -0.4, 0.4);
world.setSliderMotor(sliderId, { state: 1, targetVelocity: 0.2, targetPosition: 0.1, maxForce: 20 });

world.step(1 / 60);
if (activationEvents.length < 1) throw new Error('Expected activation events');
if (contactEvents.length < 1) throw new Error('Expected contact events');

const skeleton = world.createSkeleton();
skeleton.addJoint('root', -1);
skeleton.addJoint('spine', 0);
if (!skeleton.finalize()) throw new Error('Failed to finalize skeleton');
const rootIdx = skeleton.getJointIndex('root');
if (rootIdx !== 0) throw new Error(`Expected root joint index=0 got ${rootIdx}`);
const spineInfo = skeleton.getJointInfo(1);
if (spineInfo.name !== 'spine' || spineInfo.parentIndex !== 0) throw new Error('getJointInfo mismatch');

const ragdollSettings = skeleton.createRagdollSettings({
  capsuleHalfHeight: 0.2,
  capsuleRadius: 0.1,
  spacing: 0.4
});
const ragdoll = ragdollSettings.createRagdoll({ collisionGroup: 777, activate: true });

const boneCount = ragdoll.bodyCount();
if (boneCount !== 2) throw new Error(`Expected ragdoll bodyCount=2 got ${boneCount}`);
const boneBodyId0 = ragdoll.getBoneBodyId(0);
if (typeof boneBodyId0 !== 'number') throw new Error('getBoneBodyId failed');

const bone0 = ragdoll.getBoneTransform(0);
console.log('ragdoll bone0:', bone0);
ragdoll.setBoneTransform(0, {
  position: { x: bone0.position.x, y: bone0.position.y + 0.2, z: bone0.position.z },
  rotation: bone0.rotation
});

const pose = ragdoll.syncToSkeletonPose();
ragdoll.syncFromSkeletonPose(pose);

// Verify that invalid IDs throw
let threw = false;
try { world.setFriction(999999, 0.5); } catch (e) { threw = true; }
if (!threw) throw new Error('Expected throw for invalid body ID in setter');

threw = false;
try { world.getHingeAngle(999999); } catch (e) { threw = true; }
if (!threw) throw new Error('Expected throw for invalid constraint ID in getter');

ragdoll.destroy();
world.removeBody(boxA);
world.removeBody(boxB);
world.removeBody(ballId);
world.removeBody(hingeBase);
world.removeBody(hingeBody);
world.removeBody(sliderBase);
world.removeBody(sliderBody);
world.removeBody(taperedCapsule);
world.removeBody(taperedCylinder);
world.destroy();

console.log('smoke test passed');
