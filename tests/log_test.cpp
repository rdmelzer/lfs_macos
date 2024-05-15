#include <assert.h>
#include <string>
#include <iostream>
#include <cstring>
#include "test_utils.hpp"
#include "../layers/log.hpp"

char flashFile[]     = "flash_file";
int segmentCacheSize = 3;
unsigned int checkpointInterval = 4;
Log * log;

void Setup()
{
	Mklfs(flashFile);

	log = new Log(flashFile, segmentCacheSize, checkpointInterval);
	log->Init();
	log->PrintTailSummary();
}

void Teardown()
{
	delete log;
	DeleteTestFlash(flashFile);
}

void TestShortWrite()
{
    std::cout << "\nTestShortWrite\n" << std::endl;
	unsigned int inum = 2;
	unsigned int block = 1;
	LogAddress addr;
	char s[] = "Test short write\n";
	void * buffer = malloc(512 * 2);
	memset(buffer, 0, 512 * 2);
	memcpy(buffer, s, sizeof(s)); 
	assert(0 == log->Log_Write(inum, block, buffer, &addr));
	log->PrintTail();
	assert(3 == addr.logSegment);
	assert(4 == addr.blockNumber);
	assert(3 == log->getTailSegmentNumber());
	free(buffer);
}

void TestMultiBlockWrite()
{
    std::cout << "\nTestMultiBlockWrite\n" << std::endl;
	unsigned int inum = 2;
	unsigned int block = 1;
	LogAddress addr;
	char s[] = "Test multi block write\n";
	void * buffer = malloc(512 * 2);
	memset(buffer, 0, 512 * 2);
	memcpy(buffer, s, sizeof(s)); 
	assert(0 == log->Log_Write(inum, block, buffer, &addr));
	assert(3 == addr.logSegment);
	assert(5 == addr.blockNumber);
	assert(0 == log->Log_Write(inum, block+1, buffer, &addr));
	assert(3 == addr.logSegment);
	assert(6 == addr.blockNumber);
	log->PrintTail();
	assert(3 == log->getTailSegmentNumber());
	free(buffer);
}

void TestSegmentFill()
{
    std::cout << "\nTestSegmentFill\n" << std::endl;
	unsigned int blocksToFill = 32 - 7;
	unsigned int inum = 2;
	void * buffer = malloc(512 * 2);
	for (int block = 0; block < blocksToFill; ++block)
	{
    	std::cout << "writing block: " << block + 4 << std::endl;
		LogAddress addr;
		char s[] = "Test segment fill write\n";
		memset(buffer, 0, 512 * 2);
		memcpy(buffer, s, sizeof(s)); 
		assert(0 == log->Log_Write(inum, block, buffer, &addr));
		assert(3 == addr.logSegment);
		assert(block + 7 == addr.blockNumber);
    	std::cout << "wrote block: " << block + 6 << std::endl;
	}

	log->PrintTail();
	assert(4 == log->getTailSegmentNumber());
	free(buffer);
}

void TestMoreWritesAfterSegmentFill()
{
    std::cout << "\nTestMoreWritesAfterSegmentFill\n" << std::endl;
	unsigned int inum = 2;
	unsigned int block = 1;
	LogAddress addr;
	char s[] = "Test short write after segment fill\n";
	void * buffer = malloc(512 * 2);
	memset(buffer, 0, 512 * 2);
	memcpy(buffer, s, sizeof(s)); 
	assert(0 == log->Log_Write(inum, block, buffer, &addr));
	log->PrintTail();
	assert(4 == addr.logSegment);
	assert(1 == addr.blockNumber);
	free(buffer);
}

void TestFlashFill()
{
	std::cout << "\nTestFlashFill\n" << std::endl;
	unsigned int blocksToFill = 100 * 32 * 2;
	unsigned int inum = 2;
	void * buffer = malloc(512 * 2);
	for (int block = 0; block < blocksToFill; ++block)
	{
		LogAddress addr;
		char s[] = "Test flash fill write\n";
		memset(buffer, 0, 512 * 2);
		memcpy(buffer, s, sizeof(s)); 
		log->Log_Write(inum, block, buffer, &addr); // assert
	}
	free(buffer);
}

void TestShortReadFromTail()
{
	std::cout << "\nTestShortRead\n" << std::endl;
	unsigned int inum = 2;
	unsigned int block = 1;
	LogAddress addr;
	char s[] = "Test short read\n";
	void * buffer = malloc(512 * 2);
	memset(buffer, 0, 512 * 2);
	memcpy(buffer, s, sizeof(s)); 
	assert(0 == log->Log_Write(inum, block, buffer, &addr));
	assert(3 == addr.logSegment);
	assert(4 == addr.blockNumber);
	assert(3 == log->getTailSegmentNumber());

	memset(buffer, 0, 512 * 2);
	log->Log_Read(addr, buffer);
	std::cout << "block contents: \n\t" << (char *)buffer << std::endl;
	assert(0 == strcmp(s, (char *)buffer));

	log->PrintTailSummary();
	free(buffer);
}

void TestMultiBlockReadFromTail()
{
	std::cout << "\nTestMultiBlockRead\n" << std::endl;
	unsigned int inum = 2;
	unsigned int block = 1;
	LogAddress addr;
	char s1[] = "Test multi block read1\n";
	char s2[] = "Test multi block read2\n";
	void * buffer = malloc(512 * 2);
	memset(buffer, 0, 512 * 2);
	memcpy(buffer, s1, sizeof(s1)); 
	assert(0 == log->Log_Write(inum, block, buffer, &addr));
	assert(3 == addr.logSegment);
	assert(5 == addr.blockNumber);
	memset(buffer, 0, 512 * 2);
	memcpy(buffer, s2, sizeof(s2));
	assert(0 == log->Log_Write(inum, block+1, buffer, &addr));
	assert(3 == addr.logSegment);
	assert(6 == addr.blockNumber);
	assert(3 == log->getTailSegmentNumber());

	addr.blockNumber = 5;
	memset(buffer, 0, 512 * 2);
	log->Log_Read(addr, buffer);
	std::cout << "block 1 contents: \n\t" << (char *)buffer << std::endl;
	assert(0 == strcmp(s1, (char *)buffer));


	addr.blockNumber = 6;
	memset(buffer, 0, 512 * 2);
	log->Log_Read(addr, buffer);
	std::cout << "block 2 contents: \n\t" << (char *)buffer << std::endl;
	assert(0 == strcmp(s2, (char *)buffer));

	log->PrintTailSummary();
	free(buffer);
}

void TestReadSegmentInCache()
{
	std::cout << "\nTestReadSegmentInCache\n" << std::endl;
	unsigned int blocksToFill = 32 - 7;
	unsigned int inum = 2;
	void * buffer = malloc(512 * 2);

	char s[] = "Test read segment in cache\n";
	for (int block = 0; block < blocksToFill; ++block)
	{
		LogAddress addr;
		memset(buffer, 0, 512 * 2);
		memcpy(buffer, s, sizeof(s)); 
		assert(0 == log->Log_Write(inum, block, buffer, &addr));
		assert(3 == addr.logSegment);
		assert(block + 7 == addr.blockNumber);
	}

	assert(4 == log->getTailSegmentNumber());

	LogAddress readAddr = 
	{
		.logSegment = 3,
		.blockNumber = 12
	};
	memset(buffer, 0, 512 * 2);
	log->Log_Read(readAddr, buffer);
	std::cout << "cached block contents: \n\t" << (char *)buffer << std::endl;
	assert(0 == strcmp(s, (char *)buffer));

	log->PrintTailSummary();
	free(buffer);
}

void TestReadSegmentNotInCache()
{
	std::cout << "\nTestReadSegmentNotInCache\n" << std::endl;
	unsigned int blocksToFill = 32 * 3;
	unsigned int inum = 2;
	void * buffer = malloc(512 * 2);

	char s[] = "Test read segment not in cache\n";
	for (int block = 0; block < blocksToFill; ++block)
	{
		LogAddress addr;
		memset(buffer, 0, 512 * 2);
		memcpy(buffer, s, sizeof(s)); 
		assert(0 == log->Log_Write(inum, block, buffer, &addr));
		addr.Print();
	}

	assert(7 == log->getTailSegmentNumber());

	LogAddress readAddr = 
	{
		.logSegment = 3,
		.blockNumber = 12
	};
	memset(buffer, 0, 512 * 2);
	log->Log_Read(readAddr, buffer);
	std::cout << "cached block contents: \n\t" << (char *)buffer << std::endl;
	char s2[] = "Test read segment in cache\n";
	assert(0 == strcmp(s2, (char *)buffer));

	LogAddress readAddrCache = 
	{
		.logSegment = 6,
		.blockNumber = 12
	};
	memset(buffer, 0, 512 * 2);
	log->Log_Read(readAddrCache, buffer);
	std::cout << "cached block contents: \n\t" << (char *)buffer << std::endl;
	assert(0 == strcmp(s, (char *)buffer));

	log->PrintTailSummary();
	free(buffer);
}

void TestReadAfterFree()
{
	std::cout << "\nTestReadAfterFree\n" << std::endl;

	// segment 6 in tail, blocks 1-3 full
	LogAddress addrToFree = 
	{
		.logSegment = 7,
		.blockNumber = 2
	};

	void * buffer = malloc(512 * 2);
	memset(buffer, 0, 512 * 2);

	assert(log->Log_Free(addrToFree) == 0);
	//assert(log->Log_Read(addrToFree, buffer) == 1);

	unsigned int inum = 3;
	char s[] = "Test reading freed block written over\n";
	memcpy(buffer, s, sizeof(s)); 
	LogAddress newAddr; memset(&newAddr, 0, sizeof(LogAddress)); 

	assert(log->Log_Write(inum, addrToFree.blockNumber, buffer, &newAddr) == 0);
	assert(newAddr.logSegment = 7);
	assert(newAddr.blockNumber = 2);

	memset(buffer, 0, 512 * 2);
	assert(log->Log_Read(newAddr, buffer) == 0);
	assert(0 == strcmp(s, (char *)buffer));

	free(buffer);
}

void TestReadSegmentUsageTable()
{
	std::cout << "\nTestReadSegmentUsageTable\n" << std::endl;

	SegmentUsageTableEntry * table = log->ReadSegmentUsageTable();
	log->PrintSegmentUsageTable(table);
	assert(table[0].liveBytesInSegment == 0);
	assert(table[1].liveBytesInSegment == 0);
	assert(table[2].liveBytesInSegment == 0);
	assert(table[0].ageOfYoungestBlock == 0);
	assert(table[1].ageOfYoungestBlock == 0);
	assert(table[2].ageOfYoungestBlock == 0);
	assert(table[3].liveBytesInSegment == 31744);
	assert(table[4].liveBytesInSegment == 31744);
	assert(table[5].liveBytesInSegment == 31744);
	assert(table[6].liveBytesInSegment == 31744);
	assert(table[3].ageOfYoungestBlock > 0);
	assert(table[4].ageOfYoungestBlock > 0);
	assert(table[5].ageOfYoungestBlock > 0);
	assert(table[6].ageOfYoungestBlock > 0);
	for (int i = 7; i < 100; ++i)
	{
		assert(table[i].liveBytesInSegment == 0);
		assert(table[i].ageOfYoungestBlock == 0);
	}
}

void RunWriteTests()
{
	Setup();
	TestShortWrite();
	TestMultiBlockWrite();
	TestSegmentFill();
	TestMoreWritesAfterSegmentFill();
	//TestFlashFill();
	Teardown();
}

void RunReadTests()
{
	Setup();
	TestShortReadFromTail();
	TestMultiBlockReadFromTail();
	TestReadSegmentInCache();
	TestReadSegmentNotInCache();
	TestReadAfterFree();
	TestReadSegmentUsageTable();
	Teardown();
}

void RunTests()
{
	RunWriteTests();
	RunReadTests();
}

int main(int argc, char **argv)
{
	RunTests();
	return 0;
}