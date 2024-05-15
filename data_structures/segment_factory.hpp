#pragma once

#include "flash_data.hpp"
#include "segment.hpp"
#include "../layers/flash/flash.h"


class SegmentFactory
{
private:
	FlashData& flashData;

public:
	SegmentFactory(FlashData& fData) :
		flashData(fData)
	{
	}

	InMemorySegment * Build(unsigned int segmentNumber)
	{
		std::cout << "[SegmentFactory] Building Segment: " << segmentNumber << std::endl;

		unsigned int startSector         = segmentNumber * flashData.segmentSize * flashData.blockSize;
		unsigned int segmentSizeInBytes  = getSegmentSizeInBytes();
		unsigned int segmentSizeInBlocks = getSegmentSizeInBlocks();
		InMemorySegment * segment        = new InMemorySegment(segmentNumber, startSector, segmentSizeInBytes, segmentSizeInBlocks);
		return segment;
	}

	void Destroy(InMemorySegment * segment)
	{
		free(segment->data);
		free(segment->summary.blockINums);
		free(segment->summary.iNodeBlockNumbers);
		delete segment;
	}

	unsigned int getSegmentSizeInBytes()
	{
		return flashData.segmentSize * flashData.blockSize * FLASH_SECTOR_SIZE;
	}

	unsigned int getSegmentSizeInSectors()
	{
		return flashData.segmentSize * flashData.blockSize;
	}

	unsigned int getSegmentSizeInBlocks()
	{
		return flashData.segmentSize;
	}
};