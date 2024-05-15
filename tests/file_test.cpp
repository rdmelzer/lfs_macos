#include <iostream>
#include <string.h>
#include <assert.h>
#include "test_utils.hpp"
#include "../layers/file.hpp"
#include "../data_structures/inode.hpp"
#include "../data_structures/log_address.hpp"

#define BLOCK_SIZE 512 * 2

char flashFile[]     = "flash_file";
int segmentCacheSize = 3;
unsigned int checkpointInterval = 4;


FileLayer * fileLayer;

void Setup()
{
    Mklfs(flashFile);
	
	fileLayer = new FileLayer(flashFile, segmentCacheSize, checkpointInterval, 1, 1);
	fileLayer->Init();
}

void Teardown()
{
	delete fileLayer;
	DeleteTestFlash(flashFile);
}

void TestFileCreateOneFile()
{
    std::cout << "\nTestFileCreateOneFile\n" << std::endl;

	unsigned int inum;
	FileType type = FileType::File;
	mode_t mode = 0744;

	assert(fileLayer->File_Create(type, mode, &inum) == 0);
	assert(inum == 2);

	fileLayer->PrintIFileEntry(1);
	fileLayer->PrintIFileEntry(inum);
}

void TestFileCreateMultipleFiles()
{
    std::cout << "\nTestFileCreateMultipleFiles\n" << std::endl;

	unsigned int inum1;
	FileType type1 = FileType::File;
	unsigned int inum2;
	FileType type2 = FileType::Directory;
	mode_t mode = 0744;

	assert(fileLayer->File_Create(type1, mode, &inum1) == 0);
	assert(inum1 == 3);
	assert(fileLayer->File_Create(type2, mode, &inum2) == 0);
	assert(inum2 == 4);
	fileLayer->PrintIFileEntry(1);
	fileLayer->PrintIFileEntry(inum1);
	fileLayer->PrintIFileEntry(inum2);
}

void TestFileReadEmptyFile()
{
    std::cout << "\nTestFileReadEmptyFile\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 0;
	unsigned int length = 10;
	void * buffer = NULL;

	assert(fileLayer->File_Read(inum, offset, length, buffer) == 1);
}

void TestFileWriteOneBlockThenReadOneBlock()
{
    std::cout << "\nTestFileWriteOneBlockThenReadOneBlock\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 0;
	unsigned int length = BLOCK_SIZE;
	void * writeBuffer = malloc(BLOCK_SIZE); memset(writeBuffer, 'w', BLOCK_SIZE);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);

	void * readBuffer = malloc(BLOCK_SIZE);
	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[BLOCK_SIZE]; memset(expected, 'w', BLOCK_SIZE);

	assert(strncmp((char *) readBuffer, expected, BLOCK_SIZE) == 0);

	free(readBuffer);
}

void TestFileWriteOneBlockThenReadPartialBlock()
{
    std::cout << "\nTestFileWriteOneBlockThenReadPartialBlock\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 100;
	unsigned int length = 1;

	void * writeBuffer = malloc(length); memset(writeBuffer, 'c', length);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);


	void * readBuffer = malloc(1); memset(readBuffer, 0, 1);

	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[1]; memset(expected, 'c', 1);

	assert(strncmp((char *) readBuffer, expected, 1) == 0);

	free(readBuffer);
}

void TestFileWriteOneBlockThenReadBeyondBlock()
{
	std::cout << "\nTestFileWriteOneBlockThenReadBeyondBlock\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 1020;
	unsigned int length = 50; // length is > bytes left in block

	void * readBuffer = malloc(4); memset(readBuffer, 0, 4);

	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[4]; memset(expected, 'w', 4);

	assert(strncmp((char *) readBuffer, expected, 1) == 0);

	free(readBuffer);
}

void TestFileWriteMultipleBlocksThenReadWholeBlocks()
{
	std::cout << "\nTestFileWriteMultipleBlocksThenReadWholeBlocks\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 0;
	unsigned int length = 3 * BLOCK_SIZE;
	void * writeBuffer = malloc(length); memset(writeBuffer, 'w', length);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);

	void * readBuffer = malloc(length);
	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[length]; memset(expected, 'w', length);

	assert(strncmp((char *) readBuffer, expected, length) == 0);

	free(readBuffer);
}

void TestFileWriteAcrossBlocksThenReadAcrossBlocks()
{
	std::cout << "\nTestFileWriteMultipleBlocksThenReadAcrossBlocks\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 1020;
	unsigned int length = 10;
	void * writeBuffer = malloc(length); memset(writeBuffer, 'c', length);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);

	void * readBuffer = malloc(length);
	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[length]; memset(expected, 'c', length);

	assert(strncmp((char *) readBuffer, expected, length) == 0);

	free(readBuffer);
}


void TestFileWriteOverwriteOneBlock()
{
	std::cout << "\nTestFileWriteOverwriteOneBlock\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 0;
	unsigned int length = BLOCK_SIZE;
	void * writeBuffer = malloc(BLOCK_SIZE); memset(writeBuffer, 'x', BLOCK_SIZE);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);

	void * readBuffer = malloc(BLOCK_SIZE);
	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[BLOCK_SIZE]; memset(expected, 'x', BLOCK_SIZE);

	assert(strncmp((char *) readBuffer, expected, BLOCK_SIZE) == 0);

	free(readBuffer);
}

void TestFileWriteOverwriteMultiBlock()
{
	std::cout << "\nTestFileWriteOverwriteMultiBlock\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 0;
	unsigned int length = 2*BLOCK_SIZE;
	void * writeBuffer = malloc(length); memset(writeBuffer, 'z', length);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);

	void * readBuffer = malloc(length);
	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[length]; memset(expected, 'z', length);

	assert(strncmp((char *) readBuffer, expected, length) == 0);

	free(readBuffer);
}

void TestFileWriteOverwriteAcrossMultipleBlocks()
{
	std::cout << "\nTestFileWriteOverwriteAcrossMultipleBlocks\n" << std::endl;

	unsigned int inum = 2;
	unsigned int offset = 25;
	unsigned int length = 2*BLOCK_SIZE;
	void * writeBuffer = malloc(length); memset(writeBuffer, 'y', length);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);

	void * readBuffer = malloc(length);
	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[length]; memset(expected, 'y', length);

	assert(strncmp((char *) readBuffer, expected, length) == 0);

	free(readBuffer);
}

void TestFileGetAttr()
{
	std::cout << "\nTestFileGetAttr\n" << std::endl;

	struct stat stbuf;
	
	memset(&stbuf, 0, sizeof(struct stat));
	assert(fileLayer->File_GetAttr(IFILE_INUM, &stbuf) == 0);
	assert(stbuf.st_mode == (GetFileTypeMode(FileType::File) | 0444));
	assert(stbuf.st_ino == IFILE_INUM);
	assert(stbuf.st_nlink == 1);
	assert(stbuf.st_uid == getuid());
	assert(stbuf.st_gid == getgid());
	assert(stbuf.st_blksize == BLOCK_SIZE);
	assert(stbuf.st_blocks == 1);
	assert(stbuf.st_atime > 0);
	assert(stbuf.st_mtime > 0);
	assert(stbuf.st_ctime > 0);

	memset(&stbuf, 0, sizeof(struct stat));
	assert(fileLayer->File_GetAttr(ROOT_DIRECTORY_INUM, &stbuf) == 0);
	assert(stbuf.st_mode == (GetFileTypeMode(FileType::Directory) | 0777));
	assert(stbuf.st_ino == ROOT_DIRECTORY_INUM);
	assert(stbuf.st_nlink == 3);
	assert(stbuf.st_uid == getuid());
	assert(stbuf.st_gid == getgid());
	assert(stbuf.st_blksize == BLOCK_SIZE);
	assert(stbuf.st_blocks == 2);
	assert(stbuf.st_atime > 0);
	assert(stbuf.st_mtime > 0);
	assert(stbuf.st_ctime > 0);
	
	memset(&stbuf, 0, sizeof(struct stat));
	unsigned int fileInum = 2;
	assert(fileLayer->File_GetAttr(fileInum, &stbuf) == 0);
	assert(stbuf.st_mode == (GetFileTypeMode(FileType::File) | 0744));
	assert(stbuf.st_ino == fileInum);
	assert(stbuf.st_nlink == 1);
	assert(stbuf.st_uid == getuid());
	assert(stbuf.st_gid == getgid());
	assert(stbuf.st_blksize == BLOCK_SIZE);
	assert(stbuf.st_blocks == 3);
	assert(stbuf.st_atime > 0);
	assert(stbuf.st_mtime > 0);
	assert(stbuf.st_ctime > 0);
}

void TestFileChmod()
{
	std::cout << "\nTestFileChmod\n" << std::endl;

	unsigned int fileInum = 2;
	mode_t newMode = S_IRUSR | S_IWUSR | S_IXUSR;
	assert(fileLayer->File_Chmod(fileInum, newMode) == 0);

	struct stat stbuf;
	memset(&stbuf, 0, sizeof(struct stat));
	assert(fileLayer->File_GetAttr(fileInum, &stbuf) == 0);
	assert(stbuf.st_mode == (GetFileTypeMode(FileType::File) | 0700));

	assert(stbuf.st_ino == fileInum);
	assert(stbuf.st_nlink == 1);
	assert(stbuf.st_uid == getuid());
	assert(stbuf.st_gid == getgid());
	assert(stbuf.st_blksize == BLOCK_SIZE);
	assert(stbuf.st_blocks == 3);
	assert(stbuf.st_atime > 0);
	assert(stbuf.st_mtime > 0);
	assert(stbuf.st_ctime > 0);
}

void TestFileChown()
{
	std::cout << "\nTestFileChown\n" << std::endl;

	unsigned int fileInum = 2;
	uid_t newUid = 5;
	gid_t newGid = 7;

	assert(fileLayer->File_Chown(fileInum, newUid, newGid) == 0);

	struct stat stbuf;
	memset(&stbuf, 0, sizeof(struct stat));
	assert(fileLayer->File_GetAttr(fileInum, &stbuf) == 0);
	assert(stbuf.st_uid == 5);
	assert(stbuf.st_gid == 7);

	assert(stbuf.st_mode == (GetFileTypeMode(FileType::File) | 0700));
	assert(stbuf.st_ino == fileInum);
	assert(stbuf.st_nlink == 1);
	assert(stbuf.st_blksize == BLOCK_SIZE);
	assert(stbuf.st_blocks == 3);
	assert(stbuf.st_atime > 0);
	assert(stbuf.st_mtime > 0);
	assert(stbuf.st_ctime > 0);
}

void TestFileTruncate()
{
	std::cout << "\nTestFileTruncate\n" << std::endl;

	unsigned int inum;
	FileType type = FileType::File;
	mode_t mode = 0744;

	assert(fileLayer->File_Create(type, mode, &inum) == 0);
	assert(inum == 5);
	fileLayer->PrintIFileEntry(1);
	fileLayer->PrintIFileEntry(inum);

	unsigned int offset = 0;
	unsigned int length = 2 * BLOCK_SIZE;
	void * writeBuffer = malloc(length); memset(writeBuffer, 't', length);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);

	assert(fileLayer->File_Truncate(inum, length - 100) == 0);

	void * readBuffer = malloc(length - 100); memset(readBuffer, 0, length - 100);
	assert(fileLayer->File_Read(inum, 0, length - 100, readBuffer) == 0);
	
	char expected[length - 100]; memset(expected, 't', length - 100);
	assert(strncmp((char *) readBuffer, expected, length - 100) == 0);
	free(readBuffer);
}

void TestGetFileType()
{
	std::cout << "\nTestGetFileType\n" << std::endl;

	assert(fileLayer->File_GetFileType(0) == FileType::File);
	assert(fileLayer->File_GetFileType(1) == FileType::Directory);
	assert(fileLayer->File_GetFileType(2) == FileType::File);
	assert(fileLayer->File_GetFileType(3) == FileType::File);
	assert(fileLayer->File_GetFileType(4) == FileType::Directory);
	assert(fileLayer->File_GetFileType(5) == FileType::File);
}

void TestFileAddLink()
{
	std::cout << "\nTestFileAddLink\n" << std::endl;

	unsigned int inum = 5;
	fileLayer->File_AddLink(inum);

	struct stat stbuf;
	memset(&stbuf, 0, sizeof(struct stat));
	assert(fileLayer->File_GetAttr(inum, &stbuf) == 0);
	assert(stbuf.st_mode == (GetFileTypeMode(FileType::File) | 0744));
	assert(stbuf.st_ino == inum);
	assert(stbuf.st_nlink == 2);
	assert(stbuf.st_uid == getuid());
	assert(stbuf.st_gid == getgid());
	assert(stbuf.st_blksize == BLOCK_SIZE);
	assert(stbuf.st_blocks == 2);
	assert(stbuf.st_atime > 0);
	assert(stbuf.st_mtime > 0);
	assert(stbuf.st_ctime > 0);
}

void TestFileRemoveLink()
{
	std::cout << "\nTestFileRemoveLink\n" << std::endl;

	unsigned int inum = 5;
	fileLayer->File_RemoveLink(inum);

	struct stat stbuf;
	memset(&stbuf, 0, sizeof(struct stat));
	assert(fileLayer->File_GetAttr(inum, &stbuf) == 0);
	assert(stbuf.st_mode == (GetFileTypeMode(FileType::File) | 0744));
	assert(stbuf.st_ino == inum);
	assert(stbuf.st_nlink == 1);
	assert(stbuf.st_uid == getuid());
	assert(stbuf.st_gid == getgid());
	assert(stbuf.st_blksize == BLOCK_SIZE);
	assert(stbuf.st_blocks == 2);
	assert(stbuf.st_atime > 0);
	assert(stbuf.st_mtime > 0);
	assert(stbuf.st_ctime > 0);

	fileLayer->File_RemoveLink(inum);
	assert(fileLayer->File_GetAttr(inum, &stbuf) == 0);
	assert(stbuf.st_nlink == 0);
}

void TestIndirectBlocksOneLevel()
{
	std::cout << "\nTestIndirectBlocksOneLevel\n" << std::endl;

	unsigned int inum;
	FileType type = FileType::File;
	mode_t mode = 0744;

	assert(fileLayer->File_Create(type, mode, &inum) == 0);
	assert(inum == 5);

	unsigned int numBlocks = 100;
	unsigned int offset    = 0;
	unsigned int length    = numBlocks * BLOCK_SIZE;
	void * writeBuffer     = malloc(length); 
	memset(writeBuffer, 'u', length);
	assert(fileLayer->File_Write(inum, offset, length, writeBuffer) == 0);
	free(writeBuffer);
	fileLayer->PrintIFileEntry(inum);

	void * readBuffer = malloc(length);
	assert(fileLayer->File_Read(inum, offset, length, readBuffer) == 0);
	
	char expected[length]; memset(expected, 'u', length);

	assert(strncmp((char *) readBuffer, expected, length) == 0);

	free(readBuffer);
}

void TestCreateLotsOfFiles()
{
	std::cout << "\nTestCreateLotsOfFiles\n" << std::endl;

	unsigned int inum;
	FileType type = FileType::File;
	mode_t mode = 0744;

	unsigned int n = 20;
	for (int f = 0; f < n ; f++)
	{
		assert(fileLayer->File_Create(type, mode, &inum) == 0);
		assert(inum == f + 6);
	}
}

void RunTests()
{
	Setup();
	TestFileCreateOneFile();
	TestFileCreateMultipleFiles();
	TestFileReadEmptyFile();
	TestFileWriteOneBlockThenReadOneBlock();
	TestFileWriteOneBlockThenReadPartialBlock();
	TestFileWriteOneBlockThenReadBeyondBlock();
	TestFileWriteMultipleBlocksThenReadWholeBlocks();
	TestFileWriteAcrossBlocksThenReadAcrossBlocks();
	TestFileWriteOverwriteOneBlock();
	TestFileWriteOverwriteMultiBlock();
	TestFileWriteOverwriteAcrossMultipleBlocks();
	TestFileGetAttr();
	TestFileChmod();
	TestFileChown();
	TestFileTruncate();
	TestGetFileType();
	TestFileAddLink();
	TestFileRemoveLink();
	TestIndirectBlocksOneLevel();
	TestCreateLotsOfFiles();
	Teardown();
}

int main(int argc, char **argv)
{
	RunTests();
	return 0;
}