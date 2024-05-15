#include <iostream>
#include <string.h>
#include <assert.h>
#include "test_utils.hpp"
#include "../layers/directory.hpp"
#include "../layers/file.hpp"

char flashFile[]     = "flash_file";
int segmentCacheSize = 3;
unsigned int checkpointInterval = 4;

DirectoryLayer * directoryLayer;

void Setup()
{
    Mklfs(flashFile);
    directoryLayer = new DirectoryLayer(flashFile, segmentCacheSize, checkpointInterval, 4, 8);
    directoryLayer->Init();
}

void Teardown()
{
    delete directoryLayer;
    DeleteTestFlash(flashFile);
}

void TestListRoot()
{
    std::cout << "\nTestListRoot\n" << std::endl;

	const char * name = "/";
	char ** files;
	unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 3);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
    	std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
    	free(files[i]);
    }

    free(files);
}

void TestDirectoryCreate()
{
    std::cout << "\nTestDirectoryCreate\n" << std::endl;

	const char * path = "/file1.txt";
    mode_t mode = 0744;
    assert(directoryLayer->Directory_Create(path, mode) == 0);

	const char * name = "/";
	char ** files;
	unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 4);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
    	std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
    	free(files[i]);
    }

    free(files);
}

void TestDirectoryAddMultiple()
{
    std::cout << "\nTestDirectoryAddMultiple\n" << std::endl;

    const char * path1 = "/file2.txt";
    const char * path2 = "/file3.txt";
    mode_t mode        = 0744;
    assert(directoryLayer->Directory_Create(path1, mode) == 0);
    assert(directoryLayer->Directory_Create(path2, mode) == 0);

    const char * name = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 6);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);
}

void TestDirectoryExists()
{
    std::cout << "\nTestDirectoryExists\n" << std::endl;

    const char * path1 = "/";
    const char * path2 = "/file1.txt";
	const char * path3 = "/file4.txt";
    assert(directoryLayer->Directory_Exists(path1) == 0);
    assert(directoryLayer->Directory_Exists(path2) == 0);   
    assert(directoryLayer->Directory_Exists(path3) == -ENOENT);	
}

void TestDirectoryWrite()
{
    std::cout << "\nTestDirectoryWrite\n" << std::endl;

	const char * path   = "/file1.txt";
	char string[]       = "Test directory read";
	unsigned int size   = sizeof(string);
	unsigned int offset = 0;
    assert(directoryLayer->Directory_Write(path, offset, size, string) == 0);
}

void TestDirectoryRead()
{
    std::cout << "\nTestDirectoryRead\n" << std::endl;

	const char * path   = "/file1.txt";
	char result[]       = "Test directory read";
	unsigned int size   = sizeof(result);
	unsigned int offset = 0;
	char * buffer = (char *)malloc(size); memset(buffer, 0, size);
    assert(directoryLayer->Directory_Read(path, offset, size, buffer) == 0);
    assert(strncmp(result, buffer, size) == 0);
    free(buffer);
}

void TestDirectoryTruncate()
{
    std::cout << "\nTestDirectoryTruncate\n" << std::endl;

    const char * path   = "/file1.txt";
    char result[]       = "Test directory read";
    unsigned int size   = sizeof(result) - 6;
    assert(directoryLayer->Directory_Truncate(path, size) == 0);

    char * buffer = (char *)malloc(size+1); memset(buffer, 0, size+1);
    assert(directoryLayer->Directory_Read(path, 0, size+1, buffer) == 0);

    assert(strncmp("Test directory", buffer, size) == 0);
    free(buffer);
}

void TestDirectoryChmod()
{
    std::cout << "\nTestDirectoryChmod\n" << std::endl;

    const char * path1   = "/file1.txt";
    const char * path2   = "/file5.txt";
    mode_t mode = 0700;
    assert(directoryLayer->Directory_Chmod(path1, mode) == 0);
    assert(directoryLayer->Directory_Chmod(path2, mode) == -ENOENT);
}

void TestDirectoryChown()
{
    std::cout << "\nTestDirectoryChown\n" << std::endl;

    const char * path1   = "/file1.txt";
    const char * path2   = "/file5.txt";
    uid_t uid = 5;
    gid_t gid = 5;
    assert(directoryLayer->Directory_Chown(path1, uid, gid) == 0);
    assert(directoryLayer->Directory_Chown(path2, uid, gid) == -ENOENT);
}

void TestDirectoryMkdir()
{
    std::cout << "\nTestDirectoryMkdir\n" << std::endl;

    const char * path = "/test_dir";
    mode_t mode = 0744;
    assert(directoryLayer->Directory_Mkdir(path, mode) == 0);

    const char * rootPath = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(rootPath, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 7);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    assert(strcmp(files[6], "test_dir") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    assert(directoryLayer->Directory_Readdir(path, &files, &numFiles) == 0);

    std::cout << "test_dir Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 2);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);
}

void TestDirectoryCreateNestedDir()
{
    std::cout << "\nTestDirectoryCreateNestedDir\n" << std::endl;

    const char * path = "/test_dir/nested_file1.txt";
    mode_t mode = 0744;
    assert(directoryLayer->Directory_Create(path, mode) == 0);

    const char * rootPath = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(rootPath, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 7);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    assert(strcmp(files[6], "test_dir") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    const char * name = "/test_dir";
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Dir Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 3);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], "nested_file1.txt") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);
}

void TestDirectoryMkdirNestedDir()
{
    std::cout << "\nTestDirectoryMkdirNestedDir\n" << std::endl;

    const char * path = "/test_dir/test_test_dir";
    mode_t mode = 0744;
    assert(directoryLayer->Directory_Mkdir(path, mode) == 0);

    const char * rootPath = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(rootPath, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 7);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    assert(strcmp(files[6], "test_dir") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }


    const char * parentPath = "/test_dir";
    assert(directoryLayer->Directory_Readdir(parentPath, &files, &numFiles) == 0);

    std::cout << "Parent Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 4);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], "nested_file1.txt") == 0);
    assert(strcmp(files[3], "test_test_dir") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    assert(directoryLayer->Directory_Readdir(path, &files, &numFiles) == 0);

    std::cout << "test_test_dir Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 2);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);
}

void TestDirectoryCreateNestedNestedDir()
{
    std::cout << "\nTestDirectoryCreateNestedNestedDir\n" << std::endl;

    const char * path = "/test_dir/test_test_dir/nested_nested_file1.txt";
    mode_t mode = 0744;
    assert(directoryLayer->Directory_Create(path, mode) == 0);

    const char * name = "/test_dir/test_test_dir";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Dir Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 3);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], "nested_nested_file1.txt") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);
}

void TestDirectoryLinkSameToDir()
{
    std::cout << "\nTestDirectoryLinkSameToDir\n" << std::endl;

    const char * fromPath = "/file1.txt";
    const char * toPath   = "/file1_hard_link.txt";

    assert(directoryLayer->Directory_Link(fromPath, toPath) == 0);

    const char * name = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 8);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    assert(strcmp(files[6], "test_dir") == 0);
    assert(strcmp(files[7], "file1_hard_link.txt") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);

    struct stat stbuf;
    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(fromPath, &stbuf) == 0);
    assert(stbuf.st_nlink == 2);

    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(toPath, &stbuf) == 0);
    assert(stbuf.st_nlink == 2);
}

void TestDirectoryLinkDifferentToDir()
{
    std::cout << "\nTestDirectoryLinkDifferentToDir\n" << std::endl;

    const char * fromPath = "/file1.txt";
    const char * toPath   = "/test_dir/test_test_dir/file1_hard_link.txt";

    assert(directoryLayer->Directory_Link(fromPath, toPath) == 0);

    const char * name = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 8);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    assert(strcmp(files[6], "test_dir") == 0);
    assert(strcmp(files[7], "file1_hard_link.txt") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);

    const char * GrandParent = "/test_dir";
    assert(directoryLayer->Directory_Readdir(GrandParent, &files, &numFiles) == 0);

    std::cout << "GrandParent Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 4);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], "nested_file1.txt") == 0);
    assert(strcmp(files[3], "test_test_dir") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    const char * parentPath = "/test_dir/test_test_dir";
    assert(directoryLayer->Directory_Readdir(parentPath, &files, &numFiles) == 0);

    std::cout << "Parent Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 4);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], "nested_nested_file1.txt") == 0);
    assert(strcmp(files[3], "file1_hard_link.txt") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    struct stat stbuf;
    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(fromPath, &stbuf) == 0);
    assert(stbuf.st_nlink == 3);

    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(toPath, &stbuf) == 0);
    assert(stbuf.st_nlink == 3);
}

void TestDirectorySymlinkSameDir()
{
    std::cout << "\nTestDirectorySymlinkSameDir\n" << std::endl;

    const char * toPath = "/file1.txt";
    const char * fromPath   = "/file1_symbolic_link.txt";

    assert(directoryLayer->Directory_Symlink(toPath, fromPath) == 0);

    const char * name = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 9);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    assert(strcmp(files[6], "test_dir") == 0);
    assert(strcmp(files[7], "file1_hard_link.txt") == 0);
    assert(strcmp(files[8], "file1_symbolic_link.txt") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);

    struct stat stbuf;
    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(fromPath, &stbuf) == 0);
    assert(stbuf.st_nlink == 1);

    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(toPath, &stbuf) == 0);
    assert(stbuf.st_nlink == 3);

    unsigned int size   = strlen(toPath) + 1;
    unsigned int offset = 0;
    char * buffer = (char *)malloc(size); memset(buffer, 0, size);
    assert(directoryLayer->Directory_Read(fromPath, offset, size, buffer) == 0);
    assert(strncmp(toPath, buffer, size) == 0);
    free(buffer);
}

void TestDirectoryReadLink()
{
    std::cout << "\nTestDirectoryReadLink\n" << std::endl;

    const char * path = "/file1_symbolic_link.txt";
    const char * result = "/file1.txt";

    size_t size = strlen(result);
    char * buffer = (char *) malloc(size);
    assert(directoryLayer->Directory_Readlink(path, buffer, size) == 0);
    assert(strncmp(result, buffer, size) == 0);
    free(buffer);

    size = strlen(result) - 3;
    buffer = (char *) malloc(size);
    assert(directoryLayer->Directory_Readlink(path, buffer, size) == 0);
    assert(strncmp(result, buffer, size) == 0);
    free(buffer);
}

void TestDirectoryUnlinkHardLink()
{
    std::cout << "\nTestDirectoryUnlinkHardLink\n" << std::endl;

    const char * linkPath = "/file1_hard_link.txt";
    const char * filePath = "/file1.txt";

    assert(directoryLayer->Directory_Unlink(linkPath) == 0);

    const char * name = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 8);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    assert(strcmp(files[6], "test_dir") == 0);
    assert(strcmp(files[7], "file1_symbolic_link.txt") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);

    struct stat stbuf;
    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(linkPath, &stbuf) == -ENOENT);

    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(filePath, &stbuf) == 0);
    assert(stbuf.st_nlink == 2);
}

void TestDirectoryUnlinkSymlink()
{
    std::cout << "\nTestDirectoryUnlinkSymlink\n" << std::endl;

    const char * linkPath = "/file1_symbolic_link.txt";
    const char * filePath = "/file1.txt";

    assert(directoryLayer->Directory_Unlink(linkPath) == 0);

    const char * name = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 7);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "file3.txt") == 0);
    assert(strcmp(files[6], "test_dir") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);

    struct stat stbuf;
    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(linkPath, &stbuf) == -ENOENT);

    memset(&stbuf, 0, sizeof(struct stat));
    assert(directoryLayer->Directory_GetAttr(filePath, &stbuf) == 0);
    assert(stbuf.st_nlink == 2);
}

void TestDirectoryRmdir()
{
    std::cout << "\nTestDirectoryRmdir\n" << std::endl;
    const char * filePath = "/test_dir/test_test_dir";

    assert(directoryLayer->Directory_Unlink("/test_dir/test_test_dir/nested_nested_file1.txt") == 0);
    assert(directoryLayer->Directory_Unlink("/test_dir/test_test_dir/file1_hard_link.txt") == 0);

    assert(directoryLayer->Directory_Rmdir(filePath) == 0);
    assert(directoryLayer->Directory_Exists(filePath) != 0);

    const char * parentPath = "/test_dir";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(parentPath, &files, &numFiles) == 0);

    std::cout << "Parent Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 3);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], "nested_file1.txt") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }
}

void TestDirectoryRmdirNonEmptyDir()
{
    std::cout << "\nTestDirectoryRmdirNonEmptyDir\n" << std::endl;
    const char * filePath = "/test_dir";
    assert(directoryLayer->Directory_Rmdir(filePath) == -ENOTEMPTY);
    assert(directoryLayer->Directory_Exists(filePath) == 0);
}

void TestDirectoryRename()
{
    std::cout << "\nTestDirectoryRename\n" << std::endl;
    const char * from = "/file3.txt";
    const char * to = "/test_dir/nested_file3.txt";

    assert(directoryLayer->Directory_Rename(from, to) == 0);

    const char * name = "/";
    char ** files;
    unsigned int numFiles = 0;
    assert(directoryLayer->Directory_Readdir(name, &files, &numFiles) == 0);

    std::cout << "Root Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 6);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], ".ifile") == 0);
    assert(strcmp(files[3], "file1.txt") == 0);
    assert(strcmp(files[4], "file2.txt") == 0);
    assert(strcmp(files[5], "test_dir") == 0);

    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }

    free(files);

    const char * parentPath = "/test_dir";
    assert(directoryLayer->Directory_Readdir(parentPath, &files, &numFiles) == 0);

    std::cout << "Parent Contents: (num files = " << numFiles << ")" << std::endl;
    assert(numFiles == 4);
    assert(strcmp(files[0], ".") == 0);
    assert(strcmp(files[1], "..") == 0);
    assert(strcmp(files[2], "nested_file1.txt") == 0);
    assert(strcmp(files[3], "nested_file3.txt") == 0);
    for (int i = 0; i < numFiles; ++i)
    {
        std::cout << "\t" << files[i] << std::endl;
    }

    for (int i = 0; i < numFiles; ++i)
    {
        free(files[i]);
    }
}

void TestCleaner()
{
    std::cout << "\nTestCleaner\n" << std::endl;

    std::vector<std::string> filenames;
    for (int i = 0; i < 14; i++)
    {
        std::string filename = std::string("/file") + std::to_string(i);
        filenames.push_back(filename);

        mode_t mode = 0744;
        assert(directoryLayer->Directory_Create(filename.c_str(), mode) == 0);

        unsigned int size = 66000;
        char * buffer = (char *) malloc(size);
        memset(buffer, 'w', size);
        assert(directoryLayer->Directory_Write(filename.c_str(), 0, size, buffer) == 0);

        free(buffer);
    }

    for (int i = 0; i < 14; i++)
    {
        std::string filename = std::string("/file") + std::to_string(i);
        assert(directoryLayer->Directory_Unlink(filename.c_str()) == 0);
    }

    for (int i = 0; i < 14; i++)
    {
        std::string filename = std::string("/file") + std::to_string(i);
        filenames.push_back(filename);

        mode_t mode = 0744;
        assert(directoryLayer->Directory_Create(filename.c_str(), mode) == 0);

        unsigned int size = 66000;
        char * buffer = (char *) malloc(size);
        memset(buffer, 'w', size);
        assert(directoryLayer->Directory_Write(filename.c_str(), 0, size, buffer) == 0);

        free(buffer);
    }

    for (int i = 0; i < 14; i++)
    {
        std::string filename = std::string("/file") + std::to_string(i);
        filenames.push_back(filename);

        unsigned int size = 66000;
        char * readBuffer = (char *)malloc(size); memset(readBuffer, 0, size);
        assert(directoryLayer->Directory_Read(filename.c_str(), 0, size, readBuffer) == 0);

        char * expected = (char *) malloc(size);
        memset(expected, 'w', size);
        assert(strncmp(expected, readBuffer, size) == 0);

        free(readBuffer);
        free(expected);
    }
}

void RunTests()
{
    Setup();
	TestListRoot();
    TestDirectoryCreate();
	TestDirectoryAddMultiple();
	TestDirectoryExists();
	TestDirectoryWrite();
    TestDirectoryRead();
    TestDirectoryTruncate();
    TestDirectoryChmod();
    TestDirectoryChown();
	TestDirectoryMkdir();
    TestDirectoryCreateNestedDir();
    TestDirectoryMkdirNestedDir();
    TestDirectoryCreateNestedNestedDir();
    TestDirectoryLinkSameToDir();
    TestDirectoryLinkDifferentToDir();
    TestDirectorySymlinkSameDir();
    TestDirectoryReadLink();
    TestDirectoryUnlinkHardLink();
    TestDirectoryUnlinkSymlink();
    TestDirectoryRmdir();
    TestDirectoryRmdirNonEmptyDir();
    TestDirectoryRename();
    Teardown();

    Setup();
    TestCleaner();
    Teardown();

}

int main(int argc, char **argv)
{
	RunTests();
	return 0;
}