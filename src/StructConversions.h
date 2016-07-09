// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

#include "../include/Structs.h"

namespace Voxels
{

inline glm::vec3 tovec3(const float3& fl)
{
	return glm::vec3(fl.x, fl.y, fl.z);
}

inline float3 tofloat3(const glm::vec3& fl)
{
	return float3(fl.x, fl.y, fl.z);
}

inline glm::vec4 tovec4(const float4& fl)
{
	return glm::vec4(fl.x, fl.y, fl.z, fl.w);
}

inline float4 tofloat4(const glm::vec4& fl)
{
	return float4(fl.x, fl.y, fl.z, fl.w);
}


}
