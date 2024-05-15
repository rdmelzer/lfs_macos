#pragma once

#include <stdio.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../layers/log.hpp"
#include "../data_structures/inode.hpp"
#include "../data_structures/log_address.hpp"

class IFileLayer
{
public:
    virtual ~IFileLayer(){}
    virtual int Init() = 0;
	virtual int File_Statfs(struct statvfs * stbuf) = 0;
	virtual int File_Create(FileType type, mode_t mode, unsigned int * inumOut) = 0;
	virtual int File_Write(unsigned int inum, unsigned int offset, unsigned int length, const void * buffer) = 0;
	virtual int File_Read(unsigned int inum, unsigned int offset, unsigned int length, void * buffer) = 0;
	virtual int File_Truncate(unsigned int inum, unsigned int size) = 0;
	virtual int File_Free(unsigned int inum) = 0;
	virtual int File_GetAttr(unsigned int inum, struct stat * stbuf) = 0;
	virtual int File_Chmod(unsigned int inum, mode_t mode) = 0;
	virtual int File_Chown(unsigned int inum, uid_t uid, gid_t gid) = 0;
	virtual int File_AddLink(unsigned int inum) = 0;
	virtual int File_RemoveLink(unsigned int inum) = 0;
	virtual FileType File_GetFileType(unsigned int inum) = 0;
	virtual int RunCleaner() = 0;
};

class FileLayer : public IFileLayer
{
private:
	ILog *       log;
	unsigned int iFileSizeInINodes;
	unsigned int blockSizeInBytes;
	unsigned int numLogAddrInBlock;
	unsigned int maxFileBlocks;
	unsigned int flashSize;
	unsigned int cleaningStartThreshold;
	unsigned int cleaningEndThreshold;
	unsigned int firstSegment;

public:
	FileLayer(char * flashFile, unsigned int cacheSize, unsigned int checkpointInterval, unsigned int cleaningStart, unsigned int cleaningEnd) :
		iFileSizeInINodes(INITIAL_IFILE_SIZE),
		cleaningStartThreshold(cleaningStart),
		cleaningEndThreshold(cleaningEnd)
	{
    	log = new Log(flashFile, cacheSize, checkpointInterval);
	}

	~FileLayer()
	{
		delete log;
	}

	int Init()
	{
    	std::cout << "[FileLayer] Initializing File Layer..." << std::endl;
    	log->Init();
    	iFileSizeInINodes = log->GetIFileINode().fileSize / sizeof(INode);
    	blockSizeInBytes  = log->GetFileBlockSizeInBytes();
    	flashSize         = log->GetFlashSize();
    	firstSegment      = log->GetFirstSegment();
    	numLogAddrInBlock = blockSizeInBytes / sizeof(LogAddress);
    	maxFileBlocks     = 4 + numLogAddrInBlock;
		return 0;
	}

	int File_Statfs(struct statvfs* stbuf)
	{
	    stbuf->f_files  = iFileSizeInINodes; /* # inodes */
	    stbuf->f_ffree  = INITIAL_IFILE_SIZE; /* # free inodes */
	    stbuf->f_favail = INITIAL_IFILE_SIZE; /* # free inodes for unprivileged users */
		return log->Log_Statfs(stbuf);
	}

	int File_Create(FileType fileType, mode_t mode, unsigned int * inumOut) 
	{
    	std::cout << "[FileLayer] Creating file" << std::endl;

		if (InitNewINode(fileType, mode, inumOut) != 0)
		{
			std::cerr << "[FileLayer] ERROR: unable to init new inode. inum: " << *inumOut << std::endl;
			return 1;
		}

		return 0;
	}

	int File_Write(unsigned int inum, unsigned int offset, unsigned int length, const void * buffer)
	{
    	std::cout << "[FileLayer] Writing inum " << inum << " at offset " << offset << " for length " << length << std::endl;
    	if (length == 0)
    	{
    		return 0;
    	}

    	INode inode = GetINode(inum);

		if (offset > inode.fileSize)
		{
	        std::cerr << "[FileLayer] ERROR: Attempting to write beyond end of file. inum: " << inum << std::endl;
	        std::cerr << "[FileLayer] \tfile size: " << inode.fileSize << std::endl;
	        std::cerr << "[FileLayer] \toffset: " << length << std::endl;
	        return 1;
		}

		unsigned int startBlock          = offset / blockSizeInBytes;
		unsigned int writeLengthInBlocks = 1;
		unsigned int startBlockOffset    = offset % blockSizeInBytes;
		if (startBlockOffset + length > blockSizeInBytes)
		{
			unsigned int remainder = length - (blockSizeInBytes - startBlockOffset);
			writeLengthInBlocks += remainder / blockSizeInBytes + (remainder % blockSizeInBytes > 0);
		}

		unsigned int endBlock            = startBlock + writeLengthInBlocks - 1;

    	std::cout << "[FileLayer] Writing block " << startBlock << " to block " << endBlock << " of inum " << inum << std::endl;

    	void * blockBuffer                = malloc(blockSizeInBytes);
		unsigned int writeLengthRemaining = length;
		unsigned int bufferOffset         = 0;
    	for (int blockToWrite = startBlock; blockToWrite <= endBlock; ++blockToWrite)
    	{
    		std::cout << "[FileLayer] Writing block " << blockToWrite << std::endl;
    		if (blockToWrite >= maxFileBlocks)
    		{
    			std::cout << "[FileLayer] Attempting to write beyond maximum file blocks" << std::endl;
    			break;
    		}

			LogAddress blockAddress = GetBlockAddress(inode, blockToWrite);

    		if ((blockAddress.logSegment != EMPTY_DIRECT_BLOCK_ADDRESS && blockAddress.blockNumber == EMPTY_DIRECT_BLOCK_ADDRESS) || 
    			(blockAddress.logSegment == EMPTY_DIRECT_BLOCK_ADDRESS && blockAddress.blockNumber != EMPTY_DIRECT_BLOCK_ADDRESS))
    		{
    			std::cerr << "[FileLayer] ERROR: Malformed direct block log address. inum: " << inum << std::endl;
	        	std::cerr << "[FileLayer] \tdirectBlock: " << blockToWrite << std::endl;
	        	free(blockBuffer);
	        	return 1;
    		}

			memset(blockBuffer, 0, blockSizeInBytes);
			if (blockAddress.logSegment != EMPTY_DIRECT_BLOCK_ADDRESS)
			{
				if (log->Log_Read(blockAddress, blockBuffer) != 0)
				{
					std::cerr << "[FileLayer] ERROR: Log write failed in File_Write. inum: " << inum << std::endl;
	        		std::cerr << "[FileLayer] \tblock: " << blockToWrite << std::endl;
	        		free(blockBuffer);
	        		return 1;
				}

				log->Log_Free(blockAddress);
			}

    		unsigned int blockOffset;
    		unsigned int writeLength;
			if (writeLengthInBlocks == 1)
			{
				blockOffset   = offset % blockSizeInBytes;
				writeLength   = length;
			}
			else if (blockToWrite == startBlock)
			{
				blockOffset   = offset % blockSizeInBytes;
				writeLength   = blockSizeInBytes - blockOffset;
			}
			else if (blockToWrite == endBlock)
			{
				blockOffset   = 0;
				writeLength   = writeLengthRemaining;
			}
			else
			{
				blockOffset   = 0;
				writeLength   = blockSizeInBytes;
			}

			memcpy((char *) blockBuffer + blockOffset, (char *) buffer + bufferOffset, writeLength);
			if (log->Log_Write(inum, blockToWrite, blockBuffer, &blockAddress) != 0)
			{
				std::cerr << "[FileLayer] ERROR: Log_Write failed in File_Write. inum: " << inum << std::endl;
	        	std::cerr << "[FileLayer] \tdirectBlock: " << blockToWrite << std::endl;
	        	free(blockBuffer);
	        	return 1;
			}

			UpdateINodeBlock(&inode, blockToWrite, blockAddress);

			writeLengthRemaining -= writeLength;
			bufferOffset += writeLength;
    	}

    	if (offset + length > inode.fileSize) // think about this for truncating?????
    	{
    		inode.fileSize = offset + length;
    	}

    	time_t now = time(0);
		inode.atime = now;		
		inode.mtime = now;		

    	if (UpdateIFile(inode) != 0)
    	{
			std::cerr << "[FileLayer] ERROR: unable to update iFile on File_Create" <<  std::endl;
			// there will be inconsistent state if there is failure here. maybe fix this for phase 2.
    		return 1;
    	}

		return 0;
	}

	int File_Read(unsigned int inum, unsigned int offset, unsigned int length, void * buffer)
	{
    	std::cout << "[FileLayer] Reading inum " << inum << " at offset " << offset << " for length " << length << std::endl;
    	if (length == 0)
    	{
    		return 0;
    	}

		INode inode = GetINode(inum);
		if (inode.inUse == false)
		{
    		std::cerr << "[FileLayer] inum " << inum << " is empty" << std::endl;
    		return 1;
		}

		if (length > inode.fileSize - offset)
		{
			length = inode.fileSize - offset;
		}
    	
    	memset(buffer, 0, length);

		unsigned int startBlock         = offset / blockSizeInBytes;
		unsigned int readLengthInBlocks = 1;
		unsigned int startBlockOffset   = offset % blockSizeInBytes;
		if (startBlockOffset + length > blockSizeInBytes)
		{
			unsigned int remainder = length - (blockSizeInBytes - startBlockOffset);
			readLengthInBlocks += remainder / blockSizeInBytes + (remainder % blockSizeInBytes > 0);
		}

		unsigned int endBlock           = startBlock + readLengthInBlocks - 1;

    	std::cout << "[FileLayer] Reading block " << startBlock << " to block " << endBlock << " of inum " << inum << std::endl;

		void * blockBuffer               = malloc(blockSizeInBytes);
		unsigned int readLengthRemaining = length;
		unsigned int bufferOffset        = 0;
		for (int blockToRead = startBlock; blockToRead <= endBlock; ++blockToRead)
		{
			if (blockToRead >= maxFileBlocks)
    		{
    			std::cout << "[FileLayer] Attempting to read beyond maximum file blocks" << std::endl;
    			break;
    		}

			LogAddress addr = GetBlockAddress(inode, blockToRead);
			if ((addr.logSegment != EMPTY_DIRECT_BLOCK_ADDRESS && addr.blockNumber == EMPTY_DIRECT_BLOCK_ADDRESS) || 
    			(addr.logSegment == EMPTY_DIRECT_BLOCK_ADDRESS && addr.blockNumber != EMPTY_DIRECT_BLOCK_ADDRESS))
    		{
    			std::cerr << "[FileLayer] ERROR: Malformed direct block log address. inum: " << inum << std::endl;
	        	std::cerr << "[FileLayer] \tdirectBlock: " << blockToRead << std::endl;
	        	free(blockBuffer);
	        	return 1;
    		}

			if (addr.logSegment == EMPTY_DIRECT_BLOCK_ADDRESS)
			{
				std::cerr << "[FileLayer] ERROR: Attempting to read empty direct block. inum: " << inum << std::endl;
	        	std::cerr << "[FileLayer] \tblock: " << blockToRead << std::endl;
	        	free(blockBuffer);
	        	return 1;
			}

			memset(blockBuffer, 0, blockSizeInBytes);
			if (log->Log_Read(addr, blockBuffer) != 0)
			{
				std::cerr << "[FileLayer] ERROR: Log read failed in File_Read. inum: " << inum << std::endl;
	        	std::cerr << "[FileLayer] \tblock: " << blockToRead << std::endl;
	        	free(blockBuffer);
	        	return 1;
			}

			unsigned int blockOffset;
			unsigned int copyLength;
			if (readLengthInBlocks == 1)
			{
				blockOffset  = offset % blockSizeInBytes;
				copyLength   = length;
			}
			else if (blockToRead == startBlock)
			{
				blockOffset  = offset % blockSizeInBytes;
				copyLength   = blockSizeInBytes - blockOffset;
			}
			else if (blockToRead == endBlock)
			{
				blockOffset  = 0;
				copyLength   = readLengthRemaining;
			}
			else
			{
				blockOffset  = 0;
				copyLength   = blockSizeInBytes;
			}

			memcpy((char *)buffer + bufferOffset, (char *)blockBuffer + blockOffset, copyLength);

			readLengthRemaining -= copyLength;
			bufferOffset += copyLength;
		}

		free(blockBuffer);		
		return 0;
	}

	int File_Truncate(unsigned int inum, unsigned int size)
	{
    	std::cout << "[FileLayer] Truncating file at inum: " << inum << " size: " << size << std::endl;

		INode inode = GetINode(inum);

		unsigned int oldFileSize = inode.fileSize;
		void * buffer = malloc(oldFileSize);

		if (File_Read(inum, 0, oldFileSize, buffer) != 0)
    	{
			std::cerr << "[FileLayer] ERROR: File_Truncate: File_Read failed" << std::endl;
    		return 1;
    	}

        inode.fileSize = 0;
    	for (int b = 0; b < 4; ++b)
        {
        	log->Log_Free(inode.directBlocks[b]);
            inode.directBlocks[b].logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
            inode.directBlocks[b].blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;
        }

    	log->Log_Free(inode.indirectBlock);
        inode.indirectBlock.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
        inode.indirectBlock.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;
        
        UpdateIFile(inode);

    	void * newBuffer = malloc(size); memset(newBuffer, 0, size);
    	unsigned int copyLength = oldFileSize;
    	if (size <= oldFileSize)
    	{
    		copyLength = size;
    	}
    	
    	memcpy(newBuffer, buffer, copyLength);
    	if (File_Write(inum, 0, size, newBuffer) != 0)
    	{
			std::cerr << "[FileLayer] ERROR: File_Truncate: File_Write failed" << std::endl;
    		return 1;
    	}

    	free(buffer);
    	free(newBuffer);
		return 0;
	}

	int File_Free(unsigned int inum)
	{
    	std::cout << "[FileLayer] File_Free Freeing inum " << inum << std::endl;
    	if (inum == IFILE_INUM)
    	{
			std::cerr << "[FileLayer] ERROR: File_Free attempting to free iFile" << std::endl;
    		throw;
    	}

		INode toFree = GetINode(inum); // maybe change this from throwing error? DONT MEMSET, INUM BECOMES 0
		toFree.inUse = false;
		for (int b = 0; b < 4; ++b)
        {
        	log->Log_Free(toFree.directBlocks[b]);
            toFree.directBlocks[b].logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
            toFree.directBlocks[b].blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;
        }

    	log->Log_Free(toFree.indirectBlock);
        toFree.indirectBlock.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
        toFree.indirectBlock.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;

		UpdateIFile(toFree);
		return 0;
	}

	int File_GetAttr(unsigned int inum, struct stat * stbuf)
	{
		INode inode = GetINode(inum);

		unsigned int blocks = inode.fileSize / blockSizeInBytes;
	    if (inode.fileSize % blockSizeInBytes != 0)
	    {
	    	blocks++;
	    }

		stbuf->st_mode    = GetFileTypeMode(inode.fileType) | inode.permissions;
	    stbuf->st_nlink   = inode.nlinks;   // number of hardlinks. add to inode
	    stbuf->st_size    = inode.fileSize; // change for symlinks
	    stbuf->st_ino     = inum;
	    stbuf->st_uid     = inode.uid;
	    stbuf->st_gid     = inode.gid;
	    stbuf->st_blksize = blockSizeInBytes;
	    stbuf->st_blocks  = blocks;
	    stbuf->st_atime   = inode.atime; // time of last access
	    stbuf->st_mtime   = inode.mtime; // time of last modification
	    stbuf->st_ctime   = inode.ctime; // time of last status change

		return 0;
	}

	int File_Chmod(unsigned int inum, mode_t mode)
	{
		INode inode       = GetINode(inum);
		inode.permissions = mode;
		inode.ctime       = time(0);		
		return UpdateIFile(inode);
	}

	int File_Chown(unsigned int inum, uid_t uid, gid_t gid)
	{
		INode inode = GetINode(inum);
		inode.uid   = uid;
		inode.gid   = gid;
		inode.ctime = time(0);		
		return UpdateIFile(inode);
	}

	int File_AddLink(unsigned int inum)
	{
		INode noteToUpdate = GetINode(inum);
		noteToUpdate.nlinks++;
		return UpdateIFile(noteToUpdate);
	}

	int File_RemoveLink(unsigned int inum)
	{
		INode noteToUpdate = GetINode(inum);
		noteToUpdate.nlinks--;
		int ret = UpdateIFile(noteToUpdate);
		if (noteToUpdate.nlinks == 0)
		{
			return File_Free(inum);
		}

		return ret;
	}

	FileType File_GetFileType(unsigned int inum)
	{
		return GetINode(inum).fileType;
	}

	void PrintIFile()
	{
		std::cout << "[FileLayer] Printing IFile INode " << std::endl;
		GetINode(IFILE_INUM).Print();

		for (int inum = 1; inum < iFileSizeInINodes; inum++)
		{
			PrintIFileEntry(inum);
		}
	}

	void PrintIFileEntry(unsigned int inum)
	{
		std::cout << "[FileLayer] Printing IFile Entry for inum " << inum << std::endl;
		GetINode(inum).Print();
	}

	int RunCleaner()
	{
		std::cout << "[Cleaner] Checking number of free segments..." << std::endl;

		unsigned int cleanSegments = 0;
		SegmentUsageTableEntry * segmentUsageTable = log->ReadSegmentUsageTable();

		for (unsigned int segment = firstSegment; segment < flashSize; ++segment)
		{
			if (segmentUsageTable[segment].liveBytesInSegment == 0)
			{
				cleanSegments++;
			}
		}

		if (cleanSegments > cleaningStartThreshold)
		{
			free(segmentUsageTable);
			return 0;
		}

		return CleanLog(cleanSegments, segmentUsageTable);
	}

private:
	INode GetINode(unsigned int inum)
	{
		if (inum > iFileSizeInINodes)
		{
			std::cerr << "[FileLayer] ERROR: Attempting to retrieve non-existent inode. inum: " << inum << std::endl;
			throw;
		}

		if (inum == IFILE_INUM)
		{
			return log->GetIFileINode();
		}

		INode toReturn;
		memset(&toReturn, 0, sizeof(INode));
		unsigned int iNodeOffset = (inum - 1) * sizeof(INode);
		if (File_Read(IFILE_INUM, iNodeOffset, sizeof(INode), (void *) &toReturn) != 0)
		{
			std::cerr << "[FileLayer] ERROR: GetINode failed. inum: " << inum << std::endl;
			throw;
		}

		return toReturn;
	}

	int InitNewINode(FileType fileType, mode_t mode, unsigned int * out)
	{
    	unsigned int inum = GetUnusedINum();
    	std::cout << "[FileLayer] Creating file with inum " << inum << " and file type " << GetFileTypeString(fileType) << std::endl;

		if (inum <= iFileSizeInINodes && GetINode(inum).inUse == true)
		{
			std::cerr << "[FileLayer] ERROR: inum already in use. inum: " << inum << std::endl;
			return 1;
		}

    	time_t now = time(0);

		INode newINode = {
			.inUse       = true,
			.inum        = inum,
			.fileType    = fileType,
			.fileSize    = 0,
			.nlinks      = 1,
			.uid         = getuid(),
			.gid         = getgid(),
			.permissions = mode,
			.atime       = now,
			.mtime       = now,
			.ctime       = now,
		};

		for (int b = 0; b < 4; ++b)
		{
			newINode.directBlocks[b] = {
				.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS,
				.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS
			};
		}

		newINode.indirectBlock = {
			.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS,
			.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS
		};


		if (inum == iFileSizeInINodes + 1)
		{
			iFileSizeInINodes++;
		}

		*out = inum;
		return UpdateIFile(newINode);
	}

	int UpdateIFile(INode toUpdate)
	{
		unsigned int inum = toUpdate.inum;
		if (inum == IFILE_INUM)
		{
    		log->UpdateIFileINode(toUpdate);
			return 0;
		}

		unsigned int iNodeOffset = (inum - 1) * sizeof(INode);
		return File_Write(IFILE_INUM, iNodeOffset, sizeof(INode), (const void *) &toUpdate);
	}

	unsigned int GetUnusedINum()
	{
		for (unsigned int inum = IFILE_INUM + 1; inum <= iFileSizeInINodes; inum++)
		{
			if (GetINode(inum).inUse == false)
			{
				return inum;
			}
		}

		return iFileSizeInINodes + 1;
	}

	LogAddress GetBlockAddress(INode iNode, unsigned int blockNum)
	{
		if (blockNum < 4)
		{
			return iNode.directBlocks[blockNum];
		}

		if (blockNum > numLogAddrInBlock + 4)
		{
			std::cerr << "[FileLayer] ERROR: GetBlockAddress Beyond max number of blocks" << std::endl;
			throw;
		}

		if (iNode.indirectBlock.logSegment == EMPTY_DIRECT_BLOCK_ADDRESS)
		{
			LogAddress ret = {
				.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS,
				.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS
			};

			return ret;
		}

		LogAddress * indirectBlocks      = ReadIndirectBlocks(iNode.indirectBlock);
		LogAddress ret                   = indirectBlocks[blockNum - 4];
		free(indirectBlocks);
		return ret;
	}

	LogAddress * ReadIndirectBlocks(LogAddress addr)
	{
		if (addr.logSegment == EMPTY_DIRECT_BLOCK_ADDRESS || addr.blockNumber == EMPTY_DIRECT_BLOCK_ADDRESS)
		{
			std::cerr << "[FileLayer] ERROR: ReadIndirectBlocks attempting to read empty log address" << std::endl;
			throw;
		}

		void * indirectBlocksBuffer = malloc(blockSizeInBytes);
		memset(indirectBlocksBuffer, 0, blockSizeInBytes);

		if (log->Log_Read(addr, indirectBlocksBuffer) != 0)
		{
			std::cerr << "[FileLayer] ERROR: Log read failed in ReadIndirectBlocks" << std::endl;
        	addr.Print();
        	free(indirectBlocksBuffer);
        	return NULL;
		}

		LogAddress * indirectBlocks = (LogAddress *)malloc(numLogAddrInBlock * sizeof(LogAddress));
		memcpy(indirectBlocks, indirectBlocksBuffer, numLogAddrInBlock * sizeof(LogAddress));
		free(indirectBlocksBuffer);
		return indirectBlocks;
	}

	int UpdateINodeBlock(INode * iNode, unsigned int blockNum, LogAddress logAddress)
	{
		if (blockNum < 4)
		{
			iNode->directBlocks[blockNum] = logAddress;
			return 0;
		}

		LogAddress * indirectBlocks;
		if (iNode->indirectBlock.logSegment == EMPTY_DIRECT_BLOCK_ADDRESS)
		{
			indirectBlocks = (LogAddress *)malloc(numLogAddrInBlock * sizeof(LogAddress));
			for (int i = 0; i < numLogAddrInBlock; ++i)
			{
				indirectBlocks[i] = {
					.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS,
					.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS
				};
			}
		}
		else
		{
			indirectBlocks = ReadIndirectBlocks(iNode->indirectBlock);
		}

		indirectBlocks[blockNum - 4] = logAddress;
		int ret = WriteIndirectBlocks(iNode, indirectBlocks);
		free(indirectBlocks);
		return ret;
	}

	int WriteIndirectBlocks(INode * iNode, LogAddress * indirectBlocks)
	{
		void * blockBuffer = malloc(blockSizeInBytes);
		memset(blockBuffer, 0, blockSizeInBytes);

		memcpy(blockBuffer, indirectBlocks, numLogAddrInBlock * sizeof(LogAddress));

		int ret = 0;
		if (log->Log_Write(iNode->inum, iNode->indirectBlock.blockNumber, blockBuffer, &iNode->indirectBlock) != 0)
		{
			std::cerr << "[FileLayer] ERROR: Log_Write failed in WriteIndirectBlocks. inum: " << iNode->inum << std::endl;
        	ret = 1;
		}

        free(blockBuffer);
		return ret;
	}

	int CleanLog(unsigned int cleanSegments, SegmentUsageTableEntry * segmentUsageTable)
	{
		std::cout << "[Cleaner] Cleaning log. Clean segments: " << cleanSegments << ". Start threshold: " << cleaningStartThreshold << std::endl;
		log->PrintSegmentUsageTable(segmentUsageTable);

		std::vector<std::tuple<double, unsigned int>> policies;
		for (unsigned int segment = firstSegment; segment < flashSize; ++segment)
		{
			double costBenefit = ComputePolicy(segmentUsageTable[segment]);
			if (costBenefit == 0.0)
			{
				continue;
			}

			policies.push_back(std::make_tuple(costBenefit, segment));
		}

		std::sort(policies.begin(), policies.end());
		std::cout << "[Cleaner] Policies:" << std::endl;

		for (auto it = policies.begin(); it != policies.end(); ++it)
		{
			auto policy = *it;
			double p = std::get<0>(policy);
			unsigned int seg = std::get<1>(policy);
			std::cout << "[Cleaner] Policy: " << p << " segment: " << seg << std::endl;
		}

		while (!policies.empty())
		{
			if (cleanSegments >= cleaningEndThreshold)
			{
				break;
			}

			std::tuple<double, unsigned int> segmentPolicy = policies.back();
			unsigned int segmentNumber                     = std::get<1>(segmentPolicy);
			InMemorySegment * segment                      = log->ReadSegment(segmentNumber);

			int ret = CleanSegment(segment);
			if (ret != 0)
			{
				return ret;
			}

			log->EraseSegment(segment->summary.segmentNumber);
			log->InvalidateSegment(segment->summary.segmentNumber);
			log->FreeSegment(segment);
			policies.pop_back();
			cleanSegments++;
		}

		free(segmentUsageTable);
		std::cout << "[Cleaner] Cleaner Done" << std::endl;

		segmentUsageTable = log->ReadSegmentUsageTable();
		log->PrintSegmentUsageTable(segmentUsageTable);
		free(segmentUsageTable);

		return 0;
	}

	double ComputePolicy(SegmentUsageTableEntry entry)
	{
		double u = entry.liveBytesInSegment / blockSizeInBytes;
		double age = static_cast<long int>(entry.ageOfYoungestBlock) / 1e14;
		return ( (1 - u) * age ) / (1 + u);
	}

	int CleanSegment(InMemorySegment * segment)
	{
		std::cout << "[Cleaner] Cleaning segment: " << segment->summary.segmentNumber << std::endl;
		log->PrintTailSummary();

		int ret = 0;
		SegmentSummary * summary = &segment->summary;
		//std::vector<std::tuple<INode, unsigned int, LogAddress>> updates;

		for (unsigned int block = 1; block < summary->numberOfBlocks; ++block)
		{
			int inum            = summary->blockINums[block];
			int fileBlockNumber = summary->iNodeBlockNumbers[block];
			if (inum == NO_INUM)
			{
				continue;
			}

			LogAddress logAddress = 
			{
				.logSegment  = summary->segmentNumber,
				.blockNumber = block,
			};

			INode inode = GetINode(inum);

			LogAddress currentBlockAddress;
			if (fileBlockNumber >= 0)
			{
				currentBlockAddress = GetBlockAddress(inode, fileBlockNumber);
			}
			else if (fileBlockNumber == INDIRECT_BLOCK)
			{
				currentBlockAddress = inode.indirectBlock;
			}
			else
			{
				std::cerr << "[Cleaner] ERROR CleanSegment encountered invalid file block in segment summary. fileBlockNumber: " << fileBlockNumber << std::endl;
				throw;
			}

			if (currentBlockAddress != logAddress)
			{
				continue;
			}

			void * blockBuffer = malloc(blockSizeInBytes);
			memset(blockBuffer, 0, blockSizeInBytes);

			unsigned int offset = (block - 1) * blockSizeInBytes;
			memcpy(blockBuffer, (char *)segment->data + offset, blockSizeInBytes);

			LogAddress newAddress = 
			{
				.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS,
				.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS
			};

			ret += log->Log_Write(inum, fileBlockNumber, blockBuffer, &newAddress);
			ret += UpdateINodeBlock(&inode, fileBlockNumber, newAddress);
	 		ret += UpdateIFile(inode);
	 		//updates.push_back(std::make_tuple(inode, fileBlockNumber, newAddress));

	 		if (ret != 0)
	 		{
	 			return ret;
	 		}
		}

		/*
		for (auto it = updates.begin(); it != updates.end(); ++it)
		{
			auto update = *it;
			INode inode = std::get<0>(update);
			unsigned int fileBlockNumber = std::get<1>(update);
			LogAddress newAddress = std::get<2>(update);

			ret += UpdateINodeBlock(&inode, fileBlockNumber, newAddress);
	 		ret += UpdateIFile(inode);
		}
		*/

		return ret;
	}
};
