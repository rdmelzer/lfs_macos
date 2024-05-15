#include <iostream>
#include <string.h>
#include <assert.h>
#include "test_utils.hpp"
#include "../layers/file.hpp"
#include "../data_structures/inode.hpp"
#include "../data_structures/log_address.hpp"

#define BLOCK_SIZE 1024
#define SEGMENT_SIZE 32
#define FLASH_SIZE 100

char flashFile[]                = "flash_file";
int segmentCacheSize            = 3;
unsigned int checkpointInterval = 4;
unsigned int cleaningStart      = 5;
unsigned int cleaningEnd        = 10;

FileLayer * fileLayer;
unsigned int MAX_FILE_SIZE;

void Setup()
{
    Mklfs(flashFile);
	
	MAX_FILE_SIZE = (4 * BLOCK_SIZE) + (BLOCK_SIZE / sizeof(LogAddress)) * BLOCK_SIZE;

	fileLayer = new FileLayer(flashFile, segmentCacheSize, checkpointInterval, cleaningStart, cleaningEnd);
	fileLayer->Init();
}

void Teardown()
{
	delete fileLayer;
	DeleteTestFlash(flashFile);
}

void TestRunCleanerBelowThreshold()
{
	std::cout << "\nTestRunCleanerBelowThreshold\n" << std::endl;
	assert(fileLayer->RunCleaner() == 0);
}

void TestRunCleanerMaxedFiles()
{
	std::cout << "\nTestRunCleanerMaxedFiles\n" << std::endl;
	unsigned int startSegment = 3;
	unsigned int flashSize = (FLASH_SIZE - startSegment) * SEGMENT_SIZE * BLOCK_SIZE;
	unsigned int nFiles = flashSize / MAX_FILE_SIZE - 12;
	std::cout << "nFiles: " << nFiles << std::endl;
	std::cout << "MAX_FILE_SIZE: " << MAX_FILE_SIZE << std::endl;

	unsigned int inum;	
	FileType type = FileType::File;
	mode_t mode = 0744;
	unsigned int offset = 0;
	unsigned int length = MAX_FILE_SIZE;

	for (int file = 0; file < nFiles; ++file)
	{
		assert(fileLayer->File_Create(type, mode, &inum) == 0);
		assert(inum == 2 + file);

		void * writeBuffer = malloc(length); memset(writeBuffer, 'w', length);
		assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
		free(writeBuffer);
	}

	for (int inum = 2; inum < nFiles; inum += 2)
	{
		assert(fileLayer->File_Free(inum) == 0);
	}

	assert(fileLayer->RunCleaner() == 0);

	for (int inum = 2; inum < nFiles; inum++)
	{
		void * readBuffer = malloc(length); memset(readBuffer, 0, length);

		if (inum % 2 == 0)
		{
			assert(fileLayer->File_Read(inum, offset, length, readBuffer) != 0);
			std::cerr << "should get invalid read here" << std::endl;		
			free(readBuffer);
			continue;
		}

		assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);

		char expected[length]; memset(expected, 'w', length);
		assert(strncmp((char *) readBuffer, expected, length) == 0);
		free(readBuffer);
	}

	fileLayer->PrintIFile();
}

void TestCleanerMoreFiles()
{
	std::cout << "\nTestCleanerMoreFiles\n" << std::endl;

	unsigned int nFiles = 23;

	unsigned int inum;	
	FileType type = FileType::File;
	mode_t mode = 0744;
	unsigned int offset = 0;
	unsigned int length = 66000;

	for (int file = 0; file < 17; ++file)
	{
		assert(fileLayer->File_Create(type, mode, &inum) == 0);
		assert(inum == 2 + file);

		void * writeBuffer = malloc(length); memset(writeBuffer, 'w', length);
		assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
		free(writeBuffer);
	}

	assert(fileLayer->File_Free(2) == 0);
	assert(fileLayer->File_Free(3) == 0);
	assert(fileLayer->File_Free(4) == 0);
	assert(fileLayer->File_Free(5) == 0);
	assert(fileLayer->File_Free(6) == 0);
	assert(fileLayer->File_Free(7) == 0);
	
	assert(fileLayer->RunCleaner() == 0);

	for (int file = 17; file < nFiles; ++file)
	{
		assert(fileLayer->File_Create(type, mode, &inum) == 0);

		void * writeBuffer = malloc(length); memset(writeBuffer, 'w', length);
		assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
		free(writeBuffer);
	}

	assert(fileLayer->RunCleaner() == 0);

	fileLayer->PrintIFile();
}

void RunTests()
{
	Setup();
	TestRunCleanerBelowThreshold();
	TestRunCleanerMaxedFiles();
	Teardown();

	Setup();
	TestCleanerMoreFiles();
	Teardown();
}

int main(int argc, char **argv)
{
	RunTests();
	return 0;
}