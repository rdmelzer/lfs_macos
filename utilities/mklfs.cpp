/*
 * Utility for the Log layer which creates an empty LFS. It is responsible for
 * initializing all of the on-flash data structures, metadata  and creating the
 * root directory and its initial contents. 
 *
 * USAGE: mklfs [options] file
 */

#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include "../layers/flash/flash.h"
#include "../data_structures/directory_list.hpp"
#include "../data_structures/log_address.hpp"
#include "../data_structures/flash_data.hpp"
#include "../data_structures/segment.hpp"
#include "../data_structures/inode.hpp"
#include "../utils.hpp"

int parseArgs(int argc, char **argv, unsigned int *blockSize, unsigned int *segmentSize, unsigned int *flashSize, unsigned int *wearLimit);
int initFlash(char *file, unsigned int blockSize, unsigned int segmentSize, unsigned int flashSize, unsigned int wearLimit);
int optionCheck(char * s);

int main(int argc, char **argv)
{
    unsigned int blockSize    = 2;    // Size of a block, in sectors. The default is 2 (1KB)
    unsigned int segmentSize  = 32;   // Segment size, in blocks. The segment size must be a multiple of the flash erase block size, report an error otherwise. The default is 32
    unsigned int flashSize    = 100;  // Size of the flash, in segments.  The default is 100
    unsigned int wearLimit    = 1000; // Wear limit for erase blocks. The default is 1000.

    if (parseArgs(argc, argv, &blockSize, &segmentSize, &flashSize, &wearLimit) != 0)
    {
        return 1;
    }

    std::cout << "\tblock size in sectors: "  << blockSize         << std::endl;
    std::cout << "\tsegment size in blocks: " << segmentSize       << std::endl;
    std::cout << "\tflash size in segments: " << flashSize         << std::endl;
    std::cout << "\twear limit: "             << wearLimit         << std::endl;
    
	if (segmentSize % FLASH_SECTORS_PER_BLOCK != 0)
    {
        std::cerr << "Segment size must be a multiple of block size" << std::endl;
        return 1;
    }

    if (sizeof(Checkpoint) > CHECKPOINT_SIZE_IN_SECTORS * FLASH_SECTOR_SIZE)
    {
        std::cerr << "checkpoints must only be " << CHECKPOINT_SIZE_IN_SECTORS << " sectors large" << std::endl;
        throw;
    }

    char *file = argv[argc - 1];
    if (initFlash(file, blockSize, segmentSize, flashSize, wearLimit) != 0)
    {
        return 1;
    }

	return 0;
}

int initFlash(char *file, unsigned int blockSize, unsigned int segmentSize, unsigned int flashSize, unsigned int wearLimit)
{
    std::cout << "initializing flash..."                                 << std::endl;
    std::cout << "\tflash file: "             << file                    << std::endl;
    std::cout << "\tsector size in bytes: "   << FLASH_SECTOR_SIZE       << std::endl;
    std::cout << "\tblock size in sectors: "  << blockSize               << std::endl;
    std::cout << "\tsegment size in blocks: " << segmentSize             << std::endl;
    std::cout << "\tflash size in segments: " << flashSize               << std::endl;
    std::cout << "\twear limit: "             << wearLimit               << std::endl;
    std::cout << "\tblocks: "                 << segmentSize * flashSize << std::endl;

    // Create the flash
    if (Flash_Create(file, wearLimit, segmentSize * flashSize) != 0)
    {
        std::cerr << "wearLimit must be <= 100,000 and blocks must be <= 1,000,000" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }

    // open flash
    Flash_Flags flash_flags = FLASH_SILENT | FLASH_ASYNC; 
    unsigned int blocks;

    Flash flash = Flash_Open(file, flash_flags, &blocks);
    if (flash == NULL)
    {
        std::cerr << "Unable to open flash on initFlash" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }

    // init flash data 
    FlashData flashData = {
        .blockSize                = blockSize,
        .segmentSize              = segmentSize,
        .flashSize                = flashSize,
        .wearLimit                = wearLimit,
        .numBlocks                = segmentSize * flashSize,
    };

    // compute size of flash data in sectors 
    unsigned int flashDataFieldsSizeInBytes   = sizeof(flashData);
    unsigned int flashDataSizeInSectors       = flashDataFieldsSizeInBytes / FLASH_SECTOR_SIZE;
    if (flashDataFieldsSizeInBytes % FLASH_SECTOR_SIZE != 0 || flashDataSizeInSectors == 0)
    {
        flashDataSizeInSectors++;
    }

    // compute size of seg usage table in sectors
    unsigned int segmentUsageTableSizeInBytes   = flashSize * sizeof(SegmentUsageTableEntry);
    unsigned int segmentUsageTableSizeInSectors = segmentUsageTableSizeInBytes / FLASH_SECTOR_SIZE;
    if (segmentUsageTableSizeInBytes % FLASH_SECTOR_SIZE != 0 || segmentUsageTableSizeInSectors == 0)
    {
        segmentUsageTableSizeInSectors++;
    }

    // reserve entire segment for on flash data data
    unsigned int segmentSizeInSectors    = segmentSize * blockSize;
    unsigned int flashDataSizeInSegments = flashDataSizeInSectors / segmentSizeInSectors;
    if (flashDataSizeInSectors % segmentSizeInSectors != 0 || flashDataSizeInSegments == 0)
    {
        flashDataSizeInSegments++;
    }

    // reserve segments for segment usage table
    unsigned int segmentUsageTableSizeInSegments = segmentUsageTableSizeInSectors / segmentSizeInSectors;
    if (segmentUsageTableSizeInSectors % segmentSizeInSectors != 0 || segmentUsageTableSizeInSegments == 0)
    {
        segmentUsageTableSizeInSegments++;
    }

    // checkpoint location
    unsigned int flashDataSegment = 0;
    unsigned int segmentUsageTableSegment = flashDataSegment + flashDataSizeInSegments;
    flashData.checkpointSegment = segmentUsageTableSegment + segmentUsageTableSizeInSegments;

    // write flash data fields
    unsigned int flashDataFieldsSector      = 0; 
    unsigned int flashDataFieldsSectorCount = flashDataSizeInSectors;
    unsigned int bufferSize                 = flashDataFieldsSectorCount * FLASH_SECTOR_SIZE;
    void * flashDataFieldsBuffer            = malloc(bufferSize);
    memcpy(flashDataFieldsBuffer, &flashData, sizeof(flashData));
    if (Flash_Write(flash, flashDataFieldsSector, flashDataFieldsSectorCount, flashDataFieldsBuffer) != 0)
    {
        std::cerr << "Unable to write on-flash data fields on initFlash" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }

    free(flashDataFieldsBuffer);

    // build iFile data
    INode iFile[INITIAL_IFILE_SIZE]; memset(&iFile[0], 0, sizeof(iFile));
    for (int i = 0; i < INITIAL_IFILE_SIZE; ++i)
    {
        iFile[i].inUse = false;
        for (int b = 0; b < 4; ++b)
        {
            iFile[i].directBlocks[b].logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
            iFile[i].directBlocks[b].blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;
        }

        iFile[i].indirectBlock.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
        iFile[i].indirectBlock.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;
    }

    // create iFile iNode, it will be in checkpoint region
    time_t now = time(0);
    INode iFileINode = {
        .inUse       = true,
        .inum        = IFILE_INUM,
        .fileType    = FileType::File,
        .fileSize    = sizeof(iFile),
        .nlinks      = 1,
        .uid         = getuid(),
        .gid         = getgid(),
        .permissions = 0444, // read only
        .atime       = now,
        .mtime       = now,
        .ctime       = now,
    };

    // compute iFile direct block addresses 
    unsigned int initialIFileSizeInBytes  = INITIAL_IFILE_SIZE * sizeof(INode);
    unsigned int initialIFileSizeInBlocks = initialIFileSizeInBytes / (blockSize * FLASH_SECTOR_SIZE);
    if (initialIFileSizeInBytes % (blockSize * FLASH_SECTOR_SIZE) != 0 || initialIFileSizeInBlocks == 0)
    {
        initialIFileSizeInBlocks++;
    }

    if (initialIFileSizeInBlocks > 4)
    {
        std::cerr << "initialIFileSizeInBlocks > 4. Limit on init for ifile size is 4 blocks" << std::endl;
        std::cerr << "initialIFileSizeInBlocks: " << initialIFileSizeInBlocks << std::endl;
        throw;
    }

    unsigned int iFileSegment = flashData.checkpointSegment + 1; // segment after flash data and checkpoint segment
    for (int b = 0; b < 4; ++b)
    {
        if (b < initialIFileSizeInBlocks)
        {
            iFileINode.directBlocks[b].logSegment  = iFileSegment; 
            iFileINode.directBlocks[b].blockNumber = b + 1; // + 1 b/c segment summary block is first
        }
        else
        {
            iFileINode.directBlocks[b].logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS;
            iFileINode.directBlocks[b].blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;
        }
    }

    iFileINode.indirectBlock.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
    iFileINode.indirectBlock.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;
    
    // now we can make checkpoint regions
    Checkpoint initialCheckpoint = {
        .isValid                  = true,
        .time                     = NanosSinceEpoch(),
        .segmentUsageTableSegment = segmentUsageTableSegment,
        .lastSegmentWritten       = iFileSegment,
        .iFileINode               = iFileINode,
    };

    // write checkpoint regions to next segment after flash data segment
    unsigned int checkpointsSector = segmentSizeInSectors * flashData.checkpointSegment;
    void * checkpointBuffer        = malloc(CHECKPOINT_SIZE_IN_SECTORS * FLASH_SECTOR_SIZE);
    memset(checkpointBuffer, 0, CHECKPOINT_SIZE_IN_SECTORS * FLASH_SECTOR_SIZE);
    memcpy(checkpointBuffer, &initialCheckpoint, sizeof(Checkpoint));

    if (Flash_Write(flash, checkpointsSector, CHECKPOINT_SIZE_IN_SECTORS, checkpointBuffer) != 0)
    {
        std::cerr << "Unable to write checkpoints on initFlash" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }

    free(checkpointBuffer);

    // Create root dir with ., .., and .ifile entries
    DirectoryList root("/", ROOT_DIRECTORY_INUM);
    root.AddFile(".ifile", IFILE_INUM);

    // create root dir inode
    iFile[ROOT_DIRECTORY_INUM - 1] = {
        .inUse                       = true,
        .inum                        = ROOT_DIRECTORY_INUM,
        .fileType                    = FileType::Directory,
        .fileSize                    = sizeof(DirectoryList) + 3 * sizeof(DirectoryEntry),
        .nlinks                      = 3, // FIX???
        .uid                         = getuid(),
        .gid                         = getgid(),
        .permissions                 = 0777,
        .atime                       = now,
        .mtime                       = now,
        .ctime                       = now,
    };

    for (int b = 0; b < 4; ++b)
    {
        iFile[ROOT_DIRECTORY_INUM - 1].directBlocks[b].logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
        iFile[ROOT_DIRECTORY_INUM - 1].directBlocks[b].blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;
    }

    iFile[ROOT_DIRECTORY_INUM - 1].indirectBlock.logSegment  = EMPTY_DIRECT_BLOCK_ADDRESS; 
    iFile[ROOT_DIRECTORY_INUM - 1].indirectBlock.blockNumber = EMPTY_DIRECT_BLOCK_ADDRESS;

    unsigned int rootDirSizeInBlocks = iFile[ROOT_DIRECTORY_INUM - 1].fileSize / (blockSize * FLASH_SECTOR_SIZE);
    if (rootDirSizeInBlocks % (blockSize * FLASH_SECTOR_SIZE) != 0 || rootDirSizeInBlocks == 0)
    {
        rootDirSizeInBlocks++;
    }

    if (rootDirSizeInBlocks > 4)
    {
        std::cerr << "Root dir size > 4 blocks on init" << std::endl;
        throw;
    }

    for (int b = 0; b < rootDirSizeInBlocks; ++b)
    {
        iFile[ROOT_DIRECTORY_INUM - 1].directBlocks[b].logSegment  = iFileSegment; // hardcoded to be in ifile segment
        iFile[ROOT_DIRECTORY_INUM - 1].directBlocks[b].blockNumber = initialIFileSizeInBlocks + b + 1;
    }

    // make a segment summary
    unsigned int startSector = iFileSegment * flashData.segmentSize * flashData.blockSize;
    SegmentSummary iFileSegmentSummary(iFileSegment, startSector, segmentSize);
    int b = 1;
    while (b <= initialIFileSizeInBlocks)
    {
        iFileSegmentSummary.blockINums[b] = IFILE_INUM;
        iFileSegmentSummary.iNodeBlockNumbers[b] = b - 1;
        b++;
    }

    while (b <= initialIFileSizeInBlocks + rootDirSizeInBlocks)
    {
        iFileSegmentSummary.blockINums[b] = ROOT_DIRECTORY_INUM;
        iFileSegmentSummary.iNodeBlockNumbers[b] = b - initialIFileSizeInBlocks - 1;
        b++;
    }
    
    // write summary
    std::cout << "Writing ifile log segment to flash. seg num : " << iFileSegment << std::endl;
    std::cout << "ifile block size: " << initialIFileSizeInBlocks << std::endl;

    char * segmentSummaryBlock = (char *)malloc(blockSize * FLASH_SECTOR_SIZE);
    memset(segmentSummaryBlock, 0, blockSize);
    memcpy(segmentSummaryBlock, &iFileSegmentSummary, sizeof(SegmentSummary));
    memcpy(segmentSummaryBlock + sizeof(SegmentSummary), iFileSegmentSummary.blockINums, flashData.segmentSize * sizeof(int));
    memcpy(segmentSummaryBlock + sizeof(SegmentSummary) + flashData.segmentSize * sizeof(int), iFileSegmentSummary.iNodeBlockNumbers, flashData.segmentSize * sizeof(int));
    
    unsigned int summarySector = iFileSegmentSummary.startSector;
    int ret                    = Flash_Write(flash, summarySector, blockSize, segmentSummaryBlock);
    free(segmentSummaryBlock);
    if (ret == 1)
    {
        std::cerr << "Unable to write ifile segment on initFlash" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }
    
    // write ifile
    void * dataBuffer = malloc(initialIFileSizeInBlocks * blockSize * FLASH_SECTOR_SIZE);
    memset(dataBuffer, 0, initialIFileSizeInBlocks * blockSize * FLASH_SECTOR_SIZE);
    memcpy(dataBuffer, iFile, sizeof(iFile));

    unsigned int iFileSector      = summarySector + blockSize;
    unsigned int iFileSectorCount = initialIFileSizeInBlocks * blockSize;
    if (Flash_Write(flash, iFileSector, iFileSectorCount, dataBuffer) == 1)
    {
        std::cerr << "Unable to write ifile on initFlash" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }

    free(dataBuffer);

    std::cout << "summarySector " << summarySector << std::endl;
    std::cout << "blockSize " << blockSize << std::endl;
    std::cout << "iFileSector " << iFileSector << std::endl;
    std::cout << "iFileSectorCount " << iFileSectorCount << std::endl;

    // write root dir
    void * rootDirBuffer = malloc(rootDirSizeInBlocks * blockSize * FLASH_SECTOR_SIZE);
    memset(rootDirBuffer, 0, rootDirSizeInBlocks * blockSize * FLASH_SECTOR_SIZE);
    memcpy(rootDirBuffer, &root, sizeof(DirectoryList));
    memcpy((char *)rootDirBuffer + sizeof(DirectoryList), root.directoryEntries, 3 * sizeof(DirectoryEntry));

    unsigned int rootDirSector      = iFileSector + iFileSectorCount;
    unsigned int rootDirSectorCount = rootDirSizeInBlocks * blockSize;
    if (Flash_Write(flash, rootDirSector, rootDirSectorCount, rootDirBuffer) == 1)
    {
        std::cerr << "Unable to write root dir on initFlash" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }

    std::cout << "rootDirSector " << rootDirSector << std::endl;
    std::cout << "rootDirSectorCount " << rootDirSectorCount << std::endl;

    free(rootDirBuffer);   

    // populate and write initial segment summary table
    SegmentUsageTableEntry * segmentUsageTable = (SegmentUsageTableEntry *)malloc(segmentUsageTableSizeInBytes);
    memset(segmentUsageTable, 0, segmentUsageTableSizeInBytes);
    segmentUsageTable[iFileSegment].liveBytesInSegment = (initialIFileSizeInBlocks + rootDirSizeInBlocks) * blockSize * FLASH_SECTOR_SIZE;
    segmentUsageTable[iFileSegment].ageOfYoungestBlock = now;

    unsigned int segmentUsageTableSector = initialCheckpoint.segmentUsageTableSegment * segmentSize * blockSize;
    unsigned int segmentUsageTableSectorCount = segmentUsageTableSizeInBytes / FLASH_SECTOR_SIZE;
    if (segmentUsageTableSizeInBytes % FLASH_SECTOR_SIZE != 0 || segmentUsageTableSectorCount == 0)
    {
        segmentUsageTableSectorCount++;
    }

    void * segmentUsageTableBuffer = malloc(segmentUsageTableSectorCount * FLASH_SECTOR_SIZE);
    memset(segmentUsageTableBuffer, 0, segmentUsageTableSectorCount * FLASH_SECTOR_SIZE);
    memcpy(segmentUsageTableBuffer, segmentUsageTable, segmentUsageTableSizeInBytes);
    if (Flash_Write(flash, segmentUsageTableSector, segmentUsageTableSectorCount, segmentUsageTableBuffer) != 0)
    {
        std::cerr << "Unable to write segment usage table on initFlash" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }

    free(segmentUsageTableBuffer);

    if (Flash_Close(flash) != 0)
    {
        std::cerr << "Unable to close flash on initFlash" << std::endl;
        std::cerr << "errno: " << errno << std::endl;
        return 1;
    }

    std::cout << "\nflash data fields sector: "       << flashDataFieldsSector                << std::endl;
    std::cout << "flash data fields size: "           << sizeof(FlashData)                    << std::endl;
    std::cout << "flash data fields sector count: "   << flashDataFieldsSectorCount           << std::endl;
    std::cout << "\nsegment usage table sector: "     << segmentUsageTableSector              << std::endl;
    std::cout << "segment usage table size: "         << flashSize * sizeof(SegmentUsageTableEntry) << std::endl;
    std::cout << "segment usage table sector count: " << segmentUsageTableSectorCount         << std::endl;
    std::cout << "\ncheckpoints sector: "             << checkpointsSector                    << std::endl;
    std::cout << "checkpoints size: "                 << sizeof(Checkpoint)                   << std::endl;
    std::cout << "checkpoints sector count: "         << CHECKPOINT_SIZE_IN_SECTORS           << std::endl;

    std::cout << "\nsegment summary size: " << sizeof(SegmentSummary) + 2 * flashData.segmentSize * sizeof(int)<< std::endl;
    std::cout << "\ninode size: " << sizeof(INode) << std::endl;

    unsigned int maxFileSize = (4 * blockSize * FLASH_SECTOR_SIZE) + 
        ((blockSize * FLASH_SECTOR_SIZE) / sizeof(LogAddress)) * blockSize * FLASH_SECTOR_SIZE;
    std::cout << "\nMAX FILE SIZE: " << maxFileSize / 1024 << " KB" << std::endl;
    return 0;
}

int parseArgs(int argc, char **argv, unsigned int *blockSize, unsigned int *segmentSize, unsigned int *flashSize, unsigned int *wearLimit)
{
    if (argc < 2)
    {
        std::cerr << "USAGE: " << argv[0] << " [options] file" << std::endl;
        return 1;
    } 

    else if (argc >= 3)
    {
        for (int i = 1; i < argc - 1; i++)
        {
            if (optionCheck(argv[i]) != 0)
            {
                std::cerr << "Invalid option: " << argv[i] << "\nValid options: -b, -l, -s, -w, --block=size, --segment=size, --segments=segments, --wearLimit=limit" << std::endl;
                return 1;
            }

            std::string option = argv[i];
            std::string arg = argv[i + 1];
            std::string delimiter = "=";
            std::string token = option.substr(option.find(delimiter) + 1, option.size());

            // First case is option is formatted as --option=argument [e.g: --block=200]
            
            if (isPrefix("--", option))
            {
                if (!isNumber(token))
                {
                    std::cerr << "Invalid argument: " << token << " --- Must be a number!" << std::endl;
                    return 1;
                }

                if (isPrefix("--block=", option))
                {
                    *blockSize = stoi(token);
                    continue;
                } 
                else if (isPrefix("--segment=", option))
                {
                    *segmentSize = stoi(token);
                    continue;
                }
                else if (isPrefix("--segments=", option))
                {
                    *flashSize = stoi(token);
                    continue;
                }
                else if (isPrefix("--wearLimit=", option))
                {
                    *wearLimit = stoi(token);
                    continue;
                }
            }
        
            // Second case is option is formatted as -option argument [e.g: -b 20]
            // Must check if the argument is a number.
            if (!isNumber(arg))
            {
                std::cerr << "Invalid argument: " << arg << "\nMust be a number" << std::endl;
                return 1;
            }

            if(option.compare("-b") == 0)
            {
                *blockSize = stoi(arg);
            }
            else if(option.compare("-l") == 0)
            {
                *segmentSize = stoi(arg);
            }
            else if(option.compare("-s") == 0)
            {
                *flashSize = stoi(arg);
            }
            else if(option.compare("-w") == 0)
            {
                *wearLimit = stoi(arg);
            }
            
            i++;
        }
    }

    return 0;
}

int optionCheck(char * str)
{
    std::string s = str;
    std::string b = "--block=";
    std::string l = "--segment=";
    std::string seg = "--segments=";
    std::string w = "--wearLimit=";

    if (s.compare("-b") == 0 ||
        s.compare("-l") == 0 ||
        s.compare("-s") == 0 ||
        s.compare("-w") == 0 ||
        isPrefix(b, s)       ||
        isPrefix(l, s)       ||
        isPrefix(seg, s)     ||
        isPrefix(w, s))
    {
       return 0;
    } 

    return 1;
}
