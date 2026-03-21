#pragma once

#include <iterator>   // std::size

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


}
