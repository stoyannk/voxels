// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#include "stdafx.h"
#include "TransVoxelImpl.h"
#include "Transvoxel.inl"

#include "../include/MaterialMap.h"
#include "../include/Grid.h"

#include <../dx11-framework/Utilities/MathInlines.h>

#include "StdAllocatorAligned.h"
#include "StructConversions.h"
#include "Aligned.h"

#include <glm/gtx/norm.hpp>
#include <iterator>

#ifndef PROFI_ENABLE
	#ifndef _DEBUG
		#define USE_OPENMAP
	#endif
#endif

#define SURFACE_SHIFTING_CORRECTION 1

#define USE_MATERIAL_CACHE

#include <concurrent_unordered_map.h>

namespace Voxels
{

static const glm::vec3 UNIT_X = glm::vec3(1.f, 0.f, 0.f);
static const glm::vec3 UNIT_Y = glm::vec3(0.f, 1.f, 0.f);
static const glm::vec3 UNIT_Z = glm::vec3(0.f, 0.f, 1.f);

typedef union {
	float asFloat;
	unsigned asUnsigned;
	int asInt;
} FloatInt;

#ifdef PROFI_ENABLE
static const char* LEVEL_STRS[] = 
{
	"Polygonize block Level 0",
	"Polygonize block Level 1",
	"Polygonize block Level 2",
	"Polygonize block Level 3",
	"Polygonize block Level 4",
	"Polygonize block Level 5",
	"Polygonize block Level 6",
};
#endif

static const unsigned BLOCK_EXTENT = 16;
static const unsigned BLOCK_EXTENT_POWER = 4;
static const float TRANSITION_CELL_COEFF = 0.25f;

///////// PUBLIC INTERFACE //////////////

Polygonizer::Polygonizer()
	: m_Impl(new TransVoxelImpl)
{
}

Polygonizer::~Polygonizer()
{
	delete m_Impl;
}

PolygonSurface* Polygonizer::Execute(const Grid& grid,
	const MaterialMap* materials,
	Modification* modification)
{
	return m_Impl->Execute(*grid.GetInternalRepresentation(), materials, modification);
}

Modification* Modification::Create()
{
	auto result = new MapModification;
	result->Map = nullptr;
	return result;
}

Modification::~Modification()
{}

const unsigned PolygonSurface::INVALID_ID = 0xFFFFFFFF;

glm::vec3 normalizeFixZero(const glm::vec3& in)
{
	static const glm::vec3 zero = glm::vec3(0.0f);
	const auto len = glm::length(in);

	if (len <= glm::epsilon<float>()) {
		return zero;
	}

	return in/len;
}

///////// PRIVATE ////////////
const unsigned* MapModification::GetModifiedBlocks(unsigned* count) const
{
	const auto sz = ModifiedBlocks.size();
	if (count)
		*count = sz;
	return sz ? &ModifiedBlocks[0] : nullptr;
}

void MapModification::Destroy()
{
	delete this;
}

PolygonMap::Statistics::Statistics()
{
	Reset();
}

void PolygonMap::Statistics::Reset()
{
	TrivialCells = 0;
	NonTrivialCells = 0;
	DegenerateTrianglesRemoved = 0;
	BlocksCalculated = 0;
	
	std::fill(PerCaseCellsCount, PerCaseCellsCount + _countof(PerCaseCellsCount), 0);
}

PolygonMap::PolygonMap()
	: Extents(0, 0, 0)
	, m_NextId(0)
{
	Stats.Reset();
}

PolygonMap::PolygonMap(PolygonMap&& lhs)
	: Stats(lhs.Stats)
	, Levels(std::move(lhs.Levels))
	, Extents(std::move(lhs.Extents))
	, Cache(std::move(lhs.Cache))
	, m_NextId(std::move(lhs.m_NextId))
{}

unsigned PolygonMap::GetNextBlockId()
{
	return m_NextId++;
}

PolygonMap::LodLevel::LodLevel()
{}

PolygonMap::LodLevel::LodLevel(LodLevel&& l)
	: Blocks(std::move(l.Blocks))
{}

PolygonMap::MaterialCache::MaterialCache()
{}

PolygonMap::MaterialCache::MaterialCache(MaterialCache&& m)
	: Level0ConsistencyCache(std::move(m.Level0ConsistencyCache))
	, LevelMaterialCache(std::move(m.LevelMaterialCache))
{}

float3 PolygonMap::GetExtents() const
{
	return float3(Extents.x, Extents.y, Extents.z);
}

unsigned PolygonMap::GetLevelsCount() const
{
	return Levels.size();
}

unsigned PolygonMap::GetBlocksForLevelCount(unsigned level) const
{
	return Levels[level].Blocks.size();
}

const BlockPolygons* PolygonMap::GetBlockForLevel(unsigned level, unsigned id) const
{
	if (id >= Levels[level].Blocks.size())
		return nullptr;
	return &Levels[level].Blocks[id];
}

const PolygonizationStatistics* PolygonMap::GetStatistics() const
{
	return &Stats;
}

unsigned PolygonMap::GetCacheSizeBytes() const
{
	size_t total = 0;
	for (auto blockCache = Cache.Level0ConsistencyCache.cbegin(), blockEnd = Cache.Level0ConsistencyCache.cend(); blockCache != blockEnd; ++blockCache)
	{
		total += blockCache->size();
	}
	// divide by 8 to take into account that the values are bools
	total >>= 3;

	for (auto block = Cache.LevelMaterialCache.cbegin(),
		blockEnd = Cache.LevelMaterialCache.cend();
		block != blockEnd;
		++block)
	{
		for (auto cell = block->cbegin(), cellEnd = block->cend();
			cell != cellEnd;
			++cell)
		{
			total += cell->size() * sizeof(MaterialCache::CellMaterialVec::value_type);
		}
	}

	return unsigned(total);
}

unsigned PolygonMap::GetPolygonDataSizeBytes() const
{
	size_t result = 0;
	for (auto level = Levels.cbegin(), levEnd = Levels.cend(); level != levEnd; ++level) {
		for (auto block = level->Blocks.cbegin(), blockEnd = level->Blocks.cend(); block != blockEnd; ++block) {
			result += block->Vertices.size() * sizeof(VerticesVec::value_type);
			result += block->Indices.size() * sizeof(IndicesVec::value_type);
			result += block->TransitionVertices.size() * sizeof(TransitionVerticesVec::value_type);
			result += block->TransitionIndices.size() * sizeof(TransitionIndicesVec::value_type);
		}
	}

	return unsigned(result);
}

void PolygonMap::Destroy()
{
	delete this;
}

TransVoxelImpl::TransVoxelImpl()
{}

PolygonBlock::PolygonBlock(unsigned id
						, const float3& min
						, const float3& max)
	: MinimalCorner(min)
	, MaximalCorner(max)
	, Id(id)
{}

PolygonBlock::PolygonBlock(PolygonBlock&& block)
	: Id(block.Id)
	, Vertices(std::move(block.Vertices))
	, Indices(std::move(block.Indices))
	, TransitionVertices(std::move(block.TransitionVertices))
	, TransitionIndices(std::move(block.TransitionIndices))
	, MinimalCorner(std::move(block.MinimalCorner))
	, MaximalCorner(std::move(block.MaximalCorner))
{}

PolygonBlock& PolygonBlock::operator=(PolygonBlock&& block)
{
	if (this != &block)
	{
		Id = block.Id;
		std::swap(Vertices, block.Vertices);
		std::swap(Indices, block.Indices);
		std::swap(TransitionVertices, block.TransitionVertices);
		std::swap(TransitionIndices, block.TransitionIndices);

		MinimalCorner = block.MinimalCorner;
		MaximalCorner = block.MaximalCorner;
	}

	return *this;
}

unsigned PolygonBlock::GetId() const
{
	return Id;
}

const PolygonVertex* PolygonBlock::GetVertices(unsigned* count) const
{
	const auto sz = Vertices.size();
	if (count)
		*count = sz;
	return sz ? &Vertices[0] : nullptr;
}

const unsigned* PolygonBlock::GetIndices(unsigned* count) const
{
	const auto sz = Indices.size();
	if (count)
		*count = sz;
	return sz ? &Indices[0] : nullptr;
}

const PolygonVertex* PolygonBlock::GetTransitionVertices(TransitionFaceId face, unsigned* count) const
{
	const auto sz = TransitionVertices[face].size();
	if (count)
		*count = sz;
	return sz ? &TransitionVertices[face][0] : nullptr;
}

const unsigned* PolygonBlock::GetTransitionIndices(TransitionFaceId face, unsigned* count) const
{
	const auto sz = TransitionIndices[face].size();
	if (count)
		*count = sz;
	return sz ? &TransitionIndices[face][0] : nullptr;
}

float3 PolygonBlock::GetMinimalCorner() const
{
	return float3(MinimalCorner.x, MinimalCorner.y, MinimalCorner.z);
}

float3 PolygonBlock::GetMaximalCorner() const
{
	return float3(MaximalCorner.x, MaximalCorner.y, MaximalCorner.z);
}

inline unsigned fastlog2i(unsigned value)
{
	unsigned ret = 0;
	while(value >>= 1) ++ret;

	return ret;
}

struct TransVoxelRun
{
	typedef std::vector<glm::vec4, StdAllocatorAligned<glm::vec4, 16>> SSEVectors;
	typedef std::vector<SSEVectors> SSEVectorsVec;

	typedef std::vector<glm::vec3, StdAllocatorAligned<glm::vec3, 16>> SSEVectors3;
	typedef std::vector<SSEVectors3> SSEVectors3Vec;

	typedef IndicesVec IndicesVec;
	typedef std::vector<IndicesVec> IndicesVecVec;
	
	typedef MaterialInfo MaterialInfo;
	typedef std::vector<MaterialInfo> MaterialsInfo;

	typedef PolygonMap ResultType;
	typedef MapModification ModificationType;
	
	typedef glm::vec3 Coord;

	TransVoxelRun(const Voxels::VoxelGrid& grid
				, const MaterialMap* materials
				, ModificationType* modification)
		: m_Grid(grid)
		, m_Materials(materials)
		, m_Modification(modification)
		, m_Result(nullptr)
	{
		if(m_Modification) {
			m_Result = static_cast<PolygonMap*>(m_Modification->Map);
		}
	}

	~TransVoxelRun()
	{
		std::for_each(m_PerThreadCaches.cbegin(),
			m_PerThreadCaches.cend(),
			[](const GridBlockMap::value_type& value)
		{
			delete value.second;
		});
		m_PerThreadCaches.clear();
		ts_BlocksCache = nullptr;
	}

	void GetBlocksCount(unsigned levelMultiplier, Coord& count) {
		count.x = float((m_Grid.GetWidth() >> BLOCK_EXTENT_POWER) / levelMultiplier);
		count.y = float((m_Grid.GetDepth() >> BLOCK_EXTENT_POWER) / levelMultiplier);
		count.z = float((m_Grid.GetHeight() >> BLOCK_EXTENT_POWER) / levelMultiplier);
	}

	void GenerateBlockListForLevel(unsigned level) {
		// we create a new fresh map
		m_LoadedBlocks.clear();
		auto multiplier = 1 << level;
		m_BlockCounts.resize(std::max(m_BlockCounts.size(), size_t(level + 1)));
		GetBlocksCount(multiplier, m_BlockCounts[level]);
		const auto totBlockCnt = m_BlockCounts[level].x * m_BlockCounts[level].y * m_BlockCounts[level].z;
		if(!m_Modification) {
			m_LoadedBlocks.reserve(size_t(totBlockCnt));

			for(unsigned blockZ = 0; blockZ < m_BlockCounts[level].z; ++blockZ)
			for(unsigned blockY = 0; blockY < m_BlockCounts[level].y; ++blockY)	
			for(unsigned blockX = 0; blockX < m_BlockCounts[level].x; ++blockX)
			{
				unsigned coordId = unsigned(blockZ * m_BlockCounts[level].y * m_BlockCounts[level].x + blockY * m_BlockCounts[level].x + blockX);
				m_LoadedBlocks.push_back(Block(m_Result->GetNextBlockId(), coordId, level, Coord(blockX, blockY, blockZ)));
			}

			const auto totalBlockExt = BLOCK_EXTENT*BLOCK_EXTENT*BLOCK_EXTENT;
			// allocate the caches
			if (level == 0)
			{
				assert(m_Result->Cache.Level0ConsistencyCache.empty());
				m_Result->Cache.Level0ConsistencyCache.reserve(size_t(totBlockCnt));
				for (auto i = 0; i < totBlockCnt; ++i)
				{
					m_Result->Cache.Level0ConsistencyCache.push_back(ResultType::MaterialCache::ConsistencyVec());
					m_Result->Cache.Level0ConsistencyCache.back().resize(totalBlockExt);
				}
			}
			else
			{
				assert(level > m_Result->Cache.LevelMaterialCache.size());
				m_Result->Cache.LevelMaterialCache.push_back(ResultType::MaterialCache::BlockMaterialVec());
				auto& blocksCache = m_Result->Cache.LevelMaterialCache.back();
				blocksCache.reserve(size_t(totBlockCnt));
				for (auto i = 0; i < totBlockCnt; ++i)
				{
					blocksCache.push_back(ResultType::MaterialCache::CellMaterialVec());
					blocksCache.back().resize(totalBlockExt, MaterialInfo(VoxelGrid::EMPTY_MATERIAL, 0));
				}
			}
		} 
		// we want to modify an existing map
		else {
			// NOTE: Here we calculate everything in "outer" coordinates - with Y up

			// find the extended cube that includes the dirty region - we also include the neighbors!
			const float blockMult = float(multiplier * BLOCK_EXTENT);
			
			const glm::vec3 extentsMin = glm::vec3(0.0f);
			const glm::vec3 extentsMax = tovec3(m_Result->Extents);
			const glm::vec3 minModified = tovec3(m_Modification->MinCornerModified);
			const glm::vec3 maxModified = tovec3(m_Modification->MaxCornerModified);
			const glm::vec3 blockMultVec = glm::vec3(blockMult);
			const glm::vec3 minCornerDirtyVec = glm::clamp(((glm::floor(minModified / blockMultVec - glm::vec3(1.0f))) * blockMult), extentsMin, extentsMax);
			const glm::vec3 maxCornerDirtyVec = glm::clamp(((glm::floor(maxModified / blockMultVec + glm::vec3(2.0f))) * blockMult), extentsMin, extentsMax);

			auto& oldLevelBlocks = static_cast<PolygonMap*>(m_Modification->Map)->Levels[level].Blocks;
			// delete all the old blocks
			oldLevelBlocks.erase(std::remove_if(oldLevelBlocks.begin(), oldLevelBlocks.end(), [&](PolygonBlock& block) {
				const glm::vec3 blockMin = tovec3(block.MinimalCorner);
				return glm::all(glm::greaterThanEqual(blockMin, minCornerDirtyVec)) && glm::all(glm::lessThan(blockMin, maxCornerDirtyVec));
			}), oldLevelBlocks.end());

			oldLevelBlocks.shrink_to_fit();

			const glm::vec3 minBlockCoord = minCornerDirtyVec / blockMultVec;
			const glm::vec3 maxBlockCoord = maxCornerDirtyVec / blockMultVec;

			// NOTE: Here we move again to the "inner" coordinates - with Z up
			for(unsigned blockZ = unsigned(minBlockCoord.y); blockZ < unsigned(maxBlockCoord.y); ++blockZ)
			for(unsigned blockY = unsigned(minBlockCoord.z); blockY < unsigned(maxBlockCoord.z); ++blockY)	
			for(unsigned blockX = unsigned(minBlockCoord.x); blockX < unsigned(maxBlockCoord.x); ++blockX)
			{
				auto id = m_Result->GetNextBlockId();
				unsigned coordId = unsigned(blockZ * m_BlockCounts[level].y * m_BlockCounts[level].x + blockY * m_BlockCounts[level].x + blockX);
				m_LoadedBlocks.push_back(Block(id, coordId, level, Coord(blockX, blockY, blockZ)));
				m_Modification->ModifiedBlocks.push_back(id);
			}
		}
	}
	
	ResultType* Execute()
	{
		PROFI_SCOPE(m_Result ? "Polygonize partial" : "Polygonize full")

		const auto gridWidth = m_Grid.GetWidth();
		const auto gridDepth = m_Grid.GetDepth();
		const auto gridHeight = m_Grid.GetHeight();

		// prepare the Polygon result if none is passed
		if(!m_Result) {
			m_Result = new ResultType;
			
			// NOTE: Here the second param is the height, because on output we observe the DX-style components
			m_Result->Extents = float3(float(gridWidth), float(gridHeight), float(gridDepth));
		} else {
			m_Result->Stats.Reset();
		}
		
		m_BlockCounts.resize(1);
		m_BlockCounts[0] = m_Grid.GetBlocksCount();
		m_MaxExtents = glm::vec3(m_Grid.GetWidth() - 1, m_Grid.GetDepth() - 1, m_Grid.GetHeight() - 1);

		const unsigned levelsCount = fastlog2i((gridWidth) >> BLOCK_EXTENT_POWER) + 1;
		
		for(auto currentLevel = 0u; currentLevel < levelsCount; ++currentLevel) {
			if(m_Result->Levels.size() <= currentLevel) {
				m_Result->Levels.push_back(ResultType::LodLevel());
			}
			
			GenerateBlockListForLevel(currentLevel);
			
			const int blocksCnt = int(m_LoadedBlocks.size());
			#ifdef USE_OPENMAP
			#pragma omp parallel for
			#endif
			for (int blockIt = 0; blockIt < blocksCnt; ++blockIt)
			{
				Block& block = m_LoadedBlocks[blockIt];
				{
					__declspec(thread) static auto tid = 0;
					tid = ::GetCurrentThreadId();
					auto cache = m_PerThreadCaches.find(tid);
					if (cache == m_PerThreadCaches.end()) {
						auto gridCachePtr = new GridBlocksCache(m_Grid);
						m_PerThreadCaches.insert(std::make_pair(tid, gridCachePtr));
						ts_BlocksCache = gridCachePtr;
					}
					else {
						ts_BlocksCache = cache->second;
					}
				}

				const bool isEmpty = block.Level == 0 && AreBlockAndNeighborsEmpty(block);
				if (!isEmpty) {
					PolygonizeBlock(block, *m_Result);
					if (block.Level && (block.Level != levelsCount - 1)) {
						GenerateTransitionCells(block);
					}
				}
			}
			m_Result->Stats.BlocksCalculated += m_LoadedBlocks.size();
			std::for_each(m_LoadedBlocks.begin(), m_LoadedBlocks.end(), [this](const Block& block) {
				m_Result->Stats += block.Stats;
			});
			PushBlocksToResult();
		}
		
		m_Result->Cache.Level0ConsistencyCache.shrink_to_fit();

		return m_Result;
	}

private:
	TransVoxelRun(const TransVoxelRun&);
	TransVoxelRun& operator=(const TransVoxelRun&);

	static const unsigned INVALID_INDEX = 0xFFFFFFFF;

	struct Cell
	{
		enum FaceId {
			ZPos = 0,
			YPos,
			XPos,
			
			ZNeg,
			YNeg,
			XNeg,

			Face_Count
		};
		
		Coord Base;
		Coord LocalBase;
		unsigned BlockCoordId;
		unsigned LocalId;
		char V[8];
		unsigned LevelMultiplier;
		char Level;
		bool IsOnBoundary;
		MaterialInfo Material;

		static const char* GetCornerIdsForFace(FaceId face) {
			static const char FaceCornerIds[Face_Count][4] = {
				{4, 5, 6, 7}, // ZPos
				{2, 3, 6, 7}, // YPos
				{1, 3, 5, 7}, // XPos
				{0, 1, 2, 3}, // ZNeg
				{0, 1, 4, 5}, // YNeg
				{0, 2, 4, 6}  // XNeg
			};

			return FaceCornerIds[face];
		}

		static bool IsCornerOnFace(char cornerId, FaceId face) {
			auto ids = GetCornerIdsForFace(face);
			const auto end = ids + 4;
			return end != std::find(ids, end, cornerId);
		}

		static bool IsEdgeOnFace(char corner0, char corner1, FaceId face) {
			return IsCornerOnFace(corner0, face) && IsCornerOnFace(corner1, face);
		}

		int CornerOnBlockBoundary(char cornerId) const {
			int result = 0;
			if(!IsOnBoundary || LevelMultiplier == 1)
				return result;

			if(LocalBase.x == 0 && IsCornerOnFace(cornerId, XNeg)) {
				result |= 1 << XNeg;
			}
			if(LocalBase.x == BLOCK_EXTENT - 1 && IsCornerOnFace(cornerId, XPos)) {
				result |= 1 << XPos;
			}
			if(LocalBase.y == 0 && IsCornerOnFace(cornerId, YNeg)) {
				result |= 1 << YNeg;
			}
			if(LocalBase.y == BLOCK_EXTENT - 1 && IsCornerOnFace(cornerId, YPos)) {
				result |= 1 << YPos;
			}
			if(LocalBase.z == 0 && IsCornerOnFace(cornerId, ZNeg)) {
				result |= 1 << ZNeg;
			}
			if(LocalBase.z == BLOCK_EXTENT - 1 && IsCornerOnFace(cornerId, ZPos)) {
				result |= 1 << ZPos;
			}

			return result;
		}

		int EdgeOnBlockBoundary(char corner0, char corner1) const {
			int result = 0;
			if(!IsOnBoundary || LevelMultiplier == 1)
				return result;

			if(LocalBase.x == 0 && IsEdgeOnFace(corner0, corner1, XNeg)) {
				result |= 1 << XNeg;
			}
			if(LocalBase.x == BLOCK_EXTENT - 1 && IsEdgeOnFace(corner0, corner1, XPos)) {
				result |= 1 << XPos;
			}
			if(LocalBase.y == 0 && IsEdgeOnFace(corner0, corner1, YNeg)) {
				result |= 1 << YNeg;
			}
			if(LocalBase.y == BLOCK_EXTENT - 1 && IsEdgeOnFace(corner0, corner1, YPos)) {
				result |= 1 << YPos;
			}
			if(LocalBase.z == 0 && IsEdgeOnFace(corner0, corner1, ZNeg)) {
				result |= 1 << ZNeg;
			}
			if(LocalBase.z == BLOCK_EXTENT - 1 && IsEdgeOnFace(corner0, corner1, ZPos)) {
				result |= 1 << ZPos;
			}

			return result;
		}

		void GetValuesForFace(FaceId face, char values[4]) const {
			switch(face) {
			case ZPos:
				values[0] = V[4]; values[1] = V[5]; values[2] = V[6]; values[3] = V[7];
				break;
			case YPos:
				values[0] = V[2]; values[1] = V[3]; values[2] = V[6]; values[3] = V[7];
				break;
			case XPos:
				values[0] = V[1]; values[1] = V[3]; values[2] = V[5]; values[3] = V[7];
				break;
			case ZNeg:
				values[0] = V[0]; values[1] = V[1]; values[2] = V[2]; values[3] = V[3];
				break;
			case YNeg:
				values[0] = V[0]; values[1] = V[1]; values[2] = V[4]; values[3] = V[5];
				break;
			case XNeg:
				values[0] = V[0]; values[1] = V[2]; values[2] = V[4]; values[3] = V[6];
				break;
			default:
				assert(false);
				break;
			};
		}

		void GetCornerCoordsForFace(FaceId face, glm::vec3 cornerCoords[4]) const {
			auto ids = GetCornerIdsForFace(face);
			for(auto i = 0; i < 4; ++i) {
				cornerCoords[i] = GetCornerCoords(ids[i]);
			}
		}

		// returns a vector directed opposite to the normal of the passed plane and
		// with the length of the side of the cube. 
		// NB: the vector is collinear to one of the 3 main axes
		glm::vec3 GetGlobalDirectionOppositeForFace(FaceId face) const {
			switch(face) {
			case ZPos:
				return GetCornerCoords(0) - GetCornerCoords(4);
				break;
			case YPos:
				return GetCornerCoords(0) - GetCornerCoords(2);
				break;
			case XPos:
				return GetCornerCoords(0) - GetCornerCoords(1);
				break;
			case ZNeg:
				return GetCornerCoords(4) - GetCornerCoords(0);
				break;
			case YNeg:
				return GetCornerCoords(2) - GetCornerCoords(0);
				break;
			case XNeg:
				return GetCornerCoords(1) - GetCornerCoords(0);
				break;
			default:
				assert(false);
				return glm::vec3(0);
				break;
			}
		}

		static glm::vec3 GetCornerCoords(int cornerId, const Coord& base, unsigned levelMultiplier) {
			assert(cornerId >= 0 && cornerId < 8);

			switch(cornerId)
			{
			case 0:
				return base;
			case 1:
				return base + UNIT_X * float(levelMultiplier);
			case 2:
				return base + UNIT_Y * float(levelMultiplier);
			case 3:
				return base + (UNIT_Y + UNIT_X) * float(levelMultiplier);
			case 4:
				return base + UNIT_Z * float(levelMultiplier);
			case 5:
				return base + (UNIT_Z + UNIT_X) * float(levelMultiplier);
			case 6:
				return base + (UNIT_Z + UNIT_Y) * float(levelMultiplier);
			case 7:
				return base + (UNIT_Z + UNIT_Y + UNIT_X) * float(levelMultiplier);
			};

			assert(false);
			return glm::vec3(FLT_MAX);
		}

		glm::vec3 GetCornerCoords(int cornerId) const {
			return Cell::GetCornerCoords(cornerId, Base, LevelMultiplier);
		}

		static unsigned long CalcCaseCode(const char V[8]) {
			return ((V[0] >> 7) & 0x01)
				 | ((V[1] >> 6) & 0x02)
				 | ((V[2] >> 5) & 0x04)
				 | ((V[3] >> 4) & 0x08)
				 | ((V[4] >> 3) & 0x10)
				 | ((V[5] >> 2) & 0x20)
				 | ((V[6] >> 1) & 0x40)
				 | ( V[7] & 0x80);
		}
	};

	void CalculateMaterialForCellCache(Cell& cell)
	{
		if (cell.LevelMultiplier == 1)
		{
			m_Result->Cache.Level0ConsistencyCache[cell.BlockCoordId][cell.LocalId] = 1;
			cell.Material = GetMaterialInfo(cell.Base);
			return;
		}

		auto baseVec = cell.Base;
		auto childMult = cell.LevelMultiplier >> 1;

		const auto childBlockExt = BLOCK_EXTENT * childMult;
		Coord childBlocksCnt;
		GetBlocksCount(childMult, childBlocksCnt);
		unsigned char materials[8];
		int counters[8] = { 0 };
		unsigned blends[8] = { 0 };
		unsigned count = 0;
		// calculate the 8 children
		for (unsigned z = 0; z < 2; ++z)
		for (unsigned y = 0; y < 2; ++y)
		for (unsigned x = 0; x < 2; ++x)
		{
			const auto newBase = baseVec +
				(UNIT_X * float(x * childMult)) + 
				(UNIT_Y * float(y * childMult)) + 
				(UNIT_Z * float(z * childMult));

			const unsigned blockId = unsigned(
				  (unsigned(newBase.z) / childBlockExt) * childBlocksCnt.y * childBlocksCnt.x
				+ (unsigned(newBase.y) / childBlockExt) * childBlocksCnt.x
				+ (unsigned(newBase.x) / childBlockExt));

			const auto localCoord = glm::vec3((unsigned)newBase.x % childBlockExt / childMult,
				(unsigned)newBase.y % childBlockExt / childMult,
				(unsigned)newBase.z % childBlockExt / childMult);

			const unsigned localId = unsigned(localCoord.z * BLOCK_EXTENT * BLOCK_EXTENT
				+ localCoord.y * BLOCK_EXTENT
				+ localCoord.x);
			
			MaterialInfo childMaterial;
			if (cell.LevelMultiplier == 2)
			{
				if (m_Result->Cache.Level0ConsistencyCache[blockId][localId])
				{
					childMaterial = GetMaterialInfo(newBase);
				}
				else
				{
					childMaterial.Id = VoxelGrid::EMPTY_MATERIAL;
				}
			}
			else
			{
				childMaterial = m_Result->Cache.LevelMaterialCache[cell.Level - 2][blockId][localId];
			}

			bool found = false;
			for (auto id = 0u; id < count; ++id) {
				if (materials[id] == childMaterial.Id) {
					++counters[id];
					blends[id] += childMaterial.Blend;
					found = true;
					break;
				}
			}
			if (!found && childMaterial.Id != VoxelGrid::EMPTY_MATERIAL) {
				materials[count] = childMaterial.Id;
				counters[count] = 1;
				blends[count] = childMaterial.Blend;
				++count;
			}
		}

		if (count) {
			const auto largestCnt = std::max_element(counters, counters + count);
			const auto largestId = largestCnt - counters;
			
			cell.Material.Id = materials[largestId];
			cell.Material.Blend = blends[largestId] / *largestCnt;

			m_Result->Cache.LevelMaterialCache[cell.Level - 1][cell.BlockCoordId][cell.LocalId] = cell.Material;
		}
	}

	bool CalculateMaterial(const Coord& base, unsigned multiplier, MaterialInfo& output) {
		if(multiplier == 1) {
			char V[8];
			for(auto i = 0; i < 8; ++i) {
				V[i] = GetGridValue(Cell::GetCornerCoords(i, base, multiplier));	
			}
			auto caseCode = Cell::CalcCaseCode(V);
			if(((caseCode ^ ((V[7] >> 7) & 0xFF)) == 0)) {
				return false; //is trivial
			}
			output = GetMaterialInfo(base);
			return true;
		}

		unsigned char materials[8];
		int counters[8] = {0};
		unsigned blends[8] = {0};
		unsigned count = 0;
		auto childMult = multiplier / 2;
		Coord nbc; 
		MaterialInfo childMaterial;
		// calculate the 8 children
		for(unsigned z = 0; z < 2; ++z)
		for(unsigned y = 0; y < 2; ++y)
		for(unsigned x = 0; x < 2; ++x)
		{
			auto nb = base + (UNIT_X * float(x * childMult)) + (UNIT_Y * float(y * childMult)) + (UNIT_Z * float(z * childMult));
			if (CalculateMaterial(nb, childMult, childMaterial)) {
				bool found = false;
				for(auto id = 0u; id < count; ++id) {
					if(materials[id] == childMaterial.Id) {
						++counters[id];
						blends[id] += childMaterial.Blend;
						found = true;
						break;
					}
				}
				if(!found) {
					materials[count] = childMaterial.Id;
					counters[count] = 1;
					blends[count] = childMaterial.Blend;
					++count;
				}
			}
		}

		if(count) {
			const auto largestCnt = std::max_element(counters, counters + count);
			const auto largestId = largestCnt - counters;
			output.Id = materials[largestId];
			output.Blend = blends[largestId] / *largestCnt;
			return true;
		} else {
			return false;
		}
	}

	MaterialInfo CalculateMaterialForCell(const Cell& cell) {
		MaterialInfo result;
		if(!CalculateMaterial(cell.Base, cell.LevelMultiplier, result)) {
			result = GetMaterialInfo(cell.Base);
		}

		return result;
	}

	template <unsigned Size>
	struct GenericFilledCell
	{
		GenericFilledCell()
		{
			Reset();
		}

		void Reset()
		{
			std::fill(ReuseVertexIndices, ReuseVertexIndices + Size, INVALID_INDEX);
		}

		unsigned ReuseVertexIndices[Size];
	};

	struct Block
	{
		// TODO: The data layout of the members in this class is very unoptimal - we should probably move
		// to a AoS layout as we work with all te props of a vertex
		Block(unsigned id, unsigned coordId, unsigned level, const Coord& coords)
			: Id(id)
			, CoordId(coordId)
			, Level(level)
			, LevelMultiplier(1 << level)
			, Coords(coords)
		{
			TransitionMeshVertices.resize(Cell::Face_Count);
			TransitionMaterials.resize(Cell::Face_Count);
			TransitionMeshSecondaryVertices.resize(Cell::Face_Count);
			TransitionMeshNormals.resize(Cell::Face_Count);
			TransitionMeshIndices.resize(Cell::Face_Count);

			FilledCells.reserve(BLOCK_EXTENT * BLOCK_EXTENT * BLOCK_EXTENT);
		}
	
		unsigned Id;
		unsigned Level;
		unsigned LevelMultiplier;
		unsigned CoordId;
		typedef GenericFilledCell<4> FilledCell;
		Coord Coords;

		typedef std::vector<FilledCell> FilledCellsVec;
		FilledCellsVec FilledCells;

		MaterialsInfo Materials;
		SSEVectors Vertices;
		SSEVectors SecondaryVertices;
		IndicesVec Indices;
		
		typedef std::vector<MaterialsInfo> MaterialsInfoVec;
		SSEVectorsVec TransitionMeshVertices;
		MaterialsInfoVec TransitionMaterials;
		SSEVectorsVec TransitionMeshSecondaryVertices;
		SSEVectors3Vec TransitionMeshNormals;
		IndicesVecVec TransitionMeshIndices;

		SSEVectors3 Normals;

		ResultType::Statistics Stats;
	};

	Cell MakeCell(const Coord& globalCoords, unsigned levelMultiplier)
	{
		PROFI_SCOPE_S3("MakeCell - coords")

			Cell result;
		result.Base = globalCoords;
		result.LevelMultiplier = levelMultiplier;
		result.LocalBase = glm::vec3((int)globalCoords.x % (BLOCK_EXTENT * levelMultiplier) / levelMultiplier,
			(int)globalCoords.y % (BLOCK_EXTENT * levelMultiplier) / levelMultiplier,
			(int)globalCoords.z % (BLOCK_EXTENT * levelMultiplier) / levelMultiplier);
		result.IsOnBoundary = result.LocalBase.x == 0 || result.LocalBase.x == BLOCK_EXTENT - 1
			|| result.LocalBase.y == 0 || result.LocalBase.y == BLOCK_EXTENT - 1
			|| result.LocalBase.z == 0 || result.LocalBase.z == BLOCK_EXTENT - 1;

		result.Material = MaterialInfo(VoxelGrid::EMPTY_MATERIAL, 0);

		for (auto i = 0; i < 8; ++i) {
			result.V[i] = GetGridValue(result.GetCornerCoords(i));
		}

#ifndef USE_MATERIAL_CACHE
		if (result.LevelMultiplier == 1)
		{
			cell.Material = GetMaterialInfo(result.LocalBase);
		}
		else
		{
			cell.Material = CalculateMaterialForCell(result);
		}
#endif

		return result;
	}

	glm::vec3 GetLocalCornerCoords(int cornerId, const Cell& cell, glm::vec3& blockCoords) const {
		assert(cornerId >= 0 && cornerId < 8);

		glm::vec3 localCoords = cell.LocalBase;

		switch (cornerId)
		{
		case 1:
			localCoords += UNIT_X;
			break;
		case 2:
			localCoords += UNIT_Y;
			break;
		case 3:
			localCoords += (UNIT_Y + UNIT_X);
			break;
		case 4:
			localCoords += UNIT_Z;
			break;
		case 5:
			localCoords += (UNIT_Z + UNIT_X);
			break;
		case 6:
			localCoords += (UNIT_Z + UNIT_Y);
			break;
		case 7:
			localCoords += (UNIT_Z + UNIT_Y + UNIT_X);
			break;
		};

		for (auto i = 0u; i < 3u; ++i) {
			if (localCoords[i] < BLOCK_EXTENT)
				continue;

			auto remainder = long(localCoords[i]) % (BLOCK_EXTENT - 1);
			if (remainder) {
				auto newBlockCoord = blockCoords[i] + remainder;
				if (newBlockCoord < m_BlockCounts[cell.Level][i]) {
					localCoords[i] = 0;
					blockCoords[i] = newBlockCoord;
				}
				else {
					localCoords[i] = BLOCK_EXTENT - 1;
				}
			}
		}
		return localCoords;
	}

	Cell MakeCell(const Block& block, const Coord& localCoords)
	{
		PROFI_SCOPE_S3("MakeCell - block")

			Cell result;
		result.Base = Coord((localCoords.x + block.Coords.x * BLOCK_EXTENT) * block.LevelMultiplier
			, (localCoords.y + block.Coords.y * BLOCK_EXTENT) * block.LevelMultiplier
			, (localCoords.z + block.Coords.z * BLOCK_EXTENT) * block.LevelMultiplier);

		result.LevelMultiplier = block.LevelMultiplier;
		result.Level = block.Level;
		result.LocalBase = localCoords;
		result.IsOnBoundary = result.LocalBase.x == 0 || result.LocalBase.x == BLOCK_EXTENT - 1
			|| result.LocalBase.y == 0 || result.LocalBase.y == BLOCK_EXTENT - 1
			|| result.LocalBase.z == 0 || result.LocalBase.z == BLOCK_EXTENT - 1;

		result.BlockCoordId = block.CoordId;
		result.LocalId = unsigned(result.LocalBase.z * BLOCK_EXTENT*BLOCK_EXTENT
			+ result.LocalBase.y * BLOCK_EXTENT
			+ result.LocalBase.x);

		if (block.Level == 0) {
			for (auto i = 0; i < 8; ++i) {
				auto blockCoords = block.Coords;
				const auto localCoords = GetLocalCornerCoords(i, result, blockCoords);
				result.V[i] = GetGridValue(block.Level, blockCoords, localCoords);
			}
		}
		else {
			for (auto i = 0; i < 8; ++i) {
				result.V[i] = GetGridValue(result.GetCornerCoords(i));
			}
		}
		return result;
	}

	class GridBlocksCache : public Aligned<16>
	{
	public:
		GridBlocksCache(const Voxels::VoxelGrid& grid)
			: m_Grid(grid)
			, m_CacheToEvict(0)
			, m_MaterialCacheToEvict(0)
			, m_GridSzMinusOne(glm::vec3(m_Grid.GetWidth() - 1, m_Grid.GetHeight() - 1, m_Grid.GetDepth() - 1))
			, m_BlockExt(glm::vec3(float(BLOCK_EXTENT)))
			, m_BlockCoeffs(glm::vec3(1,
										(m_Grid.GetWidth() / BLOCK_EXTENT),
										(m_Grid.GetWidth() / BLOCK_EXTENT) * (m_Grid.GetHeight() / BLOCK_EXTENT)))
			, m_BlockIdCoeffs(glm::vec3(1, BLOCK_EXTENT, BLOCK_EXTENT * BLOCK_EXTENT))
		{
			Reset();
		}

		void Reset()
		{
			std::fill(m_CachedBlocks, m_CachedBlocks + BLOCKS_CACHE_SIZE, std::make_pair(FREE_BLOCK, FREE_BLOCK));
			std::fill(m_MaterialCachedBlocks, m_MaterialCachedBlocks + BLOCKS_CACHE_SIZE, FREE_BLOCK);
		}

		char GetGridValue(unsigned blockLevel,
			const glm::vec3& blockCoordsf3,
			const glm::vec3& localCoords) const
		{
			const auto blockId = unsigned(glm::dot(blockCoordsf3, m_BlockCoeffs));
			const char* blockFound = nullptr;
			for (int i = 0u; i < BLOCKS_CACHE_SIZE; ++i)
			{
				if (blockLevel == m_CachedBlocks[i].first && blockId == m_CachedBlocks[i].second)
				{
					blockFound = m_Cache[i];
					break;
				}
			}
			if (!blockFound)
			{
				PROFI_SCOPE_S3("Fetch distance block");
				m_Grid.GetBlockData(blockCoordsf3, m_Cache[m_CacheToEvict]);
				m_CachedBlocks[m_CacheToEvict].first = blockLevel;
				m_CachedBlocks[m_CacheToEvict].second = blockId;
				blockFound = m_Cache[m_CacheToEvict];

				m_CacheToEvict = (m_CacheToEvict + 1) % BLOCKS_CACHE_SIZE;
			}

			const unsigned pointId = unsigned(glm::dot(glm::mod(localCoords, m_BlockExt), m_BlockIdCoeffs));
			return blockFound[pointId];
		}

		char GetGridValue(const glm::vec3& coordinates) const
		{
			const unsigned blockLevel = 0;
			glm::vec3 clamped;
			glm::vec3 blockCoordsf3;

			CalculateNeededCoords(coordinates,
				clamped,
				blockCoordsf3);
			
			return GetGridValue(blockLevel, blockCoordsf3, clamped);
		}

		MaterialInfo GetMaterialGridValue(const glm::vec3& coordinates) const
		{
			glm::vec3 clamped;
			glm::vec3 blockCoordsf3;

			CalculateNeededCoords(coordinates,
				clamped,
				blockCoordsf3);

			const auto blockId = unsigned(glm::dot(blockCoordsf3, m_BlockCoeffs));

			const unsigned char* materialBlockFound = nullptr;
			const unsigned char* blendBlockFound = nullptr;
			for (int i = 0u; i < BLOCKS_CACHE_SIZE; ++i)
			{
				if (blockId == m_MaterialCachedBlocks[i])
				{
					materialBlockFound = m_MaterialCache[i];
					blendBlockFound = m_BlendCache[i];
					break;
				}
			}
			if (!materialBlockFound)
			{
				PROFI_SCOPE_S3("Fetch material block")
				const auto vec3coords = glm::vec3(blockCoordsf3.x, blockCoordsf3.y, blockCoordsf3.z);
				m_Grid.GetMaterialBlockData(vec3coords,
					(unsigned char*)(m_MaterialCache[m_MaterialCacheToEvict]),
					(unsigned char*)(m_BlendCache[m_MaterialCacheToEvict]));
				m_MaterialCachedBlocks[m_MaterialCacheToEvict] = blockId;
				materialBlockFound = m_MaterialCache[m_MaterialCacheToEvict];
				blendBlockFound = m_BlendCache[m_MaterialCacheToEvict];

				m_MaterialCacheToEvict = (m_MaterialCacheToEvict + 1) % BLOCKS_CACHE_SIZE;
			}

			const unsigned pointId = unsigned(glm::dot(glm::mod(clamped, m_BlockExt), m_BlockIdCoeffs));
			return MaterialInfo(materialBlockFound[pointId], blendBlockFound[pointId]);
		}

	private:
		void CalculateNeededCoords(const glm::vec3& coordinates,
			glm::vec3& clamped,
			glm::vec3& blockCoordsf3) const
		{
			clamped = glm::clamp(coordinates, glm::vec3(0.f), m_GridSzMinusOne);

			blockCoordsf3 = glm::floor(clamped / m_BlockExt);
		}

		const Voxels::VoxelGrid& m_Grid;
		static const unsigned BLOCKS_CACHE_SIZE = 8u;
		static const unsigned FREE_BLOCK = 0xFFFFFFFF;

		glm::vec3 m_GridSzMinusOne;
		glm::vec3 m_BlockExt;
		glm::vec3 m_BlockIdCoeffs;
		glm::vec3 m_BlockCoeffs;

		mutable std::pair<unsigned, unsigned> m_CachedBlocks[BLOCKS_CACHE_SIZE];
		mutable char m_CacheToEvict;
		mutable char m_Cache[BLOCKS_CACHE_SIZE][BLOCK_EXTENT*BLOCK_EXTENT*BLOCK_EXTENT];

		mutable unsigned m_MaterialCachedBlocks[BLOCKS_CACHE_SIZE];
		mutable char m_MaterialCacheToEvict;
		mutable unsigned char m_MaterialCache[BLOCKS_CACHE_SIZE][BLOCK_EXTENT*BLOCK_EXTENT*BLOCK_EXTENT];
		mutable unsigned char m_BlendCache[BLOCKS_CACHE_SIZE][BLOCK_EXTENT*BLOCK_EXTENT*BLOCK_EXTENT];
	};

	char GetGridValue(unsigned blockLevel,
		const glm::vec3& blockCoordsf3,
		const glm::vec3& localCoords) const
	{
		return ts_BlocksCache->GetGridValue(blockLevel, blockCoordsf3, localCoords);
	}

	char GetGridValue(const glm::vec3& coord) const
	{
		return ts_BlocksCache->GetGridValue(coord);
	}

	MaterialInfo GetMaterialInfo(const Coord& coord) const
	{
		return ts_BlocksCache->GetMaterialGridValue(coord);
	}

	glm::vec3 CalcNormal(const glm::vec3& coord) const
	{
		const glm::vec3 minVec(0.f);
		auto normal = glm::vec3(  (GetGridValue(glm::clamp(coord + UNIT_X, minVec, m_MaxExtents)) - GetGridValue(glm::clamp(coord - UNIT_X, minVec, m_MaxExtents))) * 0.5f,
								  (GetGridValue(glm::clamp(coord + UNIT_Z, minVec, m_MaxExtents)) - GetGridValue(glm::clamp(coord - UNIT_Z, minVec, m_MaxExtents))) * 0.5f,
								  (GetGridValue(glm::clamp(coord + UNIT_Y, minVec, m_MaxExtents)) - GetGridValue(glm::clamp(coord - UNIT_Y, minVec, m_MaxExtents))) * 0.5f);
		return normalizeFixZero(normal);
	}

	bool FillTextureIdsForVertex(unsigned material, unsigned char blend, PolygonVertex& output) {
		auto m = m_Materials->GetMaterial(material);
		if(!m)
			return false;

		output.Textures.TextureIndices.Txz = m->DiffuseIds0[1];
		output.Textures.TextureIndices.Tpy = m->DiffuseIds0[0];
		output.Textures.TextureIndices.Tny = m->DiffuseIds0[2];

		output.Textures.TextureIndices.Uxz = m->DiffuseIds1[1];
		output.Textures.TextureIndices.Upy = m->DiffuseIds1[0];
		output.Textures.TextureIndices.Uny = m->DiffuseIds1[2];

		output.Textures.TextureIndices.Blend = blend;

		return true;
	}
	
	void PushBlocksToResult()
	{
		PROFI_SCOPE_S2("Push blocks to result")
		const float normFactor = 1 / 256.f;
		const float transitionNormFactor = 1 / 256.f;

		const auto totalSize = m_LoadedBlocks.size();
				
		for(auto id = 0u; id < totalSize; ++id) {
			auto& loadedBlock = m_LoadedBlocks[id];
			auto& indices = loadedBlock.Indices;
			auto& vertices = loadedBlock.Vertices;
			if(!vertices.size())
				continue;

			auto& outputBlocks = m_Result->Levels[loadedBlock.Level].Blocks;
	
			unsigned levelMultiplier = loadedBlock.LevelMultiplier;
			glm::vec3 coords = loadedBlock.Coords;
			coords *= levelMultiplier * BLOCK_EXTENT;
			float3 minCorner = tofloat3(coords);
			coords += glm::vec3(float(levelMultiplier * BLOCK_EXTENT));
			float3 maxCorner = tofloat3(coords);
			// NOTE: here we swap z & y because on output we want DX-style dimensions
			std::swap(minCorner.y, minCorner.z);
			std::swap(maxCorner.y, maxCorner.z);

			outputBlocks.push_back(PolygonBlock(loadedBlock.Id, minCorner, maxCorner));
			
			const auto indSz = indices.size();
			const auto eps = std::numeric_limits<float>::epsilon();
			auto& outputIndices = outputBlocks.back().Indices;
			outputIndices.reserve(indSz);

			for(auto i = 0u; i < indSz; i += 3)
			{
				const auto& v0 = vertices[indices[i]];
				const auto& v1 = vertices[indices[i + 1]];
				const auto& v2 = vertices[indices[i + 2]];

				auto a = v1 - v0;
				auto b = v2 - v0;

				const auto len = glm::length2(glm::cross(glm::vec3(a), glm::vec3(b)));

				if(len >= eps)
				{
					outputIndices.push_back(indices[i]);
					outputIndices.push_back(indices[i + 1]);
					outputIndices.push_back(indices[i + 2]);
				}
				else
				{
					++(m_Result->Stats.DegenerateTrianglesRemoved);
				}
			}

			auto& outputBlock = m_Result->Levels[loadedBlock.Level].Blocks.back();
			auto& secondaryVertices = loadedBlock.SecondaryVertices;
			auto& resultVertices = outputBlock.Vertices;
			auto& normals = loadedBlock.Normals;
			auto& materials = loadedBlock.Materials;
			const auto vertsSize = vertices.size();
			resultVertices.reserve(vertsSize);
			for (auto i = 0u; i < vertsSize; ++i)
			{
				auto& fv = vertices[i];
				resultVertices.push_back(PolygonVertex());
				auto& finalVertex = resultVertices.back(); 

				finalVertex.Position = tofloat3(glm::vec3(fv) * normFactor);
				// swap z & y so that the returned polygon surface matches the popular DX notation
				std::swap(finalVertex.Position.y, finalVertex.Position.z);

				// calculate the normal
				#ifdef _DEBUG
				const auto len = glm::length(normals[i]);
				assert((len - 1.0f) < 0.01f && "Un-normalized normal detected!");
				#endif
				finalVertex.Normal = tofloat3(normals[i]);

				auto& secondaryFv = secondaryVertices[i];
				// NB: We swap the last three bits with the three prior to them to 
				// match the output transition faces enum
				FloatInt transitionFlags;
				transitionFlags.asFloat = secondaryFv.w;
				if (transitionFlags.asUnsigned) {
					auto temp3bits = transitionFlags.asUnsigned & 0x7;
					transitionFlags.asUnsigned >>= 3;
					transitionFlags.asUnsigned |= (temp3bits << 3);
				}
				
				finalVertex.SecondaryPosition = tofloat4(secondaryFv * normFactor);
				finalVertex.SecondaryPosition.w = transitionFlags.asFloat;
				std::swap(finalVertex.SecondaryPosition.y, finalVertex.SecondaryPosition.z);

				//define the textures
				const auto matId = materials[i].Id;
				if(!FillTextureIdsForVertex(matId, materials[i].Blend, finalVertex)) {
					char buffer[VOXELS_LOG_SIZE];
					snprintf(buffer, VOXELS_LOG_SIZE, "Unable to assign textures on vertex with material id %u", matId);
					VOXLOG(LS_Error, buffer);
				}
			}

			// Transform and set the transition meshes
			outputBlock.TransitionVertices.reserve(6);
			outputBlock.TransitionIndices.reserve(6);
			for(auto trans = 0; trans < 6; ++trans) {
				outputBlock.TransitionVertices.push_back(VerticesVec());
				outputBlock.TransitionIndices.push_back(IndicesVec());

				auto& outputTransVertices = outputBlock.TransitionVertices.back();
				auto& outputTransIndices = outputBlock.TransitionIndices.back();

				auto& transVertices = loadedBlock.TransitionMeshVertices[trans];
				auto& transMaterials = loadedBlock.TransitionMaterials[trans];
				auto& transSecondaryVertices = loadedBlock.TransitionMeshSecondaryVertices[trans];
				auto& transNormals  = loadedBlock.TransitionMeshNormals[trans];
				auto& transIndices  = loadedBlock.TransitionMeshIndices[trans];

				assert(transNormals.size() == transVertices.size());

				const auto vertsCount = transNormals.size();
				outputTransVertices.reserve(vertsCount);
				for(auto i = 0u; i < vertsCount; ++i) {
					outputTransVertices.push_back(PolygonVertex());
					auto& finalVertex = outputTransVertices.back();

					finalVertex.Position = tofloat3(glm::vec3(transVertices[i]) * transitionNormFactor);
					// swap z & y so that the returned polygon surface matches the popular DX notation
					std::swap(finalVertex.Position.y, finalVertex.Position.z);

					FloatInt transitionFlags;
					transitionFlags.asFloat = transSecondaryVertices[i].w;
					if (transitionFlags.asUnsigned) {
						auto temp3bits = transitionFlags.asUnsigned & 0x7;
						transitionFlags.asUnsigned >>= 3;
						transitionFlags.asUnsigned |= (temp3bits << 3);
					}
					finalVertex.SecondaryPosition = tofloat4(transSecondaryVertices[i] * transitionNormFactor);
					finalVertex.SecondaryPosition.w = transitionFlags.asFloat;
					std::swap(finalVertex.SecondaryPosition.y, finalVertex.SecondaryPosition.z);

					#ifdef _DEBUG
					const auto len = glm::length(transNormals[i]);
					assert((len - 1.0f) < 0.01f && "Un-normalized normal detected!");
					#endif
					finalVertex.Normal = tofloat3(transNormals[i]);

					//define the textures
					const auto matId = transMaterials[i].Id;
					if(!FillTextureIdsForVertex(matId, transMaterials[i].Blend, finalVertex)) {
						char buffer[VOXELS_LOG_SIZE];
						snprintf(buffer, VOXELS_LOG_SIZE, "Unable to assign textures on vertex with material id %u", matId);
						VOXLOG(LS_Error, buffer);
					}
				}

				std::copy(transIndices.cbegin(), transIndices.cend(), std::back_inserter(outputTransIndices));
			}
		}
	}

	static Coord FindAdjCellForReuse(const char direction, const Coord& cellCoord) {
		auto result(cellCoord);
		//x-dir
		if(direction & 1)
		{
			result.x -= 1;
		}
		//y-dir
		if(direction & 0x2)
		{
			result.y -= 1;
		}
		//z-dir
		if(direction & 0x4)
		{
			result.z -= 1;
		}
		return result;
	}

	unsigned GenerateVertexFromPoint(Block& block, const Cell& cell, char v) {
		auto V = cell.GetCornerCoords(v);
		block.Normals.push_back(CalcNormal(V));
		
		auto myMaterial = GetMaterialInfo(V);
		if(cell.Material.Id != myMaterial.Id) {
			block.Materials.push_back(cell.Material);
		} else {
			block.Materials.push_back(myMaterial);
		}
		
		V *= 256.f;
		FloatInt onboundary;
		onboundary.asInt = cell.CornerOnBlockBoundary(v);
		block.Vertices.push_back(glm::vec4(V, onboundary.asFloat));

		return block.Vertices.size() - 1;
	};

	static unsigned MakeReuseId(const Coord& reuseCoord) {
		return unsigned(reuseCoord.z * (BLOCK_EXTENT) * (BLOCK_EXTENT) + reuseCoord.y * (BLOCK_EXTENT) + reuseCoord.x);
	}

	glm::vec3 AccumulateVertexTransitionDelta(int boundaryFaces, const Cell& cell) {
		glm::vec3 result = glm::vec3(0);
		for(int i = 0; i < 6; ++i) {
			if(boundaryFaces & (1 << i)) {
				result += (cell.GetGlobalDirectionOppositeForFace(Cell::FaceId(i)) * (TRANSITION_CELL_COEFF));
			}
		}

		return result;
	}

	void FindBestVertexInLODChain(int level, glm::vec3& P0, glm::vec3& P1) const {
		for(int lev = level; lev > 0; --lev) {
			const auto midpoint = P0 + (P1 - P0) / 2.f;

			#ifdef _DEBUG
			float intpart;
			assert(std::abs(modf(midpoint.x, &intpart)) <= FLT_MIN
				&& std::abs(modf(midpoint.y, &intpart)) <= FLT_MIN
				&& std::abs(modf(midpoint.z, &intpart)) <= FLT_MIN);
			#endif

			const auto midValue = GetGridValue(midpoint);
			const auto p0Value = GetGridValue(P0);
			const auto p1Value = GetGridValue(P1);

			if((p0Value * midValue) <= 0) {
				P1 = midpoint;
			} else {
				P0 = midpoint;
			}

			#ifdef _DEBUG
			assert(P0.x == P1.x || P0.y == P1.y || P0.z == P1.z);
			#endif
		}
	}
	
	bool AreBlockAndNeighborsEmpty(Block& block) const
	{
		assert(block.Level == 0);
		// check all the neighbors
		for (int z = -1; z < 2; ++z)
		for (int y = -1; y < 2; ++y)
		for (int x = -1; x < 2; ++x)
		{
			glm::vec3 coord = glm::vec3(StMath::clamp_value(block.Coords.x + x, 0.f, m_BlockCounts[0].x - 1),
				StMath::clamp_value(block.Coords.y + y, 0.f, m_BlockCounts[0].y - 1),
				StMath::clamp_value(block.Coords.z + z, 0.f, m_BlockCounts[0].z - 1));
			if (!m_Grid.IsBlockEmpty(coord))
				return false;
		}
		
		return true;
	}

	void PolygonizeBlock(Block& block, PolygonMap& outputMap)
	{			
		PROFI_SCOPE_S2("Polygonize block")

		PROFI_SCOPE_S3(LEVEL_STRS[block.Level])
		unsigned verticesIndices[15];
		
		assert(BLOCK_EXTENT == BLOCK_EXTENT && BLOCK_EXTENT == BLOCK_EXTENT);
		const auto blockBase = block.Coords * float(BLOCK_EXTENT);

		unsigned char reuseValidityMask = 0;
		for(unsigned cellZ = 0; cellZ < BLOCK_EXTENT; ++cellZ)
		{
			// clear the y-bit
			reuseValidityMask &= 0xD;
			for(unsigned cellY = 0; cellY < BLOCK_EXTENT; ++cellY)	
			{
				// clear the x-bit
				reuseValidityMask &= 0xE;
				for(unsigned cellX = 0; cellX < BLOCK_EXTENT; ++cellX)
				{
					Coord cellCoords(cellX, cellY, cellZ);
					Cell cell = MakeCell(block, cellCoords);
					
					block.FilledCells.push_back(Block::FilledCell());

					auto& thisCellReuseData = block.FilledCells.back();

					//classify the cell
					const unsigned long caseCode = Cell::CalcCaseCode(cell.V);

					const bool isTrivial = ((caseCode ^ ((cell.V[7] >> 7) & 0xFF)) == 0);
					if(isTrivial)
					{
						++block.Stats.TrivialCells;
						continue;
					}
					
					#ifdef USE_MATERIAL_CACHE
					CalculateMaterialForCellCache(cell);
					#endif
					
					++block.Stats.NonTrivialCells;
					
					const unsigned caseIndex = regularCellClass[caseCode];
					++block.Stats.PerCaseCellsCount[caseIndex];

					const RegularCellData regCellData = regularCellData[caseIndex];
					const unsigned short* regVertexData = regularVertexData[caseCode];

					for (long vertexIndex = 0, vertexCount = regCellData.GetVertexCount(); vertexIndex < vertexCount; ++vertexIndex)
					{
						const char edgeIndex = regVertexData[vertexIndex] & 0xFF;
						char direction = (regVertexData[vertexIndex] >> 12);
						char vIndexInCell = (regVertexData[vertexIndex] >> 8) & 0x0F;
						bool checkReuse = true;
						bool shouldCreateNewVertex = true;

						assert(vIndexInCell < 4);

						const unsigned char v0 = (edgeIndex >> 4) & 0x0F;
						const unsigned char v1 = edgeIndex & 0x0F;
						long t = (cell.V[v1] << 8) / (cell.V[v1] - cell.V[v0]);

						// The vertex lies on some endpoint of the edge
						if ((t & 0x00FF) == 0)
						{
							// Vertex lies at the higher-numbered endpoint.	We never reuse
							if(t == 0 && v1 == 7)
								checkReuse = false;
							// Vertex lies on some of the endpoints of the edge - we need to modify the direction
							if(checkReuse)
							{
								auto dirModifier = (t == 0) ? v1 : v0;
								direction = (dirModifier ^ 7);
							}

							// Override the reuse index when a vertex lies on an edge endpoint
							vIndexInCell = 0;
						}

						// Reuse an old vertex
						if((direction & reuseValidityMask) == direction && checkReuse)
						{
							Coord reuseCoord = FindAdjCellForReuse(direction, cellCoords);

							const auto& filledCell = block.FilledCells[MakeReuseId(reuseCoord)];

							auto reuseIndex = filledCell.ReuseVertexIndices[vIndexInCell];

							bool sameMaterial = true;
							// check the material
							if(reuseIndex != INVALID_INDEX) {
								const auto reuseMaterial = block.Materials[reuseIndex];
								sameMaterial = (reuseMaterial.Id == cell.Material.Id);
							}

							if(sameMaterial) {
								verticesIndices[vertexIndex] = reuseIndex;
								if((t & 0x00FF) != 0 && t != 0)
								{
									assert(verticesIndices[vertexIndex] != INVALID_INDEX);
								}
								// We allow the creation of a vertex here (it's the lower endpoint of an edge)
								else
								{
									if(verticesIndices[vertexIndex] == INVALID_INDEX)
									{
										auto index = GenerateVertexFromPoint(block, cell, v0);
										verticesIndices[vertexIndex] = index;
									}
								}

								shouldCreateNewVertex = false;
							}
						}
						
						// Create a new vertex on this pos
						if(shouldCreateNewVertex)
						{
							// Vertex lies on one of the endpoints - create the new vertex there
							if((t & 0x00FF) == 0)
							{
								auto index = GenerateVertexFromPoint(block, cell, (t == 0) ? v1 : v0);
								if(t == 0 && v1 == 7)
									thisCellReuseData.ReuseVertexIndices[vIndexInCell] = index;
								// Save this index for the triangulation
								verticesIndices[vertexIndex] = index;
							}
							// Vertex lies in the interior of the edge.
							else
							{
								glm::vec3 P0, N0;
								glm::vec3 P1, N1;
								MaterialInfo M0, M1;
								// if this is a lower LOD level, prevent surface shifting by descending
								// the vertices at the corners and looking for the best two
								if(block.Level && SURFACE_SHIFTING_CORRECTION) {
									P0 = cell.GetCornerCoords(v0);
									P1 = cell.GetCornerCoords(v1);
									FindBestVertexInLODChain(block.Level, P0, P1);

									const auto p0Value = GetGridValue(P0);
									const auto p1Value = GetGridValue(P1);
									if(p0Value != p1Value) {
										t = (p1Value << 8) / (p1Value - p0Value);
									}
									else {
										t = 0;
									}

								} else {
									P0 = cell.GetCornerCoords(v0);
									P1 = cell.GetCornerCoords(v1);
								}

								N0 = CalcNormal(P0);
								N1 = CalcNormal(P1);
								M0 = GetMaterialInfo(P0);
								M1 = GetMaterialInfo(P1);

								const long u = 0x0100 - t;
								glm::vec4 Q = glm::vec4((float)t * P0 + (float)u * P1, 0.f);

								// push the vertex in the global array
								FloatInt onboundary;
								onboundary.asInt = cell.EdgeOnBlockBoundary(v0, v1);
								Q.w = onboundary.asFloat;
								block.Vertices.push_back(Q);
								if((M0.Id == M1.Id) &&  (M0.Id == cell.Material.Id)) {
									M0.Blend = Voxels::BlendFactor(((float)t * M0.Blend + (float)u * M1.Blend) / 256.f);
									block.Materials.push_back(M0);
								} else {
									block.Materials.push_back(cell.Material);
								}
								block.Normals.push_back(normalizeFixZero( N0 * (t/256.f) + N1 * (u/256.f)) );

								auto index = block.Vertices.size() - 1;

								// save the index in this cell's reuse data
								if(direction == 0x8)
								{
									thisCellReuseData.ReuseVertexIndices[vIndexInCell] = index;
								}
								
								// save this index for the triangulation
								verticesIndices[vertexIndex] = index;
							}
						}
					}
					
					block.SecondaryVertices.resize(block.Vertices.size());
					// push all triangles
					for (auto v = 0L, triagCount = regCellData.GetTriangleCount() * 3; v < triagCount; v += 3)
					{
						for(auto vtr = 0; vtr < 3; ++vtr) {
							auto vIndex = verticesIndices[regCellData.vertexIndex[v + vtr]];
							block.Indices.push_back(vIndex);

							auto vertex = block.Vertices[vIndex];
							FloatInt boundaries;
							boundaries.asFloat = vertex.w;
							if (boundaries.asInt > 0) {
								// move the vertex to make room for the transition cell
								auto delta = AccumulateVertexTransitionDelta(boundaries.asInt, cell) * 256.f;
								// TODO: Move in the tangent plane
								block.SecondaryVertices[vIndex] = glm::vec4(glm::vec3(vertex) + delta, boundaries.asFloat);
							} else {
								block.SecondaryVertices[vIndex] = vertex;
							}
						}
					}

					reuseValidityMask |= 0x1;
				} // x
				if(reuseValidityMask & 1)
					reuseValidityMask |= 0x2;
			} // y
			if(reuseValidityMask & 2)
				reuseValidityMask |= 0x4;
		} // z
	}

	typedef GenericFilledCell<10> TransitionFilledCell;

	void GenerateTransitionCells(Block& block)
	{
		PROFI_SCOPE_S2("Generate transition cells")

		int row = 0, highRow = 0;
		int column = 0, highColumn = 0;
		const int minDim = 0;
		const int maxDim = 15;
		// when going in 2D and increasing row and column this mapping translates
		// them to the 3D voxel coordinates
		const int* transitionCases[][3] = {
			{&column, &row, &minDim }, 
			{&column, &minDim, &row },
			{&minDim, &column, &row },

			{&column, &row, &maxDim }, 
			{&column, &maxDim, &row },
			{&maxDim, &column, &row },
		};

		const int* highResCoords[][3] = {
			{&highColumn, &highRow, &minDim }, 
			{&highColumn, &minDim, &highRow },
			{&minDim, &highColumn, &highRow },

			{&highColumn, &highRow, &minDim }, 
			{&highColumn, &minDim, &highRow },
			{&minDim, &highColumn, &highRow },
		};

		float blockDeltas[][3] = {
			{ 0, 0, -0.5f }, // when going 'back' from a low res cell we need to
			{ 0, -0.5f, 0 }, // move half a low-level cell 
			{ -0.5f, 0, 0 },
			
			{ 0, 0, 1 }, // here we need to skip the whole low res cell
			{ 0, 1, 0 }, 
			{ 1, 0, 0 }	 
		};

		// the faces that have to be fetched from the LOW-res cells for eevry case
		Cell::FaceId faceIds[] = {
			Cell::ZNeg,
			Cell::YNeg,
			Cell::XNeg,

			Cell::ZPos,
			Cell::YPos,
			Cell::XPos
		};

		// the faces that have to be fetched from the HIGH-res cells for every case
		Cell::FaceId highFaceIds[] = {
			Cell::ZPos,
			Cell::YPos,
			Cell::XPos,

			Cell::ZNeg,
			Cell::YNeg,
			Cell::XNeg
		};

		// we need to reverse the windiing for some cases because they won't be ok in world space
		const int reverseWinding[] = { 0, 1, 0, 1, 0, 1 };

		static const int caseCodeCoeffs[9] = { 0x01, 0x02, 0x04, 0x80, 0x100, 0x08, 0x40, 0x20, 0x10 };
		static const int charByteSz = sizeof(char) * 8;

		const auto highResLevel = block.LevelMultiplier >> 1;
		Coord blocksCnt;
		GetBlocksCount(block.LevelMultiplier, blocksCnt);
		
		for(auto transitionId = 0u; transitionId < 6; ++transitionId)
		{
			// check if there is a neighbor block or we are at a grid boundary
			Coord neighborBlockCoords(block.Coords.x + blockDeltas[transitionId][0]
									, block.Coords.y + blockDeltas[transitionId][1]
									, block.Coords.z + blockDeltas[transitionId][2]);
			if((neighborBlockCoords.x < 0 || neighborBlockCoords.x >= blocksCnt.x)
			|| (neighborBlockCoords.y < 0 || neighborBlockCoords.y >= blocksCnt.y)
			|| (neighborBlockCoords.z < 0 || neighborBlockCoords.z >= blocksCnt.z))
				continue;

			// TODO: Those could be global for a run to save one vector allocation per block
			typedef std::vector<TransitionFilledCell> FilledRow;
			FilledRow currentFilledRow;
			currentFilledRow.resize(BLOCK_EXTENT);
			FilledRow previousFilledRow;
			previousFilledRow.resize(BLOCK_EXTENT);

			const auto& face = transitionCases[transitionId];
			const auto& highFace = highResCoords[transitionId];

			highRow = 0;
			highColumn = 0;
			unsigned char reuseValidityMask = 0;
			for(row = 0; row < BLOCK_EXTENT; ++row) 
			{
				reuseValidityMask &= 0x2; // clear the column reuse for the first cell in a row
				for(column = 0; column < BLOCK_EXTENT; ++column)
				{
					const Coord cellCoords(*face[0], *face[1], *face[2]);

					Cell lowResCell = MakeCell(block, cellCoords);
					#ifdef USE_MATERIAL_CACHE
					CalculateMaterialForCellCache(lowResCell);
					#endif
					
					Coord highResBase((lowResCell.Base.x + blockDeltas[transitionId][0] * block.LevelMultiplier)
									, (lowResCell.Base.y + blockDeltas[transitionId][1] * block.LevelMultiplier)
									, (lowResCell.Base.z + blockDeltas[transitionId][2] * block.LevelMultiplier));

					assert(highRow == 0 && highColumn == 0);
					Cell highResCell0 = MakeCell(highResBase, highResLevel);
					++highColumn;
					Cell highResCell1 = MakeCell(Coord(highResBase.x + (*highFace[0] * highResLevel), highResBase.y + (*highFace[1] * highResLevel), highResBase.z + (*highFace[2] * highResLevel)), highResLevel);
					--highColumn; ++highRow;
					Cell highResCell2 = MakeCell(Coord(highResBase.x + (*highFace[0] * highResLevel), highResBase.y + (*highFace[1] * highResLevel), highResBase.z + (*highFace[2] * highResLevel)), highResLevel);
					--highRow;

					char values[13];
					glm::vec3 cornerCoords[13];

					// NB: The coordinates we use and output here are global in the space of the grid

					char cellValues[4];
					glm::vec3 corners[4];
					auto lowResCellCornerIds = Cell::GetCornerIdsForFace(faceIds[transitionId]);
					lowResCell.GetValuesForFace(faceIds[transitionId], cellValues);
					lowResCell.GetCornerCoordsForFace(faceIds[transitionId], corners);
					values[0xA] = cellValues[1]; cornerCoords[0xA] = corners[1]; // A
					values[0xB] = cellValues[2]; cornerCoords[0xB] = corners[2]; // B
					values[0xC] = cellValues[3]; cornerCoords[0xC] = corners[3]; // C
					values[9]   = cellValues[0]; cornerCoords[9]   = corners[0]; 

					highResCell0.GetValuesForFace(highFaceIds[transitionId], cellValues);
					highResCell0.GetCornerCoordsForFace(highFaceIds[transitionId], corners);
					values[0] = cellValues[0]; cornerCoords[0] = corners[0];
					values[1] = cellValues[1]; cornerCoords[1] = corners[1];
					values[3] = cellValues[2]; cornerCoords[3] = corners[2];
					values[4] = cellValues[3]; cornerCoords[4] = corners[3];

					highResCell1.GetValuesForFace(highFaceIds[transitionId], cellValues);
					highResCell1.GetCornerCoordsForFace(highFaceIds[transitionId], corners);
					values[2] = cellValues[1]; cornerCoords[2] = corners[1];
					values[5] = cellValues[3]; cornerCoords[5] = corners[3];

					highResCell2.GetValuesForFace(highFaceIds[transitionId], cellValues);
					highResCell2.GetCornerCoordsForFace(highFaceIds[transitionId], corners);
					values[6] = cellValues[2]; cornerCoords[6] = corners[2];
					values[7] = cellValues[3]; cornerCoords[7] = corners[3];

					values[8] = values[0xC]; cornerCoords[8] = cornerCoords[0xC]; // 8 == C

					assert(values[0] == values[9] 
						&& values[2] == values[0xA] 
						&& values[6] == values[0xB]
						&& values[8] == values[0xC]);

					// move the corner coords of the low-res face of the transition cell "in" the 
					// low res cell by the coefficient
					auto lowresFaceMoveDir = lowResCell.GetGlobalDirectionOppositeForFace(faceIds[transitionId]);

					int caseCode = 0;
					for(auto ci = 0; ci < 9; ++ci) {
						// add the coefficient only if the value is negative
						caseCode += ((values[ci] >> (charByteSz - 1)) & 1) * caseCodeCoeffs[ci];
					}

					if(caseCode == 0 || caseCode == 511)
						continue;

					const auto transitionClass = transitionCellClass[caseCode];
					const int shouldInvertWinding = (transitionClass >> 7);
					const TransitionCellData cellData = transitionCellData[transitionClass & 0x7F]; // ANDed as per documentation
					
					const unsigned short* vertexData = transitionVertexData[caseCode];

					IndicesVec cellIndices;
					cellIndices.reserve(cellData.GetVertexCount());
					for (long vertexIndex = 0, vertexCount = cellData.GetVertexCount(); vertexIndex < vertexCount; ++vertexIndex) {
						const char edgeIndex = vertexData[vertexIndex] & 0xFF;
						const unsigned char v0 = (edgeIndex >> 4) & 0x0F;
						const unsigned char v1 = edgeIndex & 0x0F;

						char reuseDirection = (vertexData[vertexIndex] >> 12);
						char reuseIndex = (vertexData[vertexIndex] >> 8) & 0x0F;

						long t = (values[v1] << 8) / (values[v1] - values[v0]);

						bool didReuse = false;
						bool addForReuse = true;

						// Used ONLY if the vertex lies on some endpoint
						auto corner = (t == 0) ? v1 : v0;

						// The vertex lies on some endpoint of the edge
						if ((t & 0x00FF) == 0)
						{
							// Vertex lies at the higher-numbered endpoint
							reuseDirection = transitionCornerData[corner] >> 4;
							reuseIndex = transitionCornerData[corner] & 0xF;
						}
						// Try to reuse an old vertex
						if((reuseDirection & reuseValidityMask) == reuseDirection)
						{
							addForReuse = false;

							// this is the first row or column - there is nothing to reuse
							assert(!((reuseDirection & 0x2 && row == 0) || (reuseDirection & 0x1 && column == 0)));
							auto& reuseRow = (reuseDirection & 0x2) ? previousFilledRow : currentFilledRow;
							auto reuseColumnId = column;
							if(reuseDirection & 0x1)
								--reuseColumnId;

							auto indexFound = reuseRow[reuseColumnId].ReuseVertexIndices[reuseIndex];
							if(indexFound != INVALID_INDEX)
							{
								auto reuseMaterial = block.TransitionMaterials[transitionId][indexFound];
								if(reuseMaterial.Id == lowResCell.Material.Id) {
									didReuse = true;
									cellIndices.push_back(indexFound);
								}
							}
						}
						// Create a new vertex
						if(!didReuse)
						{
							glm::vec3 P0 = cornerCoords[v0], N0;
							glm::vec3 P1 = cornerCoords[v1], N1;
							long u = 0;
							
							int adjacencyInfo = 0;

							// Vertex lies on some endpoint
							if((t & 0x00FF) == 0)
							{
								if(t == 0)
								{
									u = 256;
									N0 = glm::vec3(0.f);
									N1 = CalcNormal(P1);

									if(v1 >= 0x9) {
										adjacencyInfo = lowResCell.CornerOnBlockBoundary(lowResCellCornerIds[v1 - 9]);
										assert(adjacencyInfo);
									}
								}
								else
								{
									u = 0;
									t = 256;
									N0 = CalcNormal(P0);
									N1 = glm::vec3(0.f);
									
									if(v0 >= 0x9) {
										adjacencyInfo = lowResCell.CornerOnBlockBoundary(lowResCellCornerIds[v0 - 9]);
										assert(adjacencyInfo);
									}
								}
							}
							// vertex lies in the interior on an edge
							else
							{
								int lodOfEdge = (v0 >= 0x9) ? block.Level : block.Level - 1;
								if(SURFACE_SHIFTING_CORRECTION && lodOfEdge > 0)
								{
									FindBestVertexInLODChain(lodOfEdge, P0, P1);
									const auto p0Value = GetGridValue(P0);
									const auto p1Value = GetGridValue(P1);
									if(p0Value != p1Value) {
										t = (p1Value << 8) / (p1Value - p0Value);
									}
									else {
										t = 0;
									}
								}

								u = 0x0100 - t;
								N0 = CalcNormal(P0);
								N1 = CalcNormal(P1);

								if(v0 >= 0x9 && v1 >= 0x9) {
									adjacencyInfo = lowResCell.EdgeOnBlockBoundary(lowResCellCornerIds[v0 - 9], lowResCellCornerIds[v1 - 9]);
									assert(adjacencyInfo);
								}
							}
							MaterialInfo M0 = GetMaterialInfo(P0);
							MaterialInfo M1 = GetMaterialInfo(P1);

							// NB: Here the primary/secondary position calculations are very complicated (I think better can be done)
							// The effect they acieve is illustrated in Eric's paper on figure 4.13. The combination of adj. calculations 
							// and vertex positioning here and the corresponding vertex decision making logic in the shader do the magic.

							// secondary vertices are transposed so that we can properly fill the gap 
							// when 3 high-res cells are on one of our corners
							auto P0Sec = P0;
							auto P1Sec = P1;
							
							if(v0 >= 0x9 || v1 >= 0x9) {
								auto delta = AccumulateVertexTransitionDelta(adjacencyInfo, lowResCell);

								// transpose the vertices only if this vertex is only on the face we are
								// currenlty working on - this has the effect of NOT moving vertices
								// that are on the corner of a block
								bool isSimpleAdj = adjacencyInfo == (1 << faceIds[transitionId]);
								if(v0 >= 0x9)
								{
									P0Sec += delta;
									if(isSimpleAdj) {
										P0 += TRANSITION_CELL_COEFF * lowresFaceMoveDir;
									}
								}
								if(v1 >= 0x9)
								{
									P1Sec += delta;
									if(isSimpleAdj) {
										P1 += TRANSITION_CELL_COEFF * lowresFaceMoveDir;
									}
								}
							}
							glm::vec4 Q = glm::vec4((float)t * P0 + (float)u * P1, 0.f);
							glm::vec4 QSec = glm::vec4((float)t * P0Sec + (float)u * P1Sec, 0.f);
							FloatInt adjacency;
							adjacency.asInt = adjacencyInfo;
							QSec.w = adjacency.asFloat;

							glm::vec3 normal = normalizeFixZero( N0 * (t/256.f) + N1 * (u/256.f) );

							block.TransitionMeshVertices[transitionId].push_back(Q);
							if((M0.Id == M1.Id) &&  (M0.Id == lowResCell.Material.Id)) {
								M0.Blend = Voxels::BlendFactor(((float)t * M0.Blend + (float)u * M1.Blend) / 256.f);
								block.TransitionMaterials[transitionId].push_back(M0);
							} else {
								block.TransitionMaterials[transitionId].push_back(lowResCell.Material);
							}

							block.TransitionMeshSecondaryVertices[transitionId].push_back(QSec);
							block.TransitionMeshNormals[transitionId].push_back(normal);

							auto index = block.TransitionMeshVertices[transitionId].size() - 1;
							cellIndices.push_back(index);

							if(addForReuse && reuseDirection == 8) {
								currentFilledRow[column].ReuseVertexIndices[reuseIndex] = index;
							}
						}
					}

					// Push all triangles
					for (auto v = 0L, triagCount = cellData.GetTriangleCount() * 3; v < triagCount; v += 3)
					{
						auto vId = cellIndices[cellData.vertexIndex[v]];
						assert(vId < block.TransitionMeshVertices[transitionId].size());
						block.TransitionMeshIndices[transitionId].push_back(vId);
						vId = cellIndices[cellData.vertexIndex[v + 1]];
						assert(vId < block.TransitionMeshVertices[transitionId].size());
						block.TransitionMeshIndices[transitionId].push_back(vId);
						vId	= cellIndices[cellData.vertexIndex[v + 2]];
						assert(vId < block.TransitionMeshVertices[transitionId].size());
						block.TransitionMeshIndices[transitionId].push_back(vId);

						if(shouldInvertWinding ^ reverseWinding[transitionId]) {
							const auto sz = block.TransitionMeshIndices[transitionId].size();
							std::swap(block.TransitionMeshIndices[transitionId][sz - 1]
									, block.TransitionMeshIndices[transitionId][sz - 2]);
						}
					}

					reuseValidityMask |= 0x1;
				}
				std::swap(previousFilledRow, currentFilledRow);
				std::for_each(currentFilledRow.begin(), currentFilledRow.end(), [](TransitionFilledCell& cell) { cell.Reset(); });
				
				reuseValidityMask |= 0x2;
			}
		}
	}
	
	const Voxels::VoxelGrid& m_Grid;

	glm::vec3 m_MaxExtents;
	std::vector<glm::vec3> m_BlockCounts;

	const MaterialMap* m_Materials;

	typedef std::vector<Block> BlocksVec;
	// TODO: load and keep in memory only the needed blocks
	BlocksVec m_LoadedBlocks;

	static __declspec(thread) GridBlocksCache* ts_BlocksCache;
	typedef concurrency::concurrent_unordered_map<int, GridBlocksCache*> GridBlockMap;
	GridBlockMap m_PerThreadCaches;

	ResultType* m_Result;

	ModificationType* m_Modification;
};

PolygonMap* TransVoxelImpl::Execute(const Voxels::VoxelGrid& grid, const MaterialMap* materials, Modification* modification)
{
#ifdef GRID_LIMIT
	if (grid.GetWidth() > GRID_LIMIT
		|| grid.GetHeight() > GRID_LIMIT
		|| grid.GetDepth() > GRID_LIMIT) {
		char buffer[VOXELS_LOG_SIZE];
		snprintf(buffer, VOXELS_LOG_SIZE, "Unable to polygonize grid. Grid extents are limited to %u in this version of the library.", GRID_LIMIT);
		VOXLOG(LS_Error, buffer);
		return nullptr;
	}
#endif

	TransVoxelRun run(grid, materials, static_cast<MapModification*>(modification));
	
	return run.Execute();
}

unsigned GetBlockExtent() {
	return BLOCK_EXTENT;
}

__declspec(thread) TransVoxelRun::GridBlocksCache* TransVoxelRun::ts_BlocksCache = nullptr;

}

