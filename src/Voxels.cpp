// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#include "stdafx.h"

#include "../include/Version.h"
#include "../include/Library.h"
#include "Logger.h"

void* DefaultAllocate(size_t size)
{
	return malloc(size);
}

void DefaultDeallocate(void* ptr)
{
	free(ptr);
}

void* DefaultAllocateAligned(size_t size, size_t alignment)
{
	return _aligned_malloc(size, alignment);
}

void DefaultDeallocateAligned(void* ptr)
{
	_aligned_free(ptr);
}

Voxels::VoxelsAllocate_f voxel_allocate = &DefaultAllocate;
Voxels::VoxelsDeallocate_f voxel_deallocate = &DefaultDeallocate;
Voxels::VoxelsAllocateAligned_f voxel_alloc_aligned = &DefaultAllocateAligned;
Voxels::VoxelsDeallocateAligned_f voxel_dealloc_aligned = &DefaultDeallocateAligned;

Voxels::InitError InitializeVoxels(int version, Voxels::LogMessage logger, Voxels::VoxelsAllocators* allocators)
{
	// allow the patch and build version to differ
	if ((VOXELS_VERSION & 0xFFFF) != (version & 0xFFFF)) {
		return Voxels::IE_VersionMismatch;
	}

	if (allocators)
	{
		voxel_allocate = allocators->VoxelsAllocate;
		voxel_deallocate = allocators->VoxelsDeallocate;
		voxel_alloc_aligned = allocators->VoxelsAllocateAligned;
		voxel_dealloc_aligned = allocators->VoxelsDeallocateAligned;
	}

	Voxels::Logger::Create(logger);
	char buffer[VOXELS_LOG_SIZE];
	snprintf(buffer, VOXELS_LOG_SIZE, "Voxels library initialized - ver. %#010x", VOXELS_VERSION);
	VOXLOG(LS_Info, buffer);
	#ifdef GRID_LIMIT
	snprintf(buffer, VOXELS_LOG_SIZE, "LIMITED Grid size to %d x %d x %d", GRID_LIMIT, GRID_LIMIT, GRID_LIMIT);
	VOXLOG(LS_Info, buffer);
	#endif

	return Voxels::IE_Ok;
}

void DeinitializeVoxels()
{
	VOXLOG(LS_Info, "Voxels library deinitialized");
	Voxels::Logger::Destroy();

	voxel_allocate = &DefaultAllocate;
	voxel_deallocate = &DefaultDeallocate;
	voxel_alloc_aligned = &DefaultAllocateAligned;
	voxel_dealloc_aligned = &DefaultDeallocateAligned;
}

unsigned GetBuildVersion()
{
	return VOXELS_VERSION;
}

namespace Voxels
{

Logger* Logger::s_Logger = nullptr;

}
