#pragma once

#include <limits.h>

#define NO_INUM -1
#define SUMMARY_BLOCK -2

#define INDIRECT_BLOCK -1 
#define NO_BLOCK INT_MAX

typedef struct SegmentSummary
{
	unsigned int segmentNumber;
	unsigned int startSector;
	unsigned int numberOfBlocks;
	int *        blockINums;
	int *        iNodeBlockNumbers;

	SegmentSummary(unsigned int segNum, unsigned int sector, unsigned int nBlocks) :
		segmentNumber(segNum),
		startSector(sector),
		numberOfBlocks(nBlocks)
	{
		blockINums = (int *)malloc(numberOfBlocks * sizeof(int));
		memset(blockINums, NO_INUM, numberOfBlocks * sizeof(int));
		blockINums[0] = SUMMARY_BLOCK;

		iNodeBlockNumbers = (int *)malloc(numberOfBlocks * sizeof(int));
		iNodeBlockNumbers[0] = SUMMARY_BLOCK;
		for (int i = 1; i < numberOfBlocks; ++i)
		{
			iNodeBlockNumbers[i] = NO_BLOCK;
		}
	}

	void PrintSegmentSummaryBlock()
	{
		std::cout << "[SegmentSummary] Printing summary block" << std::endl;
		std::cout << "\t[SegmentSummary] Segment Number: "  << segmentNumber  << std::endl;
		std::cout << "\t[SegmentSummary] Start Sector: "    << startSector    << std::endl;
		std::cout << "\t[SegmentSummary] Numer of Blocks: " << numberOfBlocks << std::endl;
		std::cout << "\t[SegmentSummary] Block (inums, blockNums): \n\t{ ";

		for (int i = 0; i < numberOfBlocks; ++i)
		{
			std::cout << i << " : (" << blockINums[i] << ", " << iNodeBlockNumbers[i] << ") ";
		}

		std::cout << "} " << std::endl;
	}
} SegmentSummary;

typedef struct InMemorySegment
{
	SegmentSummary summary;
	void *         data; // maybe change to include summary

	InMemorySegment(unsigned int segmentNumber, unsigned int startSector, unsigned int segmentSizeInBytes, unsigned int segmentSizeInBlocks) :
		summary(segmentNumber, startSector, segmentSizeInBlocks)
    {
		data = malloc(segmentSizeInBytes); memset(data, 0, segmentSizeInBytes);
    }
} InMemorySegment;