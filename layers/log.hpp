#pragma once

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <iostream>
#include <vector>
#include <sys/statvfs.h>
#include "flash/flash.h"
#include "../data_structures/flash_data.hpp"
#include "../data_structures/segment.hpp"
#include "../data_structures/segment_factory.hpp"
#include "../data_structures/segment_cache.hpp"
#include "../data_structures/log_address.hpp"
#include "../data_structures/inode.hpp"
#include "../utils.hpp"

class ILog
{
public:
    virtual ~ILog(){}
	virtual int Init() = 0;
	virtual int Log_Statfs(struct statvfs * stbuf) = 0;
	virtual int Log_Read(LogAddress logAddress, void *buffer) = 0;
	virtual int Log_Write(unsigned int inum, unsigned int fileBlock, void * buffer, LogAddress * logAddress) = 0;
	virtual int Log_Free(LogAddress logAddress) = 0;
	virtual void UpdateIFileINode(INode newIFileINode) = 0;
	virtual INode GetIFileINode() = 0;
	virtual unsigned int GetFileBlockSizeInBytes() = 0;
	virtual unsigned int GetFlashSize() = 0;
	virtual unsigned int GetFirstSegment() = 0;
	virtual SegmentUsageTableEntry * ReadSegmentUsageTable() = 0;
	virtual int WriteSegmentUsageTable(SegmentUsageTableEntry * table) = 0;
	virtual InMemorySegment * ReadSegment(unsigned int segmentNumber) = 0;
	virtual void FreeSegment(InMemorySegment * segment) = 0;
	virtual int EraseSegment(unsigned int segment) = 0;
	virtual int InvalidateSegment(unsigned int segment) = 0;
	virtual void PrintTailSummary() = 0;
	virtual void PrintSegmentUsageTable(SegmentUsageTableEntry * table) = 0;

};

class Log : public ILog
{
private:	
	char *                   flashFile; 
	unsigned int             segmentCacheSize;
	unsigned int             checkpointInterval;
	unsigned int             writesSinceLastCheckpoint;
	Flash                    flash;
	FlashData                flashData;
	SegmentUsageTableEntry * segmentUsageTable;
	SegmentFactory         * segmentFactory;
	InMemorySegment        * tailSegment;
	SegmentCache           * segmentCache;
	Checkpoint               checkpoint;
	unsigned int 	         checkpointSector;
	INode 			         iFileINode;
	bool                     recoveredWithPartialSegment; // if we recover from a partial segment we need to erase it before writing it back when its full

public:
	Log(char * f, unsigned int cacheSize, unsigned int ckptInterval) :
		flashFile(f),
		segmentCacheSize(cacheSize),
		checkpointInterval(ckptInterval),
		writesSinceLastCheckpoint(0),
		recoveredWithPartialSegment(false)
	{
	}

	~Log()
	{
		CheckpointNow();
		segmentFactory->Destroy(tailSegment);
		delete segmentCache;
		delete segmentFactory;
		free(segmentUsageTable);
		Flash_Close(flash);
	}

	int Init()
	{
		// open flash
    	Flash_Flags flash_flags = FLASH_SILENT | FLASH_ASYNC; 
    	unsigned int blocks;

    	flash = Flash_Open(flashFile, flash_flags, &blocks);
	    if (flash == NULL)
	    {
	        std::cerr << "[LogLayer] ERROR: Unable to open flash on LogInit" << std::endl;
	        std::cerr << "[LogLayer] errno: " << errno << std::endl;
	        return 1;
	    }

	    // read flash data fields
	    unsigned int flashDataSizeInSectors = sizeof(FlashData) / FLASH_SECTOR_SIZE;
	    if (sizeof(FlashData) % FLASH_SECTOR_SIZE != 0 || flashDataSizeInSectors == 0)
	    {
	    	flashDataSizeInSectors++;
	    }

	    unsigned int flashDataBufferSize = flashDataSizeInSectors * FLASH_SECTOR_SIZE;
	    void * flashDataBuffer           = malloc(flashDataBufferSize);
	    memset(flashDataBuffer, 0, flashDataBufferSize);
		if(Flash_Read(flash, 0, flashDataSizeInSectors, flashDataBuffer) != 0)
		{
	        std::cerr << "[LogLayer] ERROR: Unable to read flash on LogInit" << std::endl;
	        std::cerr << "[LogLayer] errno: " << errno << std::endl;
	        return 1;
		}

		flashData = *reinterpret_cast<FlashData *>(flashDataBuffer);
		free(flashDataBuffer);

		PrintInitData();

		// copy checkpoint region
		memset(&checkpoint, 0, sizeof(Checkpoint));
	    RecoverCheckpoint(); 
	    iFileINode = checkpoint.iFileINode;

		// init data structs
		std::cout << "[LogLayer] initializing log data structures..." << std::endl;
		segmentFactory = new SegmentFactory(flashData);
		segmentCache   = new SegmentCache(segmentFactory, segmentCacheSize); 

		unsigned int lastSegmentWritten = checkpoint.lastSegmentWritten;
		tailSegment                     = segmentFactory->Build(lastSegmentWritten);
		
		if(readSegment(tailSegment) != 0)
		{
			std::cerr << "[LogLayer] ERROR: Unable to read lastSegmentWritten from flash on log init" << std::endl;
		    std::cerr << "[LogLayer] segment number: " << lastSegmentWritten << std::endl;
        	std::cerr << "[LogLayer] errno: " << strerror(errno) << std::endl;
        	return 1;
		};

		// read segment usage table
		segmentUsageTable = ReadSegmentUsageTable();
		unsigned int tailSegmentNumber = GetCleanSegment();

		if (tailSegment->summary.blockINums[flashData.segmentSize - 1] != NO_INUM)
		{
			segmentFactory->Destroy(tailSegment);
		    tailSegment = segmentFactory->Build(tailSegmentNumber);
		}
		else
		{
			recoveredWithPartialSegment = true;
		}

		std::cout << "[LogLayer] tail segment segment number: " << tailSegment->summary.segmentNumber << std::endl;
		return 0;
	}

	int Log_Statfs(struct statvfs* stbuf)
	{
		stbuf->f_bsize   = flashData.blockSize * FLASH_SECTOR_SIZE;                         /* file system block size */
	    stbuf->f_frsize  = flashData.segmentSize * flashData.blockSize * FLASH_SECTOR_SIZE; /* fragment size */
	    stbuf->f_blocks  = flashData.flashSize;                                             /* size of fs in f_frsize units */
	    
		unsigned int freeBlocks = 0;
		for (int segment = flashData.checkpointSegment + 1; segment < flashData.flashSize; segment++)
		{
			unsigned int liveBytes = segmentUsageTable[segment].liveBytesInSegment;
			unsigned int liveBlocks = liveBytes / (flashData.blockSize * FLASH_SECTOR_SIZE);
			unsigned int freeBlockInSegment = flashData.segmentSize - liveBlocks;
			freeBlocks += freeBlockInSegment;
		}

	    stbuf->f_bfree   = freeBlocks; /* # free blocks */ // TODO: change for phase 2 ot be accurate???
	    stbuf->f_bavail  = freeBlocks; /* # free blocks for unprivileged users */ // TODO: change in phase 2
	    return 0;
	}

	int Log_Read(LogAddress logAddress, void * buffer)
	{
		std::cout << "[LogLayer] Reading log address segment number: " << logAddress.logSegment << " block Number: " << logAddress.blockNumber << std::endl;

		// check valid params
		if (!ValidLogAddress(logAddress))
		{
	        std::cerr << "[LogLayer] ERROR: Attempting to read invalid log address" << std::endl;
	        std::cerr << "[LogLayer] segment number: " << logAddress.logSegment << std::endl;
	        std::cerr << "[LogLayer] block number: " << logAddress.blockNumber << std::endl;
			return 1;
		}

		// get segment
		unsigned int segmentNumber      = logAddress.logSegment;
		InMemorySegment * segmentToRead = getSegment(segmentNumber);
		if (segmentToRead == NULL)
		{
	        std::cerr << "[LogLayer] ERROR: Cant read segment: " << segmentNumber << std::endl;
			return 1;
		}
		
		// check if we are reading dead blocks (just report for now)
		unsigned int blockNumber = logAddress.blockNumber;
		if (segmentToRead->summary.blockINums[blockNumber] == NO_INUM)
		{
		    std::cerr << "[LogLayer] ERROR: Attempting to reading dead block " << std::endl;
		    std::cerr << "[LogLayer] Segment Number: " << segmentNumber << std::endl;
		    std::cerr << "[LogLayer] Block Number: " << blockNumber << std::endl;
		    throw;
		    //return 1;
		}

		char * dataToRead              = (char *)segmentToRead->data;
		unsigned int segmentDataOffset = (blockNumber - 1) * flashData.blockSize * FLASH_SECTOR_SIZE; // -1 because we are reading from segment data which starts at block 1
		memcpy(buffer, dataToRead + segmentDataOffset, flashData.blockSize * FLASH_SECTOR_SIZE);
		return 0;
	}

	int Log_Write(unsigned int inum, unsigned int fileBlock, void * buffer, LogAddress * logAddress)
	{
		std::cout << "[LogLayer] Writing inum: " << inum << " block Number: " << fileBlock << std::endl;

		SegmentSummary& tailSegmentSummary = tailSegment->summary;

		if (tailSegmentSummary.segmentNumber >= flashData.flashSize)
		{
       		std::cerr << "[LogLayer] ERROR: FLASH IS FULL CANNOT WRITE" << std::endl;
			return 1;
		}

		unsigned int emptyBlock = 0;
		for (int b = 1; b < segmentFactory->getSegmentSizeInBlocks(); ++b)
		{
			if (tailSegmentSummary.blockINums[b] == NO_INUM)
			{
				emptyBlock = b;
				break;
			}
		}

		if (emptyBlock == 0)
		{
        	std::cerr << "[LogLayer] ERROR: No empty block in tail segment" << std::endl;
        	throw;
		}

		logAddress->logSegment                           = tailSegmentSummary.segmentNumber; 
		logAddress->blockNumber                          = emptyBlock;
		tailSegmentSummary.blockINums[emptyBlock]        = inum;
		tailSegmentSummary.iNodeBlockNumbers[emptyBlock] = fileBlock;
		
		// copy bytes to tail segment
		char * tailBuffer               = (char *)tailSegment->data;
		unsigned int tailBufferTailByte = (emptyBlock - 1) * flashData.blockSize * FLASH_SECTOR_SIZE; // USE RIGHT BLOCK. subtract 1
		memcpy(tailBuffer + tailBufferTailByte, buffer, flashData.blockSize * FLASH_SECTOR_SIZE);

		// update segment usage table
		//segmentUsageTable[tailSegmentSummary.segmentNumber].ageOfYoungestBlock = time(0);
		//segmentUsageTable[tailSegmentSummary.segmentNumber].liveBytesInSegment += flashData.blockSize * FLASH_SECTOR_SIZE;

		if (emptyBlock == segmentFactory->getSegmentSizeInBlocks() - 1)
		{
			if (recoveredWithPartialSegment)
			{	
				if(eraseSegment(tailSegment->summary.segmentNumber) != 0) // what if we erase and crash before write? use flag
				{
					std::cerr << "[LogLayer] ERROR: Unable to erase segment" << std::endl;
				    std::cerr << "[LogLayer] segment number: " << tailSegment->summary.segmentNumber << std::endl;
		        	std::cerr << "[LogLayer] errno: " << strerror(errno) << std::endl;
		        	return 1;
				}

				recoveredWithPartialSegment = false;
			}

			if(writeSegment(tailSegment) != 0)
			{
       			std::cerr << "[LogLayer] ERROR: error writing segment to flash" << std::endl;
        		std::cerr << "[LogLayer] errno: " << strerror(errno) << std::endl;		
        		return 1;
			}

			// add filled tail segment to segment cache
       		std::cout << "[LogLayer] adding tail segment " << tailSegmentSummary.segmentNumber << " to cache" << std::endl;
			segmentCache->putEntry(tailSegment);

			// make new tail segment
			unsigned int tailSegmentNumber = GetCleanSegment();
			tailSegment = segmentFactory->Build(tailSegmentNumber);
			//segmentUsageTable[tailSegmentNumber].liveBytesInSegment = 0;
			//segmentUsageTable[tailSegmentNumber].ageOfYoungestBlock = 0;
			//WriteSegmentUsageTable(segmentUsageTable);

			if (writesSinceLastCheckpoint >= checkpointInterval)
			{
				CheckpointNow();
			}
		}

		std::cout << "[LogLayer] New segment: " << logAddress->logSegment << std::endl;
		std::cout << "[LogLayer] New block: " << logAddress->blockNumber << std::endl;
		return 0;
	}

	int Log_Free(LogAddress logAddress)
	{
		if (logAddress.logSegment == getTailSegmentNumber())
		{
			tailSegment->summary.blockINums[logAddress.blockNumber]        = NO_INUM;
			tailSegment->summary.iNodeBlockNumbers[logAddress.blockNumber] = NO_BLOCK;
		}

		if (logAddress.logSegment != EMPTY_DIRECT_BLOCK_ADDRESS)
		{
			segmentUsageTable[logAddress.logSegment].liveBytesInSegment -= flashData.blockSize * FLASH_SECTOR_SIZE;
		}

		return 0;
	}

	unsigned int GetFileBlockSizeInBytes()
	{
		return flashData.blockSize * FLASH_SECTOR_SIZE;
	}

	unsigned int GetFlashSize()
	{
		return flashData.flashSize;
	}

	unsigned int GetFirstSegment()
	{
		return flashData.checkpointSegment + 1;
	}

	// only want to update the ifile inode in the checkpoint when a segment is written. rethink this
	void UpdateIFileINode(INode newIFileINode)
	{
		iFileINode = newIFileINode;
	}

	INode GetIFileINode()
	{
		return iFileINode;
	}

	unsigned int getTailSegmentNumber()
	{
		return tailSegment->summary.segmentNumber;
	}

	void PrintTailSummary()
	{
		std::cout << "[LogLayer] Printing tail summary block" << std::endl;
		tailSegment->summary.PrintSegmentSummaryBlock();
	}

	void PrintTail()
	{
		std::cout << "[LogLayer] Printing log tail data" << std::endl;

		char * tailData = (char * )tailSegment->data;
		for (int i = 0; i < segmentFactory->getSegmentSizeInBytes(); ++i)
		{
			std::cout << tailData[i];
		}

		std::cout << "" << std::endl;
	}

	SegmentUsageTableEntry * ReadSegmentUsageTable()
	{
		unsigned int size = flashData.flashSize * sizeof(SegmentUsageTableEntry);
		SegmentUsageTableEntry * table = (SegmentUsageTableEntry *)malloc(size);
		memset(table, 0, size);

	    unsigned int bufferSize  = segmentFactory->getSegmentSizeInBytes();
	    void * buffer            = malloc(bufferSize);
	    unsigned int segmentSize = segmentFactory->getSegmentSizeInSectors();
	    memset(buffer, 0, bufferSize);

		if(Flash_Read(flash, checkpoint.segmentUsageTableSegment * segmentSize, segmentSize, buffer) != 0)
		{
	        std::cerr << "[LogLayer] ERROR: Unable to read flash on ReadSegmentUsageTable" << std::endl;
	        std::cerr << "[LogLayer] errno: " << errno << std::endl;
	        return NULL;
		}

		memcpy(table, buffer, size);
		free(buffer);
		return table;
	}

	int WriteSegmentUsageTable(SegmentUsageTableEntry * table)
	{
		eraseSegment(checkpoint.segmentUsageTableSegment);

		unsigned int size = flashData.flashSize * sizeof(SegmentUsageTableEntry);
	    unsigned int bufferSize = segmentFactory->getSegmentSizeInBytes();
	    void * buffer = malloc(bufferSize);
	    memset(buffer, 0, bufferSize);
	    memcpy(buffer, table, size);
	    unsigned int segmentSize = segmentFactory->getSegmentSizeInSectors();

	    if (Flash_Write(flash, checkpoint.segmentUsageTableSegment * segmentSize, segmentFactory->getSegmentSizeInSectors(), buffer) != 0)
	    {
	        std::cerr << "Unable to write segment usage table on WriteSegmentUsageTable()" << std::endl;
	        std::cerr << "errno: " << errno << std::endl;
	        return 1;
	    }

		return 0;
	}

	void PrintSegmentUsageTable(SegmentUsageTableEntry * table)
	{
		std::cout << "[SegmentUsageTable] printing SegmentUsageTable" << std::endl;
		for (int i = 0; i < flashData.flashSize; ++i)
		{
			std::cout << "[SegmentUsageTable] segment: " << i << std::endl;

			std::cout << "\t[SegmentUsageTable] liveBytesInSegment: " << table[i].liveBytesInSegment << std::endl;
			std::cout << "\t[SegmentUsageTable] ageOfYoungestBlock: " << table[i].ageOfYoungestBlock << std::endl;
		}
	}	

	InMemorySegment * ReadSegment(unsigned int segmentNumber)
	{
		InMemorySegment * segmentToRead = segmentFactory->Build(segmentNumber);
		if (readSegment(segmentToRead) != 0)
		{
			std::cerr << "[LogLayer] ERROR: Unable to read segment from flash" << std::endl;
	        std::cerr << "[LogLayer] segment number: " << segmentNumber << std::endl;
    		std::cerr << "[LogLayer] errno: " << errno << std::endl;
			segmentFactory->Destroy(segmentToRead);
			return NULL;
		}

		return segmentToRead;
	}

	void FreeSegment(InMemorySegment * segment)
	{
		segmentFactory->Destroy(segment);
	}

	int EraseSegment(unsigned int segment)
	{
		if (eraseSegment(segment) != 0)
		{
			return 1;
		}

		segmentUsageTable[segment].liveBytesInSegment = 0;
		segmentUsageTable[segment].ageOfYoungestBlock = 0;
		return WriteSegmentUsageTable(segmentUsageTable);
	}

	int InvalidateSegment(unsigned int segment)
	{
		segmentCache->invalidateEntry(segment);
		return 0;
	}

private:
	InMemorySegment * getSegment(unsigned int segmentNumber)
	{
		InMemorySegment * segmentToRead = NULL;

		if (segmentCache->containsEntry(segmentNumber))
		{
			segmentToRead = segmentCache->getEntry(segmentNumber);
		}
		else if (tailSegment->summary.segmentNumber == segmentNumber)
		{
			segmentToRead = tailSegment;
		}
		else
		{
			segmentToRead = segmentFactory->Build(segmentNumber);
			if (readSegment(segmentToRead) != 0)
			{
				std::cerr << "[LogLayer] ERROR: Unable to read segment from flash" << std::endl;
		        std::cerr << "[LogLayer] segment number: " << segmentNumber << std::endl;
        		std::cerr << "[LogLayer] errno: " << errno << std::endl;
				segmentFactory->Destroy(segmentToRead);
				return NULL;
			}

			segmentCache->putEntry(segmentToRead);
		}

		return segmentToRead;
	}

	int readSegment(InMemorySegment * segmentToRead)
	{
		std::cout << "[LogLayer] Reading log segment " << segmentToRead->summary.segmentNumber << " from flash" << std::endl;

		// read segment into temp buffer
		unsigned int segmentSizeInBytes = segmentFactory->getSegmentSizeInBytes();
		void * segmentBuffer            = malloc(segmentSizeInBytes);
		memset(segmentBuffer, 0, segmentSizeInBytes);
		unsigned int sector  = segmentToRead->summary.startSector;
		unsigned int count   = segmentFactory->getSegmentSizeInSectors();
		if (Flash_Read(flash, sector, count, segmentBuffer) == 1)
		{
			free(segmentBuffer);
			return 1;
		}

		// copy summary fields
		int * newINumPointer   = segmentToRead->summary.blockINums;
		int * newBlocksPointer = segmentToRead->summary.iNodeBlockNumbers;
		segmentToRead->summary = *reinterpret_cast<SegmentSummary *>(segmentBuffer); // DONT READ THE INUMS POINTER. ITS REALLOCATED
		segmentToRead->summary.blockINums = newINumPointer;
		segmentToRead->summary.iNodeBlockNumbers = newBlocksPointer;

		// copy blockINums
		char * blockINumsBuffer = (char *)segmentBuffer + sizeof(SegmentSummary);
		memcpy(segmentToRead->summary.blockINums, blockINumsBuffer, segmentToRead->summary.numberOfBlocks * sizeof(int));

		// copy inode blocks nums
		char * iNodeBlocksBuffer = (char *)segmentBuffer + sizeof(SegmentSummary) + segmentToRead->summary.numberOfBlocks * sizeof(int);
		memcpy(segmentToRead->summary.iNodeBlockNumbers, iNodeBlocksBuffer, segmentToRead->summary.numberOfBlocks * sizeof(int));

		// copy data
		unsigned int blockSize = flashData.blockSize * FLASH_SECTOR_SIZE;
		char * dataBuffer      = (char *)segmentBuffer + blockSize;
		memcpy(segmentToRead->data, dataBuffer, segmentSizeInBytes - flashData.blockSize * FLASH_SECTOR_SIZE);

		free(segmentBuffer);
		return 0;
	}

	int writeSegment(InMemorySegment * segmentToWrite)
	{
		std::cout << "[LogLayer] Writing log segment " << segmentToWrite->summary.segmentNumber << " to flash" << std::endl;

		// create buffer with summary block and data
		unsigned int segmentSizeInBytes = segmentFactory->getSegmentSizeInBytes();
		void * bufferToWrite            = malloc(segmentSizeInBytes);
		memset(bufferToWrite, 0, segmentSizeInBytes);

		// copy segment summary block into buffer
		unsigned int blockSize     = flashData.blockSize * FLASH_SECTOR_SIZE;
		char * segmentSummaryBlock = (char *)malloc(blockSize);
		memset(segmentSummaryBlock, 0, blockSize);
		memcpy(segmentSummaryBlock, &segmentToWrite->summary, sizeof(SegmentSummary));
		memcpy(segmentSummaryBlock + sizeof(SegmentSummary), segmentToWrite->summary.blockINums, flashData.segmentSize * sizeof(int));
		memcpy(segmentSummaryBlock + sizeof(SegmentSummary) + flashData.segmentSize * sizeof(int), segmentToWrite->summary.iNodeBlockNumbers, flashData.segmentSize * sizeof(int));
		memcpy(bufferToWrite, segmentSummaryBlock, blockSize);

		// copy data into buffer
		memcpy(((char *)bufferToWrite) + blockSize, segmentToWrite->data, segmentSizeInBytes - blockSize);

		// write to flash
		unsigned int sector = segmentToWrite->summary.startSector;
		unsigned int count  = segmentFactory->getSegmentSizeInSectors();
		int ret             = Flash_Write(flash, sector, count, bufferToWrite);
		free(segmentSummaryBlock);
		free(bufferToWrite);

		// update checkpoint ifile inodes with ifile inodes on disk
		// only want to update the ifileinode in the checkpoint when the segment is written
		checkpoint.iFileINode = iFileINode;

		writesSinceLastCheckpoint++;
		
		segmentUsageTable[segmentToWrite->summary.segmentNumber].liveBytesInSegment = 0;
		for (int s = 1; s < flashData.segmentSize; s++)
		{
			if (segmentToWrite->summary.blockINums[s] != NO_INUM)
			{
				segmentUsageTable[segmentToWrite->summary.segmentNumber].liveBytesInSegment += GetFileBlockSizeInBytes();
			}
		}

		segmentUsageTable[segmentToWrite->summary.segmentNumber].ageOfYoungestBlock = time(0);

		WriteSegmentUsageTable(segmentUsageTable);
		return ret;
	}

	int eraseSegment(unsigned int segmentToErase)
	{
		std::cout << "[LogLayer] Erasing log segment " << segmentToErase << " from flash" << std::endl;
		unsigned int eraseBlocksPerSegment = flashData.segmentSize * flashData.blockSize / FLASH_SECTORS_PER_BLOCK;
		unsigned int eraseBlock = segmentToErase * eraseBlocksPerSegment;
		if (Flash_Erase(flash, eraseBlock, eraseBlocksPerSegment) != 0)
		{
			std::cerr << "[LogLayer] ERROR: Unable to erase segment from flash" << std::endl;
	        std::cerr << "[LogLayer] segment number: " << segmentToErase << std::endl;
    		std::cerr << "[LogLayer] errno: " << errno << std::endl;
    		return 1;
		}
		
		return 0;
	}

	int CheckpointNow()
	{
		std::cout << "[LogLayer] Checkpointing" << std::endl;
		assert(checkpoint.isValid);
		checkpoint.time               = NanosSinceEpoch();
		checkpoint.lastSegmentWritten = 
			recoveredWithPartialSegment ? tailSegment->summary.segmentNumber : tailSegment->summary.segmentNumber - 1;

		// what if we recover from a full segment?
		// havent written back out the partial segment yet. want to use it in recovery. 
		// latest ifile inode already written to checkpoint struct when segment is written

		// find where to write new checkpoint
		checkpointSector = (checkpointSector + CHECKPOINT_SIZE_IN_SECTORS) % (flashData.segmentSize * flashData.blockSize);
		checkpointSector += flashData.checkpointSegment * flashData.segmentSize * flashData.blockSize;
		std::cout << "[LogLayer] Writing checkpoint to sector: " << checkpointSector << std::endl;

		// erase old checkpoint if neccessary 
		if (checkpointSector % FLASH_SECTORS_PER_BLOCK == 0)
		{
			unsigned int eraseBlocksPerSegment = flashData.segmentSize * flashData.blockSize / FLASH_SECTORS_PER_BLOCK;
			unsigned int eraseBlock = (flashData.checkpointSegment * eraseBlocksPerSegment) + (checkpointSector / FLASH_SECTORS_PER_BLOCK);
			std::cout << "[LogLayer] Erasing old checkpoints at erase block: " << eraseBlock << std::endl;
		 	Flash_Erase(flash, eraseBlock, 1);
		}

		// write checkpoint to flash
		unsigned int checkpointBufferSize = CHECKPOINT_SIZE_IN_SECTORS * FLASH_SECTOR_SIZE;
		void * checkpointBuffer = malloc(checkpointBufferSize);
		memset(checkpointBuffer, 0, checkpointBufferSize);
		memcpy(checkpointBuffer, &checkpoint, sizeof(Checkpoint));

		if (Flash_Write(flash, checkpointSector, CHECKPOINT_SIZE_IN_SECTORS, checkpointBuffer) != 0)
	    {
	        std::cerr << "[LogLayer] unable to write checkpoint to flash" << std::endl;
	        std::cerr << "[LogLayer] checkpointSector: " << checkpointSector << std::endl;
	        std::cerr << "errno: " << errno << std::endl;
	        return 1;
	    }

	    free(checkpointBuffer);

		writesSinceLastCheckpoint = 0;
		return 0;
	}

	int RecoverCheckpoint()
	{
		std::cout << "[LogLayer] recovering from checkpoint region" << std::endl;
		unsigned int checkpointBufferSize = CHECKPOINT_SIZE_IN_SECTORS * FLASH_SECTOR_SIZE;
	    void * checkpointBuffer           = malloc(checkpointBufferSize);

	    Checkpoint curr;
	    unsigned int checkpointSegmentStartSector = flashData.checkpointSegment * flashData.segmentSize * flashData.blockSize;
	    unsigned int checkpointSegmentEndSector = checkpointSegmentStartSector + (flashData.segmentSize * flashData.blockSize);

	    for (unsigned int currSector = checkpointSegmentStartSector; currSector < checkpointSegmentEndSector; currSector += CHECKPOINT_SIZE_IN_SECTORS)
	    {
	    	memset(&curr, 0, sizeof(Checkpoint));
	    	memset(checkpointBuffer, 0, checkpointBufferSize);

	    	if (Flash_Read(flash, currSector, CHECKPOINT_SIZE_IN_SECTORS, checkpointBuffer) != 0)
		    {
		        std::cerr << "[LogLayer] Unable to recover checkpoint on initFlash" << std::endl;
		        std::cerr << "[LogLayer] errno: " << errno << std::endl;
		        return 1;
		    }

	   		memcpy(&curr, checkpointBuffer, sizeof(Checkpoint));
	   		if (curr.isValid && checkpoint.time < curr.time)
	   		{
	   			checkpoint = curr;
	   			checkpointSector = currSector;
	   		}
	    }

	    free(checkpointBuffer);

		std::cout << "[LogLayer] recovered checkpoint at sector: " << checkpointSector << std::endl;
		std::cout << "[LogLayer] \t time: " << checkpoint.time << std::endl;
		std::cout << "[LogLayer] \t lastSegmentWritten: " << checkpoint.lastSegmentWritten << std::endl;
		std::cout << "[LogLayer] \t segmentUsageTableSegment: " << checkpoint.segmentUsageTableSegment << std::endl;
		checkpoint.iFileINode.Print();
		return 0;
	}

	unsigned int GetCleanSegment()
	{
		for (int segment = flashData.checkpointSegment + 1; segment < flashData.flashSize; ++segment)
		{
			if (segmentUsageTable[segment].liveBytesInSegment == 0)
			{
				return segment;
			}
		}

		return FLASH_FULL;
	}

	bool ValidLogAddress(LogAddress& logAddress)
	{
		unsigned int segmentNumber = logAddress.logSegment;
		unsigned int blockNumber   = logAddress.blockNumber;
		return segmentNumber < flashData.flashSize && blockNumber < flashData.segmentSize;
	}

	void PrintInitData()
	{
		std::cout << "[LogLayer] initializing log..."           								  << std::endl;
	    std::cout << "\tflash file: "                << flashFile                                 << std::endl;
	    std::cout << "\tblock size: "                << flashData.blockSize                       << std::endl;
	    std::cout << "\tsegment size: "              << flashData.segmentSize                     << std::endl;
	    std::cout << "\tflash size: "                << flashData.flashSize                       << std::endl;
	    std::cout << "\twear limit: "                << flashData.wearLimit                       << std::endl;	
	    std::cout << "\tnum blocks: "                << flashData.numBlocks                       << std::endl;
	}
};
