// Copyright (c) 2012-2013, Stoyan Nikolov
// All rights reserved.
// This software is governed by a permissive BSD-style license. See LICENSE.
#pragma once

template<typename T, unsigned Alignment>
class StdAllocatorAligned {
public : 
    typedef T value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

public : 
    template<typename U>
    struct rebind {
		typedef StdAllocatorAligned<U, Alignment> other;
    };

public : 
	inline StdAllocatorAligned(){}
	inline ~StdAllocatorAligned() {}
	inline StdAllocatorAligned(const StdAllocatorAligned& rhs) {}

    template<typename U>
	inline explicit StdAllocatorAligned(const StdAllocatorAligned<U, Alignment>& rhs){}

    inline pointer address(reference r) { return &r; }
    inline const_pointer address(const_reference r) { return &r; }

    inline pointer allocate(size_type cnt, typename std::allocator<void>::const_pointer = 0) 
	{ 
		return reinterpret_cast<pointer>(voxel_alloc_aligned(cnt * sizeof (T), Alignment));
    }
    inline void deallocate(pointer p, size_type) 
	{
		voxel_dealloc_aligned(p);
    }

    inline size_type max_size() const 
	{ 
        return std::numeric_limits<size_type>::max() / sizeof(T);
	}

    inline void construct(pointer p, const T& t) { new(p) T(t); }
    inline void destroy(pointer p) { p->~T(); }

	inline bool operator==(StdAllocatorAligned const&) { return true; }
	inline bool operator!=(StdAllocatorAligned const& a) { return !operator==(a); }

private:
	template<typename U, unsigned A> friend class StdAllocatorAligned;
};
