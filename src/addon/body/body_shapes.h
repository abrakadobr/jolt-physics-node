#pragma once

#include <iterator>   // std::size
#include <string>
#include "../jolt.h"
#include "../defines.h"

namespace JOLT {


/// This enumerates all shape types, each shape can return its type through Shape::GetSubType
enum class BodyShapeType
{
	// Convex shapes
	Sphere,
	Box,
	Triangle,
	Capsule,
	TaperedCapsule,
	Cylinder,
	ConvexHull,

  Convex,
  Compound,
  Decorated,

	// Compound shapes
	StaticCompound,
	MutableCompound,

	// Decorated shapes
	RotatedTranslated,
	Scaled,
	OffsetCenterOfMass,

	// Other shapes
	Mesh,
	HeightField,
	SoftBody,

	// User defined shapes
	User1,
	User2,
	User3,
	User4,
	User5,
	User6,
	User7,
	User8,

	// User defined convex shapes
	UserConvex1,
	UserConvex2,
	UserConvex3,
	UserConvex4,
	UserConvex5,
	UserConvex6,
	UserConvex7,
	UserConvex8,

	// Other shapes
	Plane,
	TaperedCylinder,
	Empty,
};

// Sets of shape sub types
static constexpr BodyShapeType AllShapeTypes[] = {
  BodyShapeType::Sphere,
  BodyShapeType::Box,
  BodyShapeType::Triangle,
  BodyShapeType::Capsule,
  BodyShapeType::TaperedCapsule,
  BodyShapeType::Cylinder,
  BodyShapeType::ConvexHull,
  BodyShapeType::StaticCompound,
  BodyShapeType::MutableCompound,
  BodyShapeType::RotatedTranslated,
  BodyShapeType::Scaled,
  BodyShapeType::OffsetCenterOfMass,
  BodyShapeType::Mesh,
  BodyShapeType::HeightField,
  BodyShapeType::SoftBody,
  BodyShapeType::User1,
  BodyShapeType::User2,
  BodyShapeType::User3,
  BodyShapeType::User4,
  BodyShapeType::User5,
  BodyShapeType::User6,
  BodyShapeType::User7,
  BodyShapeType::User8,
  BodyShapeType::UserConvex1,
  BodyShapeType::UserConvex2,
  BodyShapeType::UserConvex3,
  BodyShapeType::UserConvex4,
  BodyShapeType::UserConvex5,
  BodyShapeType::UserConvex6,
  BodyShapeType::UserConvex7,
  BodyShapeType::UserConvex8,
  BodyShapeType::Plane,
  BodyShapeType::TaperedCylinder,
  BodyShapeType::Empty
};
static constexpr BodyShapeType ConvexShapeTypes[] = {
  BodyShapeType::Sphere,
  BodyShapeType::Box,
  BodyShapeType::Triangle,
  BodyShapeType::Capsule,
  BodyShapeType::TaperedCapsule,
  BodyShapeType::Cylinder,
  BodyShapeType::ConvexHull,
  BodyShapeType::TaperedCylinder,
  BodyShapeType::UserConvex1,
  BodyShapeType::UserConvex2,
  BodyShapeType::UserConvex3,
  BodyShapeType::UserConvex4,
  BodyShapeType::UserConvex5,
  BodyShapeType::UserConvex6,
  BodyShapeType::UserConvex7,
  BodyShapeType::UserConvex8
};
static constexpr BodyShapeType CompoundShapeTypes[] = {
  BodyShapeType::StaticCompound,
  BodyShapeType::MutableCompound
};
static constexpr BodyShapeType DecoratorShapeTypes[] = {
  BodyShapeType::RotatedTranslated,
  BodyShapeType::Scaled,
  BodyShapeType::OffsetCenterOfMass
};

/// How many shape types we support
static constexpr uint NumShapeTypes = uint(std::size(AllShapeTypes));

/// Names of sub shape types
static constexpr const char *ShapeTypeNames[] = {
  "Sphere",
  "Box",
  "Triangle",
  "Capsule",
  "TaperedCapsule",
  "Cylinder",
  "ConvexHull",
  "StaticCompound",
  "MutableCompound",
  "RotatedTranslated",
  "Scaled",
  "OffsetCenterOfMass",
  "Mesh",
  "HeightField",
  "SoftBody",
  "User1",
  "User2",
  "User3",
  "User4",
  "User5",
  "User6",
  "User7",
  "User8",
  "UserConvex1",
  "UserConvex2",
  "UserConvex3",
  "UserConvex4",
  "UserConvex5",
  "UserConvex6",
  "UserConvex7",
  "UserConvex8",
  "Plane",
  "TaperedCylinder",
  "Empty"
};
static_assert(std::size(ShapeTypeNames) == NumShapeTypes);


struct PhysicsMaterial {
  std::string       name;
  float             friction    = 0.2f;
  float             restitution = 0.0f;
};

struct AbstractShape {
  BodyShapeType     type;
  BodyShapeType     subType;
  uint64_t          userData;
  PhysicsMaterial   material;
  float             convexRadius;
};

struct BoxShape: AbstractShape {
  JPH::Vec3         halfExtent;
};

struct SphereShape: AbstractShape {
  float             radius;
};

struct CapsuleShape: AbstractShape {
  float             inHalfHeight;
  float             inRadius;
};

struct TriangleShape: AbstractShape {
  JPH::Vec3         p1;
  JPH::Vec3         p2;
  JPH::Vec3         p3;
};

struct BodyCreationSettings {
  JPH::Vec3         position = JPH::Vec3::sZero();
  JPH::Quat         rotation = JPH::Quat::sIdentity();
  bool              active = false;
  BodyMotionType    motionType = BodyMotionType::Static;
  std::string       layer;
};

inline BodyShapeType fromJoltShapeType(JPH::EShapeType t) {
  if (t == JPH::EShapeType::Convex) return BodyShapeType::Convex;
  if (t == JPH::EShapeType::Compound) return BodyShapeType::Compound;
  if (t == JPH::EShapeType::Decorated) return BodyShapeType::Decorated;
  if (t == JPH::EShapeType::Mesh) return BodyShapeType::Mesh;
  if (t == JPH::EShapeType::HeightField) return BodyShapeType::HeightField;
  if (t == JPH::EShapeType::SoftBody) return BodyShapeType::SoftBody;
  if (t == JPH::EShapeType::User1) return BodyShapeType::User1;
  if (t == JPH::EShapeType::User2) return BodyShapeType::User2;
  if (t == JPH::EShapeType::User3) return BodyShapeType::User3;
  if (t == JPH::EShapeType::User4) return BodyShapeType::User4;
  if (t == JPH::EShapeType::Plane) return BodyShapeType::Plane;
  return BodyShapeType::Empty;
}

inline BodyShapeType fromJoltShapeSubType(JPH::EShapeSubType t) {
	// Convex shapes
	if (t == JPH::EShapeSubType::Sphere) return BodyShapeType::Sphere;
	if (t == JPH::EShapeSubType::Box) return BodyShapeType::Box;
	if (t == JPH::EShapeSubType::Triangle) return BodyShapeType::Triangle;
	if (t == JPH::EShapeSubType::Capsule) return BodyShapeType::Capsule;
	if (t == JPH::EShapeSubType::TaperedCapsule) return BodyShapeType::TaperedCapsule;
	if (t == JPH::EShapeSubType::Cylinder) return BodyShapeType::Cylinder;
	if (t == JPH::EShapeSubType::ConvexHull) return BodyShapeType::ConvexHull;

	// Compound shapes
	if (t == JPH::EShapeSubType::StaticCompound) return BodyShapeType::StaticCompound;
	if (t == JPH::EShapeSubType::MutableCompound) return BodyShapeType::MutableCompound;

	// Decorated shapes
	if (t == JPH::EShapeSubType::RotatedTranslated) return BodyShapeType::RotatedTranslated;
	if (t == JPH::EShapeSubType::Scaled) return BodyShapeType::Scaled;
	if (t == JPH::EShapeSubType::OffsetCenterOfMass) return BodyShapeType::OffsetCenterOfMass;

	// Other shapes
	if (t == JPH::EShapeSubType::Mesh) return BodyShapeType::Mesh;
	if (t == JPH::EShapeSubType::HeightField) return BodyShapeType::HeightField;
	if (t == JPH::EShapeSubType::SoftBody) return BodyShapeType::SoftBody;

	// User defined shapes
	if (t == JPH::EShapeSubType::User1) return BodyShapeType::User1;
	if (t == JPH::EShapeSubType::User2) return BodyShapeType::User2;
	if (t == JPH::EShapeSubType::User3) return BodyShapeType::User3;
	if (t == JPH::EShapeSubType::User4) return BodyShapeType::User4;
	if (t == JPH::EShapeSubType::User5) return BodyShapeType::User5;
	if (t == JPH::EShapeSubType::User6) return BodyShapeType::User6;
	if (t == JPH::EShapeSubType::User7) return BodyShapeType::User7;
	if (t == JPH::EShapeSubType::User8) return BodyShapeType::User8;

	// User defined convex shapes
	if (t == JPH::EShapeSubType::UserConvex1) return BodyShapeType::UserConvex1;
	if (t == JPH::EShapeSubType::UserConvex2) return BodyShapeType::UserConvex2;
	if (t == JPH::EShapeSubType::UserConvex3) return BodyShapeType::UserConvex3;
	if (t == JPH::EShapeSubType::UserConvex4) return BodyShapeType::UserConvex4;
	if (t == JPH::EShapeSubType::UserConvex5) return BodyShapeType::UserConvex5;
	if (t == JPH::EShapeSubType::UserConvex6) return BodyShapeType::UserConvex6;
	if (t == JPH::EShapeSubType::UserConvex7) return BodyShapeType::UserConvex7;
	if (t == JPH::EShapeSubType::UserConvex8) return BodyShapeType::UserConvex8;

	// Other shapes
	if (t == JPH::EShapeSubType::Plane) return BodyShapeType::Plane;
	if (t == JPH::EShapeSubType::TaperedCylinder) return BodyShapeType::TaperedCylinder;
	return BodyShapeType::Empty;
}

}
