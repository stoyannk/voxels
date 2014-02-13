// Copyright (c) 2013-2014, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

namespace Voxels
{

/// Interface for implementing a surface that can be voxelized in a 
/// Voxel grid. Wgile the grid is being generated it will call the 
/// GetSurface method that must feed it the distance between the surface and the 
/// points in space it's called for.
class VOXELS_API VoxelSurface
{
public:
	virtual ~VoxelSurface() {}

	/// Called when generating a Voxel grid for this surface.
	/// You must provide linear distances to the supplied points.
	/// @param xStart starting coordinate on X axis
	/// @param xEnd ending coordinate on X axis
	/// @param xStep setp on the X axis
	/// @param yStart starting coordinate on Y axis
	/// @param yEnd ending coordinate on Y axis
	/// @param yStep setp on the Y axis
	/// @param zStart starting coordinate on Z axis
	/// @param zEnd ending coordinate on Z axis
	/// @param zStep setp on the Z axis
	/// @param output the distance values you need to fill.
	/// @param materialid the material id values you need to fill. 
	/// Can be nullptr - in that case ignore the parameter.
	/// @param blend the blend values you need to fill.
	/// Can be nullptr - in that case ignore the parameter.
	/// values should be filled on the X, Y and Z axis in that order
	virtual void GetSurface(float xStart, float xEnd, float xStep,
								float yStart, float yEnd, float yStep,
								float zStart, float zEnd, float zStep,
								float* output,
								unsigned char* materialid,
								unsigned char* blend) = 0;
};

}