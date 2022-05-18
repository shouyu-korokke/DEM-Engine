//  Copyright (c) 2021, SBEL GPU Development Team
//  Copyright (c) 2021, University of Wisconsin - Madison
//  All rights reserved.

#pragma once

namespace sgps {

typedef uint16_t subVoxelPos_t;  ///< uint16 or uint32

typedef uint64_t voxelID_t;
// TODO: oriQ should be int (mapped to [-1,1]); applyOriQ2Vector3 and hostApplyOriQ2Vector3 need to be changed to make
// that happen
typedef float oriQ_t;
typedef unsigned int bodyID_t;
typedef unsigned int binID_t;
typedef unsigned int triID_t;
typedef uint8_t objID_t;
typedef uint8_t materialsOffset_t;
typedef uint8_t clumpBodyInertiaOffset_t;
typedef uint8_t clumpComponentOffset_t;
typedef unsigned short int clumpComponentOffsetExt_t;  ///< Extended component offset type for non-jitified part
typedef double realFine_t;
typedef char scratch_t;  ///< Data type for DEM scratch-pad array
// typedef unsigned int distinctSphereRelativePositions_default_t;
// typedef unsigned int distinctSphereRadiiOffset_default_t;

// How many bin--sphere touch pairs can there be for one sphere, tops? This type should not need to be large.
typedef unsigned short int binsSphereTouches_t;
// This type needs to be large enough to hold the result of a prefix scan of the type binsSphereTouches_t (and objID_t);
// but normally, it should be the same magnitude as bodyID_t.
typedef unsigned int binSphereTouchPairs_t;
// How many spheres a bin can touch, tops? We can assume it will not be too large to save GPU memory. Note this type
// also doubles as the type for the number of contacts in a bin. NOTE!! Seems uint8_t is not supported by CUB???
typedef unsigned short int spheresBinTouches_t;
// Need to be large enough to hold the number of total contact pairs. In general this number should be in the same
// magnitude as bodyID_t.
typedef unsigned int contactPairs_t;
// How many other entities can a sphere touch, tops?
typedef unsigned short int geoSphereTouches_t;

typedef uint8_t notStupidBool_t;  ///< Ad-hoc bool (array) type
typedef uint8_t contact_t;        ///< Contact type (sphere--sphere is 1, etc.)
typedef uint8_t family_t;         ///< Data type for clump presecription type

typedef uint8_t objType_t;
typedef bool objNormal_t;
}  // namespace sgps
