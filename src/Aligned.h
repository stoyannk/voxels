// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

namespace Voxels
{

template<unsigned Alignment>
class Aligned
{
public:
	static void* operator new(size_t size);
	static void operator delete(void* p);
};

template<unsigned Alignment>
void* Aligned<Alignment>::operator new(size_t size)
{
	return voxel_alloc_aligned(size, Alignment);
}

template<unsigned Alignment>
void  Aligned<Alignment>::operator delete(void* p)
{
	voxel_dealloc_aligned(p);
}

}