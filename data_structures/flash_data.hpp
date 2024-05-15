#pragma once

#include <time.h>
#include "inode.hpp"

#define CHECKPOINT_SIZE_IN_SECTORS 1
#define FLASH_FULL -9

typedef struct SegmentUsageTableEntry
{
	unsigned int liveBytesInSegment;
	time_t       ageOfYoungestBlock;
} SegmentUsageTableEntry;

typedef struct Checkpoint
{
	bool 		       isValid;
	unsigned long long time;
	unsigned int       segmentUsageTableSegment;
	unsigned int       lastSegmentWritten;
	INode              iFileINode;
} Checkpoint; 

typedef struct FlashData
{
	unsigned int blockSize;         // Size of a block, in sectors
	unsigned int segmentSize;       // Segment size, in blocks. The segment size must be a multiple of the flash erase block size
	unsigned int flashSize;         // Size of the flash, in segments
	unsigned int wearLimit;         // Wear limit for erase blocks
	unsigned int numBlocks;         // total number of blocks in flash
	unsigned int checkpointSegment; // reserved checkpoint segment
} FlashData;
