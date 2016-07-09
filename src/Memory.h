// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

void* operator new(size_t size);
void* operator new(std::size_t count, const std::nothrow_t& tag);
void* operator new[](std::size_t count, const std::nothrow_t& tag);
void* operator new[](size_t size);
void operator delete(void* ptr);
void operator delete(void* ptr, const std::nothrow_t& tag);
void operator delete[](void* ptr);
void operator delete[](void* ptr, const std::nothrow_t& tag);
