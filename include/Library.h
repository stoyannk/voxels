// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

#include "Declarations.h"

namespace Voxels
{

/// Specifies the verbosity of logging messages the library will emit
///
enum LogSeverity
{
	LS_Trace = 0,
	LS_Debug,
	LS_Info,
	LS_Warning,
	LS_Error,
	LS_CriticalError,
};

/// Definition for the logging funstion called by the library
///
typedef void (*LogMessage)(LogSeverity severity, const char* message);

typedef void* (*VoxelsAllocate_f)(size_t size);
typedef void (*VoxelsDeallocate_f)(void* ptr);
typedef void* (*VoxelsAllocateAligned_f)(size_t size, size_t alignment);
typedef void (*VoxelsDeallocateAligned_f)(void* ptr);

/// Collection of allocator functions that will be invoked when the 
/// library needs more memory or wants to free some
struct VoxelsAllocators
{
	VoxelsAllocate_f VoxelsAllocate;
	VoxelsDeallocate_f VoxelsDeallocate;
	VoxelsAllocateAligned_f VoxelsAllocateAligned;
	VoxelsDeallocateAligned_f VoxelsDeallocateAligned;
};

enum InitError
{
	IE_Ok = 0,
	IE_VersionMismatch
};

}

/// Initializes the library
/// @param the version of the library. Must be VOXELS_VERSION. This parameter makes sure the headers correspond
/// to the built library used
/// @param logger Function used for logging messages - can be nullptr (no logging will happen then)
/// @param allocators Collection of allocation/deallocation functions used by the library
/// if nullptr than the default internal allocator will be used
/// @return If the library was successfully initialized
extern "C" VOXELS_API Voxels::InitError VOXELS_CDECL InitializeVoxels(int version, Voxels::LogMessage logger, Voxels::VoxelsAllocators* allocators);

/// Deinitializes the library
///
extern "C" VOXELS_API void VOXELS_CDECL DeinitializeVoxels();

/// Returns the build version of the library
///
extern "C" VOXELS_API unsigned VOXELS_CDECL GetBuildVersion();