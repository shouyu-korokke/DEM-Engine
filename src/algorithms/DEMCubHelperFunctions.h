//  Copyright (c) 2021, SBEL GPU Development Team
//  Copyright (c) 2021, University of Wisconsin - Madison
//  All rights reserved.

#pragma once

#include <granular/DataStructs.h>
#include <granular/GranularStructs.h>
#include <granular/GranularDefines.h>
#include <core/utils/GpuManager.h>

namespace sgps {

void cubPrefixScan(sgps::binsSphereTouches_t* d_in,
                   sgps::binsSphereTouchesScan_t* d_out,
                   size_t n,
                   GpuManager::StreamInfo& streamInfo,
                   sgps::DEMSolverStateData& scratchPad);

void cubSortByKeys(sgps::binID_t* d_keys,
                   sgps::bodyID_t* d_vals,
                   size_t n,
                   GpuManager::StreamInfo& streamInfo,
                   sgps::DEMSolverStateData& scratchPad);

void cubCollectForces(sgps::clumpBodyInertiaOffset_t* inertiaPropOffsets,
                      sgps::bodyID_t* idA,
                      sgps::bodyID_t* idB,
                      float3* contactForces,
                      float* clump_h2aX,
                      float* clump_h2aY,
                      float* clump_h2aZ,
                      sgps::bodyID_t* ownerClumpBody,
                      float* massClumpBody,
                      double h,
                      size_t n,
                      double l,
                      GpuManager::StreamInfo& streamInfo,
                      sgps::DEMSolverStateData& scratchPad);

}  // namespace sgps
