// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#include "stdafx.h"

#include "VoxelGrid.h"
#include "../include/VoxelSurface.h"
#include <../dx11-framework/Utilities/MathInlines.h>

#define SETFLAG(Flag, Bit) ((Flag) |= (Bit))
#define UNSETFLAG(Flag, Bit) ((Flag) &= ~(Bit))

namespace Voxels
{

struct PackedGridImpl : public Grid::PackedGrid
{
	virtual void Destroy() override
	{
		delete this;
	}

	virtual unsigned GetSize() const override
	{
		return Data.size();
	}

	virtual const char* GetData() const override
	{
		return &Data[0];
	}

	std::vector<char> Data;
};


inline char round(float value)
{
	return char(StMath::min_value(StMath::max_value((float)std::numeric_limits<char>::min(), ceil(std::abs(value) /*+ 0.5f*/)) * (value > 0 ? 1 : -1), (float)std::numeric_limits<char>::max()));
}

inline char toGridDistValue(char value)
{
	static const char DIST_VALUE_BOUND = 4;
	if (value > DIST_VALUE_BOUND)
		return DIST_VALUE_BOUND;
	else if (value < -DIST_VALUE_BOUND)
		return -DIST_VALUE_BOUND;
	return value;
}

void VoxelGrid::PushBlock(unsigned& blockId,
	const std::vector<char>& blockData,
	const std::vector<unsigned char>& materialData,
	const std::vector<unsigned char>& blendData)
{
	Block newBlock;
	newBlock.InternalId = blockId++;
	newBlock.Flags = BF_None;
	bool isEmpty = false;
	if (!CompressBlock<char>(&blockData[0], newBlock.DistanceData, &isEmpty)) {
		SETFLAG(newBlock.Flags, BF_DistanceUncompressed);
	}
	if (isEmpty) {
		SETFLAG(newBlock.Flags, BF_Empty);
	}

	if (!CompressBlock<unsigned char>(&materialData[0], newBlock.MaterialData)) {
		SETFLAG(newBlock.Flags, BF_MaterialUncompressed);
	}

	if (!CompressBlock<unsigned char>(&blendData[0], newBlock.BlendData)) {
		SETFLAG(newBlock.Flags, BF_BlendUncompressed);
	}

	m_Blocks.push_back(std::move(newBlock));
}

VoxelGrid::VoxelGrid(unsigned w, unsigned d, unsigned h,
	float startX, float startY, float startZ, float step,
	VoxelSurface* surface)
	: m_Width(w)
	, m_Depth(d)
	, m_Height(h)
	, m_MemoryForBlocks(0)
{
	PROFI_FUNC

	// Create per-block data
	auto blocksX = m_Width / BLOCK_EXTENTS;
	auto blocksY = m_Depth / BLOCK_EXTENTS;
	auto blocksZ = m_Height / BLOCK_EXTENTS;

	const auto valuesCnt = BLOCK_EXTENTS*BLOCK_EXTENTS*BLOCK_EXTENTS;
	std::vector<char> blockData(valuesCnt);
	std::vector<unsigned char> materialData(valuesCnt);
	materialData.resize(valuesCnt);
	std::vector<unsigned char> blendData(valuesCnt);
	blendData.resize(valuesCnt);

	unsigned blockId = 0;
	std::unique_ptr<float[]> surfaceValues(new float[valuesCnt]);
	for (unsigned blZ = 0u; blZ < blocksY; ++blZ)
	for (unsigned blY = 0u; blY < blocksY; ++blY)
	for (unsigned blX = 0u; blX < blocksX; ++blX)
	{
		blockData.clear();

		const float xStartSurf = startX + blX * BLOCK_EXTENTS * step;
		const float xEndSurf = xStartSurf + BLOCK_EXTENTS* step;
		const float yStartSurf = startY + blY * BLOCK_EXTENTS * step;
		const float yEndSurf = yStartSurf + BLOCK_EXTENTS* step;
		const float zStartSurf = startZ + blZ * BLOCK_EXTENTS * step;
		const float zEndSurf = zStartSurf + BLOCK_EXTENTS* step;

		surface->GetSurface(xStartSurf, xEndSurf, step,
			yStartSurf, yEndSurf, step,
			zStartSurf, zEndSurf, step,
			surfaceValues.get(),
			&materialData[0],
			&blendData[0]);

		for (auto id = 0; id < BLOCK_EXTENTS*BLOCK_EXTENTS*BLOCK_EXTENTS; ++id)
		{
			blockData.push_back(toGridDistValue(round(surfaceValues[id])));
		}
		
		PushBlock(blockId, blockData, materialData, blendData);
	}

	RecalculateMemoryUsage();
}

VoxelGrid::VoxelGrid(unsigned w, unsigned d, unsigned h)
	: m_Width(w)
	, m_Depth(d)
	, m_Height(h)
	, m_MemoryForBlocks(0)
{
	PROFI_FUNC
	// Create per-block data
	auto blocksX = m_Width / BLOCK_EXTENTS;
	auto blocksY = m_Depth / BLOCK_EXTENTS;
	auto blocksZ = m_Height / BLOCK_EXTENTS;

	unsigned blockId = 0;
	for (unsigned blZ = 0u; blZ < blocksY; ++blZ)
	for (unsigned blY = 0u; blY < blocksY; ++blY)
	for (unsigned blX = 0u; blX < blocksX; ++blX)
	{

		Block newBlock;
		newBlock.InternalId = blockId++;
		newBlock.Flags = BF_None;
		m_Blocks.push_back(std::move(newBlock));
	}
}

VoxelGrid::VoxelGrid(unsigned w, const char* heightmap)
	: m_Width(w)
	, m_Depth(w)
	, m_Height(w)
	, m_MemoryForBlocks(0)
{
	PROFI_FUNC
	// Create per-block data
	auto blocksX = m_Width / BLOCK_EXTENTS;
	auto blocksY = m_Depth / BLOCK_EXTENTS;
	auto blocksZ = m_Height / BLOCK_EXTENTS;

	std::vector<char> blockData;
	std::vector<unsigned char> materialData;
	std::vector<unsigned char> blendData;

	unsigned blockId = 0;

	for (unsigned blZ = 0u; blZ < blocksY; ++blZ)
	for (unsigned blY = 0u; blY < blocksY; ++blY)
	for (unsigned blX = 0u; blX < blocksX; ++blX)
	{
		blockData.clear();
		materialData.clear();
		blendData.clear();
		
		const auto blockSlice = blZ * BLOCK_EXTENTS;
		const auto blockColumn = blX * BLOCK_EXTENTS;
		const auto blockRow = blY * BLOCK_EXTENTS;

		for (unsigned z = 0u; z < BLOCK_EXTENTS; ++z)
		{
			for (unsigned y = 0u; y < BLOCK_EXTENTS; ++y)
			{
				for (unsigned x = 0u; x < BLOCK_EXTENTS; ++x)
				{
					const auto slice = (blockSlice + z) - 127;
					const auto column = blockColumn + x;
					const auto row = blockRow + y;

					char height = (char)StMath::clamp_value<int>((int)slice - (int)heightmap[row * w + column], -127, 127);

					blockData.push_back(toGridDistValue(height));

					materialData.push_back(0);
					blendData.push_back(0);
				}
			}
		}

		PushBlock(blockId, blockData, materialData, blendData);
	}

	RecalculateMemoryUsage();
}

VoxelGrid* VoxelGrid::Load(const char* data)
{
	PROFI_FUNC
	const char* dataPtr = data;

	auto read = [&dataPtr](char* output, unsigned sz){
		::memcpy(output, dataPtr, sz);
		dataPtr += sz;
	};

	unsigned version, w, d, h;
	read((char*)&version, sizeof(version));
	if (version != CURRENT_FILE_VER)
	{
		VOXLOG(LS_Error, "Voxel grid file version not supported!");
		return nullptr;
	}

	read((char*)&w, sizeof(w));
	read((char*)&d, sizeof(d));
	read((char*)&h, sizeof(h));

	auto result = std::unique_ptr<VoxelGrid>(new VoxelGrid(w, d, h));
	const auto blocksCnt = result->m_Blocks.size();
	const auto dataRegionsCount = blocksCnt * 3;

	std::vector<unsigned> sizes;
	sizes.resize(dataRegionsCount);
	for (auto count = 0u; count < dataRegionsCount; ++count)
	{
		read((char*)&sizes[count], sizeof(unsigned));
	}

	for (auto id = 0u, sizeId = 0u; id < blocksCnt; ++id, sizeId += 3)
	{
		Block& block = result->m_Blocks[id];

		read((char*)&block.Flags, sizeof(BlockFlags));

		block.DistanceData.resize(sizes[sizeId]);
		read(&block.DistanceData[0], sizes[sizeId]);

		block.MaterialData.resize(sizes[sizeId + 1]);
		read((char*)&block.MaterialData[0], sizes[sizeId + 1]);

		block.BlendData.resize(sizes[sizeId + 2]);
		read((char*)&block.BlendData[0], sizes[sizeId + 2]);
	}

	result->RecalculateMemoryUsage();

	return result.release();
}

Grid::PackedGrid* VoxelGrid::PackForSave() const
{
	PROFI_FUNC
	std::unique_ptr<PackedGridImpl> pack(new PackedGridImpl);

	unsigned offset = 0;

	auto write = [&pack, &offset](const void* data, unsigned size) {
		pack->Data.resize(pack->Data.size() + size);
		::memcpy(&pack->Data[pack->Data.size() - size], (const char*)data, size);
		offset += size;
	};

	write(&CURRENT_FILE_VER, sizeof(CURRENT_FILE_VER));
	const auto w = GetWidth();
	const auto d = GetDepth();
	const auto h = GetHeight();
	write(&w, sizeof(w));
	write(&d, sizeof(d));
	write(&h, sizeof(h));

	// write the sizes
	unsigned blockSz = 0;
	for (auto block = m_Blocks.cbegin(); block != m_Blocks.cend(); ++block)
	{
		blockSz = block->DistanceData.size();
		write(&blockSz, sizeof(blockSz));

		blockSz = block->MaterialData.size();
		write(&blockSz, sizeof(blockSz));

		blockSz = block->BlendData.size();
		write(&blockSz, sizeof(blockSz));
	}

	// write all the data itself
	for (auto block = m_Blocks.cbegin(); block != m_Blocks.cend(); ++block)
	{
		write(&block->Flags, sizeof(BlockFlags));

		write(&block->DistanceData[0], block->DistanceData.size());
		write(&block->MaterialData[0], block->MaterialData.size());
		write(&block->BlendData[0], block->BlendData.size());
	}

	return pack.release();
}

bool IntersectBoxes(
	const glm::vec3& aMin,
	const glm::vec3& aMax,
	const glm::vec3& bMin,
	const glm::vec3& bMax)
{
	for (auto i = 0; i < 3; ++i)
	{
		if (aMin[i] > bMax[i] || bMin[i] > aMax[i])
			return false;
	}
	return true;
}

void VoxelGrid::IdentifyTouchedBlocks(const glm::vec3& position, const glm::vec3& extents, std::vector<TouchedBlock>& touchedBlocks)
{
	const glm::vec3 changeMin = position - extents;
	const glm::vec3 changeMax = position + extents;

	const auto blocksX = m_Width / BLOCK_EXTENTS;
	const auto blocksY = m_Depth / BLOCK_EXTENTS;
	const auto blocksZ = m_Height / BLOCK_EXTENTS;
	const auto blockExtDiv2 = float(BLOCK_EXTENTS >> 1);
	
	glm::vec3 blockExtents(blockExtDiv2);
	
	unsigned blockId = 0;
	for (unsigned blZ = 0u; blZ < blocksY; ++blZ)
	for (unsigned blY = 0u; blY < blocksY; ++blY)
	for (unsigned blX = 0u; blX < blocksX; ++blX)
	{
		const glm::vec3 blockCenter(blX * BLOCK_EXTENTS + blockExtDiv2,
									blY * BLOCK_EXTENTS + blockExtDiv2,
									blZ * BLOCK_EXTENTS + blockExtDiv2);

		const glm::vec3 blockMin = blockCenter - blockExtents;
		const glm::vec3 blockMax = blockCenter + blockExtents;

		if (IntersectBoxes(changeMin, changeMax, blockMin, blockMax))
		{
			const glm::vec3 blockBase(float(blX * BLOCK_EXTENTS),
										float(blY * BLOCK_EXTENTS),
										float(blZ * BLOCK_EXTENTS));
			BlockExtents blockExt = std::make_pair(blockBase, blockBase + glm::vec3((const float)BLOCK_EXTENTS));
			touchedBlocks.push_back(std::make_pair(blockId, blockExt));
		}

		++blockId;
	}
}

void VoxelGrid::CalculateTouchedBlockSection(
	const glm::vec3& position,
	const glm::vec3& extents,
	const BlockExtents& blockExt,
	glm::vec3& start,
	glm::vec3& end)
{
	const glm::vec3 extDiv2(extents.x / 2, extents.y / 2, extents.z / 2);
	const glm::vec3 initialChangePos = position - extDiv2;

	start = glm::vec3(StMath::clamp_value(initialChangePos.x, blockExt.first.x, blockExt.second.x) - blockExt.first.x,
					StMath::clamp_value(initialChangePos.y, blockExt.first.y, blockExt.second.y) - blockExt.first.y,
					StMath::clamp_value(initialChangePos.z, blockExt.first.z, blockExt.second.z) - blockExt.first.z);

	end = glm::vec3(StMath::clamp_value(initialChangePos.x + extents.x, blockExt.first.x, blockExt.second.x) - blockExt.first.x,
					StMath::clamp_value(initialChangePos.y + extents.y, blockExt.first.y, blockExt.second.y) - blockExt.first.y,
					StMath::clamp_value(initialChangePos.z + extents.z, blockExt.first.z, blockExt.second.z) - blockExt.first.z);

}

std::pair<glm::vec3, glm::vec3> VoxelGrid::InjectSurface(
	const glm::vec3& position,
	const glm::vec3& extents,
	VoxelSurface* surface,
	InjectionType type)
{
	PROFI_FUNC
	std::vector<TouchedBlock> touchedBlocks;
	IdentifyTouchedBlocks(position, extents, touchedBlocks);

	std::vector<char> newCompressed;
	char bytes[BLOCK_EXTENTS*BLOCK_EXTENTS*BLOCK_EXTENTS];
	
	glm::vec3 blockStart;
	glm::vec3 blockEnd;

	std::for_each(touchedBlocks.begin(), touchedBlocks.end(), 
		[&](TouchedBlock& touched)
	{
		Block& block = m_Blocks[touched.first];
		DecompressBlock<char>(&block.DistanceData[0], block.DistanceData.size(), !!(block.Flags & BF_DistanceUncompressed), bytes);

		const BlockExtents& blockExt = touched.second;

		CalculateTouchedBlockSection(position, extents, blockExt, blockStart, blockEnd);

		glm::vec3 surfaceCoordStart = blockExt.first + blockStart - position;
		glm::vec3 surfaceCoordEnd = blockExt.first + blockEnd - position;

		const auto valuesCnt = (unsigned)ceil(surfaceCoordEnd.x - surfaceCoordStart.x) *
								(unsigned)ceil(surfaceCoordEnd.y - surfaceCoordStart.y) *
								(unsigned)ceil(surfaceCoordEnd.z - surfaceCoordStart.z);
		std::unique_ptr<float[]> surfValues(new float[valuesCnt]);

		surface->GetSurface(surfaceCoordStart.x, surfaceCoordEnd.x, 1.f,
								surfaceCoordStart.y, surfaceCoordEnd.y, 1.f,
								surfaceCoordStart.z, surfaceCoordEnd.z, 1.f,
								surfValues.get(),
								nullptr,
								nullptr);

		unsigned surfValueId = 0;
		for (float z = blockStart.z; z < blockEnd.z; ++z)
		for (float y = blockStart.y; y < blockEnd.y; ++y)
		for (float x = blockStart.x; x < blockEnd.x; ++x)
		{
			const auto voxelId = VoxelIdInBlock(unsigned(x), unsigned(y), unsigned(z));
			const auto value = bytes[voxelId];

			char finalValue = 0;

			const float surfaceValue = surfValues[surfValueId++];

			switch (type)
			{
			case IT_Add:
				finalValue = round(StMath::min_value((float)value, surfaceValue));
				break;
			case IT_SubtractAddInner:
				finalValue = round(StMath::max_value((float)value, surfaceValue));
				break;
			case IT_Subtract:
				finalValue = round(StMath::max_value(-surfaceValue, (float)value));
				break;
			}

			bytes[voxelId] = finalValue;
		}

		m_MemoryForBlocks -= block.DistanceData.size();
		block.DistanceData.clear();
		
		bool isEmpty = false;
		if (!CompressBlock<char>(bytes, block.DistanceData, &isEmpty)) {
			SETFLAG(block.Flags, BF_DistanceUncompressed);
		}
		else {
			UNSETFLAG(block.Flags, BF_DistanceUncompressed);
		}
		m_MemoryForBlocks += block.DistanceData.size();

		if (isEmpty) {
			SETFLAG(block.Flags, BF_Empty);
		}
		else {
			UNSETFLAG(block.Flags, BF_Empty);
		}
	});
	
	const glm::vec3 initialChangePos = position - (extents / 2.0f);
	
	// DX- style coords!
	const auto globalStart = glm::vec3(StMath::max_value(0.f, initialChangePos.x),
		StMath::max_value(0.f, initialChangePos.z),
		StMath::max_value(0.f, initialChangePos.y));
	const auto globalEnd = glm::vec3(StMath::min_value(float(m_Width), globalStart.x + extents.x),
		StMath::min_value(float(m_Height), globalStart.y + extents.z),
		StMath::min_value(float(m_Depth), globalStart.z + extents.y));

	return std::make_pair(globalStart, globalEnd);
}

std::pair<glm::vec3, glm::vec3> VoxelGrid::InjectMaterial(
	const glm::vec3& position,
	const glm::vec3& extents,
	MaterialId material,
	bool addSubtractBlend)
{
	PROFI_FUNC
	std::vector<TouchedBlock> touchedBlocks;
	IdentifyTouchedBlocks(position, extents, touchedBlocks);

	const glm::vec3 extDiv2(extents / 2.0f);
	const glm::vec3 extDivCoeff = extDiv2 * 0.75f;

	std::vector<unsigned char> newMaterialCompressed;
	std::vector<unsigned char> newBlendCompressed;
	unsigned char materialBytes[BLOCK_EXTENTS*BLOCK_EXTENTS*BLOCK_EXTENTS];
	unsigned char blendBytes[BLOCK_EXTENTS*BLOCK_EXTENTS*BLOCK_EXTENTS];
			
	glm::vec3 blockStart;
	glm::vec3 blockEnd;
	std::for_each(touchedBlocks.begin(), touchedBlocks.end(),
		[&](TouchedBlock& touched)
	{
		Block& block = m_Blocks[touched.first];
		DecompressBlock<unsigned char>(&block.MaterialData[0],
			block.MaterialData.size(),
			!!(block.Flags & BF_MaterialUncompressed),
			materialBytes);
		DecompressBlock<unsigned char>(&block.BlendData[0],
			block.BlendData.size(),
			!!(block.Flags & BF_BlendUncompressed),
			blendBytes);

		const BlockExtents& blockExt = touched.second;

		CalculateTouchedBlockSection(position, extents, blockExt, blockStart, blockEnd);

		for (float z = blockStart.z; z < blockEnd.z; ++z)
		for (float y = blockStart.y; y < blockEnd.y; ++y)
		for (float x = blockStart.x; x < blockEnd.x; ++x)
		{
			glm::vec3 currentPosition = glm::vec3(x + blockExt.first.x,
				y + blockExt.first.y,
				z + blockExt.first.z);
			
			const auto voxelId = VoxelIdInBlock(unsigned(x), unsigned(y), unsigned(z));
			const auto dist = glm::length(currentPosition - position) / extDivCoeff.x;

			auto outputBlend = (unsigned char)(StMath::min_value(1.f, StMath::max_value(0.f, (1 - dist))) * 255.f);

			auto& currentMaterial = materialBytes[voxelId];
			auto& currentBlend = blendBytes[voxelId];

			if (currentMaterial == material) {
				currentBlend = StMath::max_value(0, StMath::min_value(255, (addSubtractBlend ? 1 : -1) * outputBlend + currentBlend));
			}
			else {
				currentMaterial = material;
				currentBlend = outputBlend;
			}
		}

		m_MemoryForBlocks -= block.MaterialData.size();
		block.MaterialData.clear();
		if (!CompressBlock<unsigned char>(materialBytes, block.MaterialData)) {
			SETFLAG(block.Flags, BF_MaterialUncompressed);
		}
		else {
			UNSETFLAG(block.Flags, BF_MaterialUncompressed);
		}
		m_MemoryForBlocks += block.MaterialData.size();

		m_MemoryForBlocks -= block.BlendData.size();
		block.BlendData.clear();
		if (!CompressBlock<unsigned char>(blendBytes, block.BlendData)) {
			SETFLAG(block.Flags, BF_BlendUncompressed);
		}
		else {
			UNSETFLAG(block.Flags, BF_BlendUncompressed);
		}
		m_MemoryForBlocks += block.BlendData.size();
	});

	const glm::vec3 initialChangePos = position - extDiv2;

	// DX- style coords!
	const auto globalStart = glm::vec3(StMath::max_value(0.f, initialChangePos.x),
		StMath::max_value(0.f, initialChangePos.z),
		StMath::max_value(0.f, initialChangePos.y));
	const auto globalEnd = glm::vec3(StMath::min_value(float(m_Width), globalStart.x + extents.x),
		StMath::min_value(float(m_Height), globalStart.y + extents.z),
		StMath::min_value(float(m_Depth), globalStart.z + extents.y));

	return std::make_pair(globalStart, globalEnd);
}

void VoxelGrid::GetBlockData(const glm::vec3& blockCoords, char* output) const
{
	const auto id = CalculateInternalBlockId(blockCoords);
	const Block& block = m_Blocks[id];
	
	DecompressBlock<char>(&block.DistanceData[0], block.DistanceData.size(), !!(block.Flags & BF_DistanceUncompressed), output);
}

void VoxelGrid::GetMaterialBlockData(const glm::vec3& blockCoords, unsigned char* materialOutput, unsigned char* blendOutput) const
{
	const auto id = CalculateInternalBlockId(blockCoords);
	const Block& matBlock = m_Blocks[id];
	
	DecompressBlock<unsigned char>(&matBlock.MaterialData[0], matBlock.MaterialData.size(), !!(matBlock.Flags & BF_MaterialUncompressed), materialOutput);
	
	DecompressBlock<unsigned char>(&matBlock.BlendData[0], matBlock.BlendData.size(), !!(matBlock.Flags & BF_BlendUncompressed), blendOutput);
}

bool VoxelGrid::IsBlockEmpty(const glm::vec3& blockCoords) const
{
	const auto id = CalculateInternalBlockId(blockCoords);
	return !!(m_Blocks[id].Flags & BF_Empty);
}

template<typename Type>
bool VoxelGrid::CompressBlock(const Type* data, std::vector<Type>& compressed, bool* const isEmpty)
{
	const auto sz = BLOCK_EXTENTS * BLOCK_EXTENTS * BLOCK_EXTENTS;

	compressed.reserve(sz / 2); // speculate 50% compression
	unsigned counter = 0;
	compressed.push_back(0);
	Type* outputCtr = &compressed[compressed.size() - 1];

	bool compressionEffective = true;

	if (isEmpty)
		*isEmpty = true;

	int initalByte = data[0];
	Type lastByte = data[0];
	for (auto byte = 0u; byte < sz; ++byte)
	{
		auto currentByte = data[byte];
		if (lastByte == currentByte && counter < 0xFF)
		{
			++counter;
		}
		else
		{
			*reinterpret_cast<unsigned char*>(outputCtr) = static_cast<unsigned char>(counter);
			compressed.push_back(lastByte);
			compressed.push_back(0);
			outputCtr = &compressed[compressed.size() - 1];
			counter = 1;

			lastByte = currentByte;

			const int sign = initalByte*lastByte;
			if (sign <= 0 && isEmpty)
			{
				*isEmpty = false;
			}

			if (compressed.size() > sz)
			{
				// the compression actually pessimizes the data size
				compressionEffective = false;
				break;
			}
		}
	}

	if (compressionEffective) {
		*reinterpret_cast<unsigned char*>(outputCtr) = static_cast<unsigned char>(counter);
		compressed.push_back(lastByte);
		compressed.shrink_to_fit();
		return true;
	}
	else {
		if (isEmpty)
			*isEmpty = false;
		compressed.resize(sz);
		::memcpy(&compressed[0], data, sz);
		return false;
	}
}

template<typename Type>
void VoxelGrid::DecompressBlock(const Type* data, unsigned sz, bool isUncompressed, Type* output)
{
	if (!isUncompressed) {
		unsigned char length = 0;
		Type value = 0;

		Type* outPtr = output;
		for (auto id = 0u; id < sz; id += 2)
		{
			length = reinterpret_cast<const unsigned char*>(data)[id];
			value = data[id + 1];

			std::fill(output, output + length, value);
			output += length;
		}
	}
	else {
		::memcpy(output, data, sz);
	}
}

void VoxelGrid::ModifyBlockDistanceData(const glm::vec3& coords, const char* distances)
{
	const auto id = CalculateInternalBlockId(coords);
	Block& block = m_Blocks[id];
	m_MemoryForBlocks -= block.DistanceData.size();
	block.DistanceData.clear();
	bool isEmpty = false;
	if (!CompressBlock<char>(distances, block.DistanceData, &isEmpty)) {
		SETFLAG(block.Flags, BF_DistanceUncompressed);
	}
	else {
		UNSETFLAG(block.Flags, BF_DistanceUncompressed);
	}
	m_MemoryForBlocks += block.DistanceData.size();
	if (isEmpty) {
		SETFLAG(block.Flags, BF_Empty);
	}
	else {
		UNSETFLAG(block.Flags, BF_Empty);
	}
}

void VoxelGrid::ModifyBlockMaterialData(const glm::vec3& coords, const MaterialId* materials, const BlendFactor* blends)
{
	const auto id = CalculateInternalBlockId(coords);
	Block& block = m_Blocks[id];
	m_MemoryForBlocks -= block.MaterialData.size();
	block.MaterialData.clear();
	if (!CompressBlock<unsigned char>(materials, block.MaterialData)) {
		SETFLAG(block.Flags, BF_MaterialUncompressed);
	}
	else {
		UNSETFLAG(block.Flags, BF_MaterialUncompressed);
	}
	m_MemoryForBlocks += block.MaterialData.size();

	m_MemoryForBlocks -= block.BlendData.size();
	block.BlendData.clear();
	if (!CompressBlock<unsigned char>(blends, block.BlendData)) {
		SETFLAG(block.Flags, BF_BlendUncompressed);
	}
	else {
		UNSETFLAG(block.Flags, BF_BlendUncompressed);
	}
	m_MemoryForBlocks += block.BlendData.size();
}

void VoxelGrid::RecalculateMemoryUsage()
{
	std::for_each(m_Blocks.cbegin(), m_Blocks.cend(), [this](const Block& block) {
		m_MemoryForBlocks += block.DistanceData.size();
		m_MemoryForBlocks += block.BlendData.size();
		m_MemoryForBlocks += block.MaterialData.size();
	});
}

///////////////////////////////////////////////////////////////
/// PUBLIC INTERFACE
///////////////////////////////////////////////////////////////
Grid::Grid(VoxelGrid* impl)
	: m_InternalGrid(impl)
{}

Grid* Grid::Create(unsigned w, unsigned d, unsigned h,
	float startX, float startY, float startZ, float step,
	VoxelSurface* surface)
{
#ifdef GRID_LIMIT
	if (w > GRID_LIMIT || d > GRID_LIMIT || h > GRID_LIMIT) {
		char buffer[VOXELS_LOG_SIZE];
		snprintf(buffer, VOXELS_LOG_SIZE, "Unable to create grid. Grid extents are limited to %u in this version of the library.", GRID_LIMIT);
		VOXLOG(LS_Error, buffer);
		return nullptr;
	}
#endif
	auto impl = std::unique_ptr<VoxelGrid>(new VoxelGrid(w, d, h, startX, startY, startZ, step, surface));
	return new Grid(impl.release());
}

Grid* Grid::Create(unsigned w, unsigned d, unsigned h)
{
#ifdef GRID_LIMIT
	if (w > GRID_LIMIT || d > GRID_LIMIT || h > GRID_LIMIT) {
		char buffer[VOXELS_LOG_SIZE];
		snprintf(buffer, VOXELS_LOG_SIZE, "Unable to create grid. Grid extents are limited to %u in this version of the library.", GRID_LIMIT);
		VOXLOG(LS_Error, buffer);
		return nullptr;
	}
#endif
	auto impl = std::unique_ptr<VoxelGrid>(new VoxelGrid(w, d, h));
	return new Grid(impl.release());
}

Grid* Grid::Create(unsigned w, const char* heightmap)
{
#ifdef GRID_LIMIT
	if (w > GRID_LIMIT) {
		char buffer[VOXELS_LOG_SIZE];
		snprintf(buffer, VOXELS_LOG_SIZE, "Unable to create grid. Grid extents are limited to %u in this version of the library.", GRID_LIMIT);
		VOXLOG(LS_Error, buffer);
		return nullptr;
	}
#endif
	auto impl = std::unique_ptr<VoxelGrid>(new VoxelGrid(w, heightmap));
	return new Grid(impl.release());
}

Grid* Grid::Load(const char* blob, unsigned size)
{
	return new Grid(VoxelGrid::Load(blob));
}

void Grid::Destroy()
{
	delete this;
}

Grid::~Grid()
{
	if (m_InternalGrid)
	{
		delete m_InternalGrid;
	}
}

Grid::PackedGrid* Grid::PackForSave() const
{
	return m_InternalGrid->PackForSave();
}

unsigned Grid::GetWidth() const
{
	return m_InternalGrid->GetWidth();
}

unsigned Grid::GetDepth() const
{
	return m_InternalGrid->GetDepth();
}

unsigned Grid::GetHeight() const
{
	return m_InternalGrid->GetHeight();
}

float3pair toFloat3Pair(const std::pair<glm::vec3, glm::vec3>& resultInternal)
{
	float3pair result;
	result.first.x = resultInternal.first.x;
	result.first.y = resultInternal.first.y;
	result.first.z = resultInternal.first.z;
	result.second.x = resultInternal.second.x;
	result.second.y = resultInternal.second.y;
	result.second.z = resultInternal.second.z;

	return result;
}

float3pair Grid::InjectSurface(const float3& position,
	const float3& extents,
	VoxelSurface* surface,
	InjectionType type)
{
	const auto internalPos = glm::vec3(position.x, position.y, position.z);
	const auto internalExt = glm::vec3(extents.x, extents.y, extents.z);
	auto resultInternal =
		m_InternalGrid->InjectSurface(internalPos, internalExt, surface, type);

	return toFloat3Pair(resultInternal);
}

float3pair Grid::InjectMaterial(const float3& position,
	const float3& extents,
	MaterialId material,
	bool addSubtractBlend)
{
	const auto internalPos = glm::vec3(position.x, position.y, position.z);
	const auto internalExt = glm::vec3(extents.x, extents.y, extents.z);
	auto resultInternal =
		m_InternalGrid->InjectMaterial(internalPos, internalExt, material, addSubtractBlend);

	return toFloat3Pair(resultInternal);
}

unsigned Grid::GetBlockExtent() const
{
	return VoxelGrid::BLOCK_EXTENTS;
}

bool Grid::GetBlockDistanceData(const float3& coords, char* output) const
{
	const auto internalCoords = glm::vec3(coords.x, coords.y, coords.z);
	m_InternalGrid->GetBlockData(internalCoords, output);
	return true;
}

void Grid::ModifyBlockDistanceData(const float3& coords, const char* distances)
{
	const auto internalCoords = glm::vec3(coords.x, coords.y, coords.z);
	m_InternalGrid->ModifyBlockDistanceData(internalCoords, distances);
}

bool Grid::GetBlockMaterialData(const float3& coords, MaterialId* materials, BlendFactor* blends) const
{
	const auto internalCoords = glm::vec3(coords.x, coords.y, coords.z);
	m_InternalGrid->GetMaterialBlockData(internalCoords, materials, blends);
	return true;
}

void Grid::ModifyBlockMaterialData(const float3& coords, const MaterialId* materials, const BlendFactor* blends)
{
	const auto internalCoords = glm::vec3(coords.x, coords.y, coords.z);
	m_InternalGrid->ModifyBlockMaterialData(internalCoords, materials, blends);
}

VoxelGrid* Grid::GetInternalRepresentation() const
{
	return m_InternalGrid;
}

unsigned Grid::GetGridBlocksMemorySize()
{
	return unsigned(m_InternalGrid->MemoryForGrid());
}

}