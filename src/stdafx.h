// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <utility>
#include <memory>
#include <vector>
#include <string>
#include <algorithm> 

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#define VOXELS_LOG_SIZE 512

#include <glm/glm.hpp>

#include <profi_decls.h>
#include <profi.h>
#include "../include/Declarations.h"
#include "../include/Library.h"

extern Voxels::VoxelsAllocate_f voxel_allocate;
extern Voxels::VoxelsDeallocate_f voxel_deallocate;
extern Voxels::VoxelsAllocateAligned_f voxel_alloc_aligned;
extern Voxels::VoxelsDeallocateAligned_f voxel_dealloc_aligned;

#include "Memory.h"

#include "Logger.h"
