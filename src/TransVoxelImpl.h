// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once
#include "VoxelGrid.h"

#include "..\include\Polygonizer.h"

namespace Voxels
{

class MaterialMap;

struct MaterialInfo
{
	MaterialInfo()
	{}

	MaterialInfo(MaterialId id, BlendFactor blend)
		: Id(id)
		, Blend(blend)
	{}

	MaterialId Id;
	BlendFactor Blend;
};

typedef std::vector<PolygonVertex> VerticesVec;
typedef std::vector<VerticesVec> TransitionVerticesVec;
typedef std::vector<unsigned> IndicesVec;
typedef std::vector<IndicesVec> TransitionIndicesVec;

struct PolygonBlock : public BlockPolygons
{
	PolygonBlock(unsigned id
				, const float3& min
				, const float3& max);
	PolygonBlock(PolygonBlock&& block);
	
	PolygonBlock& operator=(PolygonBlock&& block);
	
	unsigned Id;

	VerticesVec Vertices;
	IndicesVec Indices;
	TransitionVerticesVec TransitionVertices;
	TransitionIndicesVec TransitionIndices;

	float3 MinimalCorner;
	float3 MaximalCorner;

	virtual unsigned GetId() const override;
	virtual const PolygonVertex* GetVertices(unsigned* count) const override;
	virtual const unsigned* GetIndices(unsigned* count) const override;
	virtual const PolygonVertex* GetTransitionVertices(TransitionFaceId face, unsigned* count) const override;
	virtual const unsigned* GetTransitionIndices(TransitionFaceId face, unsigned* count) const override;

	virtual float3 GetMinimalCorner() const override;
	virtual float3 GetMaximalCorner() const override;
};

typedef std::vector<PolygonBlock> PolygonBlocksVec;

struct PolygonMap : public PolygonSurface
{
	PolygonMap();
	PolygonMap(PolygonMap&& lhs);

	struct LodLevel
	{
		LodLevel();
		LodLevel(LodLevel&& l);
		PolygonBlocksVec Blocks;
	};

	typedef std::vector<LodLevel> LodLevels;
	LodLevels Levels;

	float3 Extents; // width, height, depth

	struct MaterialCache
	{
		MaterialCache();
		MaterialCache(MaterialCache&& m);

		typedef std::vector<bool> ConsistencyVec;
		std::vector<ConsistencyVec> Level0ConsistencyCache;

		typedef std::vector<MaterialInfo> CellMaterialVec;
		typedef std::vector<CellMaterialVec> BlockMaterialVec;
		typedef std::vector<BlockMaterialVec> LevelMaterialVec;
		LevelMaterialVec LevelMaterialCache;
	};

	MaterialCache Cache;

	// Statistical data - Only for the most recent run!
	struct Statistics : public PolygonizationStatistics
	{
		Statistics();

		void Reset();

		Statistics& operator+=(const Statistics& rhs) {
			BlocksCalculated += rhs.BlocksCalculated;
			TrivialCells += rhs.TrivialCells;
			NonTrivialCells += rhs.NonTrivialCells;
			DegenerateTrianglesRemoved += rhs.DegenerateTrianglesRemoved;
			for (auto i = 0; i < CASES_COUNT; ++i) {
				PerCaseCellsCount[i] += rhs.PerCaseCellsCount[i];
			}

			return *this;
		}
	};

	Statistics Stats;
	unsigned GetNextBlockId();

	virtual float3 GetExtents() const override;
	virtual unsigned GetLevelsCount() const override;
	virtual unsigned GetBlocksForLevelCount(unsigned level) const override;
	virtual const BlockPolygons* GetBlockForLevel(unsigned level, unsigned id) const override;
	virtual const PolygonizationStatistics* GetStatistics() const override;
	virtual void Destroy() override;
	virtual unsigned GetCacheSizeBytes() const override;
	virtual unsigned GetPolygonDataSizeBytes() const override;
private:
	unsigned m_NextId;

	PolygonMap(const PolygonMap&);
	PolygonMap& operator=(const PolygonMap&);
};

struct MapModification : public Modification
{
	virtual const unsigned* GetModifiedBlocks(unsigned* count) const override;
	virtual void Destroy() override;

	typedef std::vector<unsigned> BlockIds;
	BlockIds ModifiedBlocks;
};

class TransVoxelImpl
{
public:
	TransVoxelImpl();
	
	PolygonMap* Execute(const Voxels::VoxelGrid& grid
									  , const MaterialMap* materials
									  , Modification* modification = nullptr);

	static unsigned GetBlockExtent();
};

}
