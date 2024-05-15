/*
Scans for:
	- in-use inodes that do not have directory entries
	- directory entries that refer to unused inodes
	- incorrect segment summary information

	USAGE: lfsck file
*/


#include <stdio.h>
#include <iostream>
#include <string>
#include "../layers/flash/flash.h"
#include "../layers/directory.hpp"
#include "../data_structures/flash_data.hpp"
#include "../data_structures/segment.hpp"
#include "../data_structures/inode.hpp"

int reportInUseINodesWithNoDirectoryEntries(int * errors);
int reportDirectoryEntriesThatReferToUnusedINodes(int * errors);
int reportIncorrectSegmentSummaryInformation(int * errors);


void printIncorrectSegmentSummaryInfo(unsigned int segment, unsigned int block, int blockINum, INode inode);
int checkBlock(unsigned int segment, unsigned int block, int blockINum, INode& inode);
int readDirectory(INode& inode, DirectoryList * directoryList);
int readSegmentSummaryBlock(unsigned int segment, SegmentSummary * summaryBlock);
int readBlock(unsigned int segment, unsigned int block, void * buffer);
int readSegment(unsigned int segment, void * buffer);
int readFlashData(char * flashFile);
int readIFileINode();
int readIFile();

Flash     flash;
FlashData flashData;
INode     iFileINode;
INode *   iFileArray;

int main(int argc, char **argv)
{
	if (argc != 2)
	{
        std::cerr << "USAGE: " << argv[0] << " file" << std::endl;
	    return 1;
	}

    char * flashFile = argv[argc - 1];
    if (readFlashData(flashFile) != 0)
    {
        std::cerr << "ERROR: readFlashData" << std::endl;
        return 1;
    }

    if (readIFileINode() != 0)
    {
        std::cerr << "ERROR: readIFileINode" << std::endl;
        return 1;
    }

    iFileINode.Print();

    if (readIFile() != 0)
    {
        std::cerr << "ERROR: readIFile" << std::endl;
        return 1;
    }

    std::cout << "\nERROR REPORT" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    int errors = 0;
    if (reportInUseINodesWithNoDirectoryEntries(&errors) != 0)
    {
    	std::cerr << "ERROR: reportInUseINodesWithNoDirectoryEntries" << std::endl;
    }

	if (reportDirectoryEntriesThatReferToUnusedINodes(&errors) != 0)
	{
    	std::cerr << "ERROR: reportDirectoryEntriesThatReferToUnusedINodes" << std::endl;
	}

	if (reportIncorrectSegmentSummaryInformation(&errors) != 0)
	{
    	std::cerr << "ERROR: reportIncorrectSegmentSummaryInformation" << std::endl;
	}

	Flash_Close(flash);
    free(iFileArray);
	return errors;
}

int reportInUseINodesWithNoDirectoryEntries(int * errors)
{
    std::cout << "IN USE INODES WITH NO DIRECTORY ENTRIES" << std::endl;

    INode& rootINode = iFileArray[ROOT_DIRECTORY_INUM - 1];
    DirectoryList * directoryList = (DirectoryList *)malloc(sizeof(DirectoryList));
    readDirectory(rootINode, directoryList);

    unsigned int iFileSize = iFileINode.fileSize / sizeof(INode);
    for (int i = 0; i < iFileSize; ++i)
    {        
        INode inode = iFileArray[i];
        if (inode.inUse && inode.inum != ROOT_DIRECTORY_INUM)
        {

            bool iNumFound = false;
            for (int dirEntry = 0; dirEntry < directoryList->size; ++dirEntry)
            {
                unsigned int dirEntryINum = directoryList->directoryEntries[dirEntry].inum;
                if (inode.inum == dirEntryINum)
                {
                    iNumFound = true;
                    break;
                }
            }

            if (!iNumFound)
            {
                std::cout << "Found an in use inode with no directory entry!" << std::endl;
                std::cout << "inodes inum: " << inode.inum << std::endl;
                (*errors)++;
            }
        }
    }

    free(directoryList->directoryEntries);
    free(directoryList);
	return 0;
}

int reportDirectoryEntriesThatReferToUnusedINodes(int * errors)
{
    std::cout << "DIRECTORY ENTRIES THAT REFER TO UNUSED INODES" << std::endl;

    INode& rootINode = iFileArray[ROOT_DIRECTORY_INUM - 1];

    DirectoryList * directoryList = (DirectoryList *)malloc(sizeof(DirectoryList));
    memset(directoryList, 0, sizeof(DirectoryList));

    readDirectory(rootINode, directoryList);

    for (int i = 0; i < directoryList->size; ++i)
    {        
        unsigned int inum = directoryList->directoryEntries[i].inum;
        if (inum == IFILE_INUM)
        {
            continue;
        }

        INode& iFileEntry = iFileArray[inum-1];
        if (iFileEntry.inUse == false)
        {
            std::cout << "Found a directory entry that refers to an unused INode!" << std::endl;
            std::cout << "Directory name: " << directoryList->directoryNameINumPair.name << std::endl;
            std::cout << "Directory entry with unused inode: " << directoryList->directoryEntries[i].name << std::endl;
            std::cout << "inum in Directory entry: " << directoryList->directoryEntries[i].inum << std::endl;
            (*errors)++;
        }
    }

    free(directoryList->directoryEntries);
    free(directoryList);
	return 0;
}

int reportIncorrectSegmentSummaryInformation(int * errors)
{
    std::cout << "INCORRECT SEGMENT SUMMARY INFO" << std::endl;

    // go through each segment
    for (int segment = flashData.checkpointSegment + 1; segment < flashData.flashSize; ++segment)
    {
        SegmentSummary * summaryBlock = new SegmentSummary(segment, segment * flashData.segmentSize * flashData.blockSize, flashData.segmentSize);
        readSegmentSummaryBlock(segment, summaryBlock);

        // check metadata
        if (summaryBlock->segmentNumber  != segment                                               ||
            summaryBlock->startSector    != segment * flashData.segmentSize * flashData.blockSize ||
            summaryBlock->numberOfBlocks != flashData.segmentSize)
        {
            if (summaryBlock->segmentNumber != 0 || summaryBlock->startSector != 0 || summaryBlock->numberOfBlocks || 0)
            {
                std::cout << "Segment: " << segment << " has incorrect metadata info" << std::endl;
                (*errors)++;
            }
        }

        // check inums
        for (int block = 0; block < flashData.segmentSize; ++block)
        {
            int blockINum = summaryBlock->blockINums[block];

            if (block == 0)
            {
                if (blockINum != SUMMARY_BLOCK && blockINum != 0)
                {
                    std::cout << "Incorrect segment summary info found!" << std::endl;
                    std::cout << "\tSegment: "    << segment << std::endl;
                    std::cout << "\tBlock: "      << block << std::endl;
                    std::cout << "\tBlock INum: " << blockINum << std::endl;
                    std::cout << "\tShould be: "  << SUMMARY_BLOCK << std::endl;
                    (*errors)++;
                }
            }
            else if (blockINum == IFILE_INUM && summaryBlock->iNodeBlockNumbers[block] != 0)
            {
                (*errors) += checkBlock(segment, block, blockINum, iFileINode);
            }
            else if (blockINum > 0)
            {
                (*errors) += checkBlock(segment, block, blockINum, iFileArray[blockINum - 1]);
            }
        }

        free(summaryBlock->blockINums);
        delete summaryBlock;
    }

	return 0;
}

int checkBlock(unsigned int segment, unsigned int block, int blockINum, INode& inode)
{
    bool blockFound = false;
    for (int dBlock = 0; dBlock < 4; ++dBlock)
    {
        if (inode.directBlocks[dBlock].logSegment  == segment && 
            inode.directBlocks[dBlock].blockNumber == block)
        {
            blockFound = true;
        }
    }

    if (!blockFound)
    {
        printIncorrectSegmentSummaryInfo(segment, block, blockINum, inode);
        return 1;
    }

    return 0;
} 

void printIncorrectSegmentSummaryInfo(unsigned int segment, unsigned int block, int blockINum, INode inode)
{
    std::cout << "Incorrect segment summary info found!" << std::endl;
    std::cout << "\tSegment: "        << segment << std::endl;
    std::cout << "\tBlock: "          << block << std::endl;
    std::cout << "\tBlock INum: "     << blockINum << std::endl;
    std::cout << "\tINode for INum "  << blockINum << ":" << std::endl;
    inode.Print();
}

int readDirectory(INode& inode, DirectoryList * directoryList)
{
    unsigned int dirSizeInBlocks = 0;
    for (int b = 0; b < 4; ++b)
    {
        if (inode.directBlocks[b].logSegment != EMPTY_DIRECT_BLOCK_ADDRESS)
        {
            dirSizeInBlocks++;
        }
    }

    void * dirBuffer = malloc(dirSizeInBlocks * flashData.blockSize * FLASH_SECTOR_SIZE);
    for (int b = 0; b < dirSizeInBlocks; ++b)
    {
        unsigned int segment      = inode.directBlocks[b].logSegment;
        unsigned int block        = inode.directBlocks[b].blockNumber;
        unsigned int bufferOffset = b * flashData.blockSize * FLASH_SECTOR_SIZE;
        readBlock(segment, block, (char *)dirBuffer + bufferOffset);
    }

    DirectoryEntry * directoryEntriesPtr = directoryList->directoryEntries;
    memcpy(directoryList, dirBuffer, sizeof(DirectoryList));
    directoryList->directoryEntries = directoryEntriesPtr;

    directoryList->directoryEntries = (DirectoryEntry *)malloc(directoryList->size * sizeof(DirectoryEntry));
    memset(directoryList->directoryEntries, 0, directoryList->size * sizeof(DirectoryEntry));

    unsigned int entriesOffset = sizeof(DirectoryList);
    memcpy(directoryList->directoryEntries, (char *)dirBuffer + entriesOffset, directoryList->size * sizeof(DirectoryEntry));

    free(dirBuffer);
    return 0;
}

int readSegmentSummaryBlock(unsigned int segment, SegmentSummary * summaryBlock)
{
    void * blockBuffer = malloc(flashData.blockSize * FLASH_SECTOR_SIZE);
    readBlock(segment, 0, blockBuffer);

    int * oldINumsPointer = summaryBlock->blockINums;
    int * oldBlocksPointer = summaryBlock->iNodeBlockNumbers;
    memcpy(summaryBlock, blockBuffer, sizeof(SegmentSummary));
    summaryBlock->blockINums = oldINumsPointer;
    summaryBlock->iNodeBlockNumbers = oldBlocksPointer;

    memcpy(summaryBlock->blockINums, (char *)blockBuffer + sizeof(SegmentSummary), flashData.segmentSize * sizeof(int));
    memcpy(summaryBlock->iNodeBlockNumbers, (char *)blockBuffer + sizeof(SegmentSummary) + flashData.segmentSize * sizeof(int), flashData.segmentSize * sizeof(int));

    free(blockBuffer);
    return 0;
}

int readBlock(unsigned int segment, unsigned int block, void * buffer)
{
    unsigned int sectorToRead          = segment * (flashData.segmentSize * flashData.blockSize) + block * flashData.blockSize;
    unsigned int numberOfSectorsToRead = flashData.blockSize;
    Flash_Read(flash, sectorToRead, numberOfSectorsToRead, buffer);
    return 0;
}

int readSegment(unsigned int segment, void * buffer)
{
    unsigned int sectorToRead          = segment * (flashData.segmentSize * flashData.blockSize);
    unsigned int numberOfSectorsToRead = flashData.blockSize * flashData.segmentSize;
    Flash_Read(flash, sectorToRead, numberOfSectorsToRead, buffer);
    return 0;
}

int readIFile()
{
    std::cout << "[lfsck] reading ifile..." << std::endl;

    unsigned int iFileSizeInBlocks = 0;
    for (int b = 0; b < 4; ++b)
    {
        if (iFileINode.directBlocks[b].logSegment != EMPTY_DIRECT_BLOCK_ADDRESS)
        {
            iFileSizeInBlocks++;
        }
    }

    unsigned int iFileSizeInBytes  = iFileINode.fileSize;
    void * iFileBuffer             = malloc(iFileSizeInBlocks * flashData.blockSize * FLASH_SECTOR_SIZE);
    for (int b = 0; b < iFileSizeInBlocks; ++b)
    {
        unsigned int segment      = iFileINode.directBlocks[b].logSegment;
        unsigned int block        = iFileINode.directBlocks[b].blockNumber;
        unsigned int bufferOffset = b * flashData.blockSize * FLASH_SECTOR_SIZE;
        readBlock(segment, block, (char *)iFileBuffer + bufferOffset);
    }

    iFileArray = (INode *)malloc(iFileSizeInBytes); memset(iFileArray, 0, iFileSizeInBytes);
    memcpy(iFileArray, iFileBuffer, iFileSizeInBytes);
    free(iFileBuffer);
    return 0;
}

int readIFileINode()
{
    std::cout << "[lfsck] reading checkpoint to get ifile inode..." << std::endl;

    unsigned int checkpointBufferSize = CHECKPOINT_SIZE_IN_SECTORS * FLASH_SECTOR_SIZE;
    void * checkpointBuffer           = malloc(checkpointBufferSize);

    Checkpoint curr;
    Checkpoint checkpoint;
    memset(&curr, 0, sizeof(Checkpoint));
    memset(&checkpoint, 0, sizeof(Checkpoint));

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
        }
    }

    free(checkpointBuffer);

    std::cout << "[lfsck] recovered checkpoint:" << std::endl;
    std::cout << "[lfsck] \t time: " << checkpoint.time << std::endl;
    std::cout << "[lfsck] \t lastSegmentWritten: " << checkpoint.lastSegmentWritten << std::endl;
    std::cout << "[lfsck] \t segmentUsageTableSegment: " << checkpoint.segmentUsageTableSegment << std::endl;
    std::cout << "[lfsck] \t IFile INode: " << std::endl;
    iFileINode = checkpoint.iFileINode;
    return 0;
}

int readFlashData(char * flashFile)
{
    // open flash
    std::cout << "[lfsck] reading flash data..." << std::endl;

    Flash_Flags flash_flags = FLASH_SILENT | FLASH_ASYNC; 
    unsigned int blocks;

    flash = Flash_Open(flashFile, flash_flags, &blocks);
    if (flash == NULL)
    {
        std::cerr << "[lfsck] ERROR: Unable to open flash" << std::endl;
        std::cerr << "[lfsck] errno: " << errno << std::endl;
        return 1;
    }

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
        std::cerr << "[lfsck] ERROR: Unable to read flash" << std::endl;
        std::cerr << "[lfsck] errno: " << errno << std::endl;
        return 1;
    }

    flashData = *reinterpret_cast<FlashData *>(flashDataBuffer);
    free(flashDataBuffer);

    std::cout << "\tflash file: "   << flashFile             << std::endl;
    std::cout << "\tblock size: "   << flashData.blockSize   << std::endl;
    std::cout << "\tsegment size: " << flashData.segmentSize << std::endl;
    std::cout << "\tflash size: "   << flashData.flashSize   << std::endl;
    std::cout << "\twear limit: "   << flashData.wearLimit   << std::endl; 
    std::cout << "\tnum blocks: "   << flashData.numBlocks   << std::endl;
    std::cout << "\tcheckpointSegment: "   << flashData.checkpointSegment   << std::endl;

    return 0;
}