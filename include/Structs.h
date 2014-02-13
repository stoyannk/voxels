// Copyright (c) 2013-2014, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

namespace Voxels
{

/// A simple struct that represents a float triplet
///
struct VOXELS_API float3
{
	float x;
	float y;
	float z;

	float3() {};
	float3(float x_, float y_, float z_)
		: x(x_), y(y_), z(z_)
	{}
};

/// A simple struct that represents a float quadruplet
///
struct VOXELS_API float4
{
	float x;
	float y;
	float z;
	float w;

	float4() {};
	float4(float x_, float y_, float z_, float w_)
		: x(x_), y(y_), z(z_), w(w_)
	{}
};

/// A simple struct that represents a pair of float triplets
///
struct VOXELS_API float3pair
{
	float3 first;
	float3 second;
};

}