#include <assert.h>
#include <string>
#include <iostream>
#include <cstring>
#include "test_utils.hpp"
#include "../layers/log.hpp"
#include "../layers/file.hpp"

char flashFile[]     = "flash_file";
int segmentCacheSize = 3;
unsigned int checkpointInterval = 4;

void Setup()
{
    Mklfs(flashFile);
}

void Teardown()
{
	DeleteTestFlash(flashFile);
}

void TestSegmentFill(Log * log)
{
    std::cout << "\nTestSegmentFill\n" << std::endl;
	unsigned int blocksToFill = 32 - 4;
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
		assert(block + 4 == addr.blockNumber);
    	std::cout << "wrote block: " << block + 6 << std::endl;
	}

	log->PrintTail();
	assert(4 == log->getTailSegmentNumber());
	free(buffer);
}

void TestMoreWritesAfterSegmentFill(Log * log)
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

void TestInitialize2ndLog(Log * log)
{
    std::cout << "\nTestInitialize2ndLog\n" << std::endl;
    delete log;

    Log log2(flashFile, segmentCacheSize, checkpointInterval);
    log2.Init();
	log2.PrintTail();
	assert(4 == log2.getTailSegmentNumber());
}

void TestWriteOneCheckpointAndRecover()
{
    std::cout << "\nTestWriteOneCheckpointAndRecover\n" << std::endl;

	Log * log2 = new Log(flashFile, segmentCacheSize, checkpointInterval);
    log2->Init();
	assert(4 == log2->getTailSegmentNumber());

	unsigned int blocksToFill = 4*32;
	unsigned int inum = 3;
	void * buffer = malloc(512 * 2);
	for (int block = 0; block < blocksToFill; ++block)
	{
		LogAddress addr;
		char s[] = "Test write one checkpoint\n";
		memset(buffer, 0, 512 * 2);
		memcpy(buffer, s, sizeof(s)); 
		assert(0 == log2->Log_Write(inum, block, buffer, &addr));
	}

	assert(8 == log2->getTailSegmentNumber());

	delete log2;

	Log log3(flashFile, segmentCacheSize, checkpointInterval);
    log3.Init();
	assert(8 == log3.getTailSegmentNumber());
}

void TestWriteMultipleCheckpointsAndRecover()
{
    std::cout << "\nTestWriteMultipleCheckpointsAndRecover\n" << std::endl;

    Log * log2 = new Log(flashFile, segmentCacheSize, checkpointInterval);
    log2->Init();
	assert(8 == log2->getTailSegmentNumber());

	unsigned int blocksToFill = 12*32;
	unsigned int inum = 3;
	void * buffer = malloc(512 * 2);
	for (int block = 0; block < blocksToFill; ++block)
	{
		LogAddress addr;
		char s[] = "Test write one checkpoint\n";
		memset(buffer, 0, 512 * 2);
		memcpy(buffer, s, sizeof(s)); 
		assert(0 == log2->Log_Write(inum, block, buffer, &addr));
	}

	assert(20 == log2->getTailSegmentNumber());

	delete log2;

	Log log3(flashFile, segmentCacheSize, checkpointInterval);
    log3.Init();
	assert(20 == log3.getTailSegmentNumber());
}

void RunTests()
{
	Setup();
	
	Log * log = new Log(flashFile, segmentCacheSize, checkpointInterval);
	log->Init();

	TestSegmentFill(log);
	TestMoreWritesAfterSegmentFill(log);
    TestInitialize2ndLog(log);
    TestWriteOneCheckpointAndRecover();
    TestWriteMultipleCheckpointsAndRecover();
    Teardown();
}

int main(int argc, char **argv)
{
	RunTests();
	return 0;
}