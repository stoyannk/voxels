// Copyright (c) 2013-2014, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

#include "Declarations.h"

namespace Voxels
{

/// Implements a material map - a mapping between the Voxel
/// grid materis ids and real texture ids later used for rendering
class VOXELS_API MaterialMap
{
public:
	/// A material for triplanar projection
	/// There are 2 sets of 3 textures that should be mixed depending on
	/// the blending parameter during shading.
	struct Material
	{
		unsigned char DiffuseIds0[3]; /* z, xy, -z*/
		unsigned char DiffuseIds1[3];
	};

	virtual ~MaterialMap() {};

	/// Provides the material mapping for a specific material id
	/// @param id the material id to map
	/// @return a maping to texture ids
	virtual Material* GetMaterial(unsigned char id) const = 0;
};

}