// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#include "stdafx.h"
#include "Memory.h"

void* operator new(size_t size)
{
	return voxel_allocate(size);
}

void* operator new(std::size_t count, const std::nothrow_t& tag)
{
	return voxel_allocate(count);
}

void* operator new[](std::size_t count, const std::nothrow_t& tag)
{
	return voxel_allocate(count);
}

void* operator new[](size_t size)
{
	return voxel_allocate(size);
}

void operator delete(void* ptr)
{
	voxel_deallocate(ptr);
}

void operator delete(void* ptr, const std::nothrow_t& tag)
{
	voxel_deallocate(ptr);
}

void operator delete[](void* ptr)
{
	voxel_deallocate(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t& tag)
{
	voxel_deallocate(ptr);
}
