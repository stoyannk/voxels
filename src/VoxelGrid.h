// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

#include "../include/Grid.h"

namespace Voxels
{
class VoxelSurface;

class VoxelGrid
{
public:
	static const MaterialId EMPTY_MATERIAL = 255;

	// Z is up!
	VoxelGrid(unsigned w, unsigned d, unsigned h,
		float startX, float startY, float startZ, float step,
		VoxelSurface* surface);
	VoxelGrid(unsigned w, unsigned d, unsigned h);
	VoxelGrid(unsigned w, const char* heightmap);

	static VoxelGrid* Load(const char* data);
	Grid::PackedGrid* PackForSave() const;
	
	unsigned GetWidth () const { return m_Width;}
	unsigned GetDepth () const { return m_Depth;}
	unsigned GetHeight() const { return m_Height;}

	unsigned GridId(unsigned x, unsigned y, unsigned z) const { return z*m_Width*m_Depth + y*m_Width + x; }

	unsigned VoxelIdInBlock(unsigned x, unsigned y, unsigned z) const {
		return z*BLOCK_EXTENTS*BLOCK_EXTENTS + y*BLOCK_EXTENTS + x;
	}

	std::pair<glm::vec3, glm::vec3> InjectSurface(
												const glm::vec3& position,
												const glm::vec3& extents,
												VoxelSurface* surface,
												InjectionType type);

	std::pair<glm::vec3, glm::vec3> InjectMaterial(
												const glm::vec3& position,
												const glm::vec3& extents,
												MaterialId material,
												bool addSubtractBlend);

	void GetBlockData(const glm::vec3& blockCoords, char* output) const;
	void GetMaterialBlockData(const glm::vec3& blockCoords, unsigned char* materialOutput, unsigned char* blendOutput) const;
	bool IsBlockEmpty(const glm::vec3& blockCoords) const;

	inline unsigned CalculateInternalBlockId(const glm::vec3& blockCoords) const;

	inline glm::vec3 GetBlocksCount() const;

	void ModifyBlockDistanceData(const glm::vec3& coords, const char* distances);
	void ModifyBlockMaterialData(const glm::vec3& coords, const MaterialId* materials, const BlendFactor* blends);

	inline size_t MemoryForGrid() const;

	static const unsigned BLOCK_EXTENTS = 16u;

private:
	typedef char value_type;
	typedef std::shared_ptr<value_type> GridPtr;
	typedef std::shared_ptr<MaterialId> MaterialGridPtr;
	typedef std::shared_ptr<BlendFactor> BlendGridPtr;

	enum BlockFlags
	{
		BF_None = 0,
		BF_Empty = 1 << 0,
		BF_DistanceUncompressed = 1 << 1,
		BF_MaterialUncompressed = 1 << 2,
		BF_BlendUncompressed = 1 << 3,

		BF_ForceSize = 0xFFFFFFFF
	};

	struct Block
	{
		Block()
			: InternalId(0)
			, Flags(BF_None)
		{}

		Block(Block&& rhs)
			: InternalId(rhs.InternalId)
			, Flags(rhs.Flags)
			, DistanceData(std::move(rhs.DistanceData))
			, MaterialData(std::move(rhs.MaterialData))
			, BlendData(std::move(rhs.BlendData))
		{}

		unsigned InternalId;
		unsigned Flags;
		std::vector<char> DistanceData;
		std::vector<unsigned char> MaterialData;
		std::vector<unsigned char> BlendData;
	};
	std::vector<Block> m_Blocks;

	unsigned m_Width;
	unsigned m_Depth;
	unsigned m_Height;

	size_t m_MemoryForBlocks;

	template<typename Type>
	static bool CompressBlock(const Type* data, std::vector<Type>& compressed, bool* const isEmpty = nullptr);

	template<typename Type>
	static void DecompressBlock(const Type* data, unsigned sz, bool isUncompressed, Type* output);

	typedef std::pair<glm::vec3, glm::vec3> BlockExtents;
	typedef std::pair<unsigned, BlockExtents> TouchedBlock;
	void IdentifyTouchedBlocks(const glm::vec3& position, const glm::vec3& extents, std::vector<TouchedBlock>& touchedBlocks);

	static void CalculateTouchedBlockSection(const glm::vec3& position,
									  const glm::vec3& extents,
									  const BlockExtents& blockExt,
									  glm::vec3& start,
									  glm::vec3& end);
	
	void RecalculateMemoryUsage();

	void PushBlock(unsigned& blockId,
		const std::vector<char>& blockData,
		const std::vector<unsigned char>& materialData,
		const std::vector<unsigned char>& blendData);

	VoxelGrid(const VoxelGrid&);
	VoxelGrid& operator=(const VoxelGrid&);

	static const unsigned CURRENT_FILE_VER = 1;
};

unsigned VoxelGrid::CalculateInternalBlockId(const glm::vec3& blockCoords) const
{
	return unsigned(blockCoords.x
				+ blockCoords.y * (GetWidth() / BLOCK_EXTENTS)
				+ blockCoords.z * (GetWidth() / BLOCK_EXTENTS) * (GetHeight() / BLOCK_EXTENTS));
}

glm::vec3 VoxelGrid::GetBlocksCount() const
{
	return glm::vec3(m_Width / BLOCK_EXTENTS,
							 m_Depth / BLOCK_EXTENTS,
							 m_Height / BLOCK_EXTENTS);
}

size_t VoxelGrid::MemoryForGrid() const
{
	return m_MemoryForBlocks;
}

}