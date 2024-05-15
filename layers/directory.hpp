#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include "file.hpp"
#include "../data_structures/directory_list.hpp"
#include "../data_structures/file_type.hpp"


class IDirectoryLayer 
{
public:
	virtual ~IDirectoryLayer(){}
	virtual int Init() = 0;
	virtual int Directory_Statfs(struct statvfs* stbuf) = 0;
	virtual int Directory_Mkdir(const char * path, mode_t mode) = 0;
	virtual int Directory_Readdir(const char * name, char ** files[], unsigned int * numFiles) = 0;
	virtual int Directory_Create(const char * path, mode_t mode) = 0;
	virtual int Directory_Read(const char * path, unsigned int offset, unsigned int size, char * buffer) = 0;
	virtual int Directory_Write(const char * path, unsigned int offset, unsigned int size, const char * buffer) = 0;
	virtual int Directory_GetAttr(const char * path, struct stat *stbuf) = 0;
	virtual int Directory_Exists(const char * path) = 0;
	virtual int Directory_Truncate(const char * path, unsigned int size) = 0;
	virtual int Directory_Chmod(const char * path, mode_t mode) = 0;
	virtual int Directory_Chown(const char * path, uid_t uid, gid_t gid) = 0;
	virtual int Directory_Link(const char * from, const char * to) = 0;
	virtual int Directory_Readlink(const char * path, char * buf, size_t size) = 0;
	virtual int Directory_Symlink(const char * to, const char * from) = 0;
	virtual int Directory_Unlink(const char * path) = 0;
	virtual int Directory_Rmdir(const char * path) = 0;
	virtual int Directory_Rename(const char * from, const char * to) = 0;
	virtual int Directory_CheckPermissions(const char * path, int flags) = 0;
};

class DirectoryLayer : public IDirectoryLayer
{
private:
	IFileLayer * fileLayer;

public:
	DirectoryLayer(char * flashFile, unsigned int cacheSize, unsigned int checkpointInterval, unsigned int cleaningStart, unsigned int cleaningEnd)
	{
    	fileLayer = new FileLayer(flashFile, cacheSize, checkpointInterval, cleaningStart, cleaningEnd);
	}

	~DirectoryLayer()
	{
		delete fileLayer;
	}

	int Init()
	{
    	std::cout << "[DirectoryLayer] Initializing Directory Layer..." << std::endl;
    	fileLayer->Init();
		return 0;
	}

	int Directory_Statfs(struct statvfs* stbuf)
	{
		fileLayer->RunCleaner();
	    stbuf->f_namemax = MAX_FILE_NAME_LENGTH; /* maximum filename length */
		return fileLayer->File_Statfs(stbuf);
	}

	int Directory_Mkdir(const char * path, mode_t mode)
	{
		std::cout << "[DirectoryLayer] Creating new directory: " << path << std::endl;
		fileLayer->RunCleaner();

    	unsigned int directoryINum;
		if (CreateFile(path, FileType::Directory, mode, &directoryINum) != 0)
		{
			return 1;
		}

		const char * directoryName = GetLastPathElement(path).c_str();
		DirectoryList * directory  = new DirectoryList(directoryName, directoryINum);
		WriteDirectory(directory);
    	FreeDirectory(directory);
		return 0;
	}

    int Directory_Readdir(const char * path, char ** files[], unsigned int * numFiles)
    {
    	std::cout << "[DirectoryLayer] Getting Directory: " << path << std::endl;
		fileLayer->RunCleaner();

    	unsigned int directoryINum = GetINum(path);
    	if (directoryINum == INUM_NOT_FOUND)
    	{
    		return -ENOENT;
    	}

    	DirectoryList * directory = ReadDirectory(directoryINum);
    	if (directory == NULL)
    	{
    		return -EIO;
    	}

    	char ** fileNameArray = (char **)malloc(directory->size * sizeof(char *));
    	memset(fileNameArray, 0, directory->size * sizeof(char *));
    	for (int i = 0; i < directory->size; ++i)
    	{
    		unsigned int fileNameSize = strlen(directory->directoryEntries[i].name) + 1;
    		fileNameArray[i] = (char *)malloc(fileNameSize); 
    		memset(fileNameArray[i], 0, fileNameSize);
    		strncpy(fileNameArray[i], directory->directoryEntries[i].name, fileNameSize);
    	}

    	*files = fileNameArray;
    	*numFiles = directory->size;
    	FreeDirectory(directory);
    	return 0;
    }

	int Directory_Create(const char * path, mode_t mode)
	{
		std::cout << "[DirectoryLayer] Creating new file: " << path << std::endl;
		fileLayer->RunCleaner();

 		unsigned int inumOut;
		return CreateFile(path, FileType::File, mode, &inumOut);
	}

	int Directory_Read(const char * path, unsigned int offset, unsigned int size, char * buffer)
    {
    	std::cout << "[DirectoryLayer] Reading Directory Path: " << path << std::endl;
    	std::cout << "[DirectoryLayer] offset: " << offset << " size: " << size << std::endl;
		fileLayer->RunCleaner();

    	unsigned int inum = GetINum(path);
    	if (inum == INUM_NOT_FOUND)
    	{
			std::cerr << "[DirectoryLayer] ERROR: Directory_Read: inum not found for path " << path << std::endl;
    		return 1;
    	}

    	if (fileLayer->File_Read(inum, offset, size, buffer) != 0)
    	{
			std::cerr << "[DirectoryLayer] ERROR: Directory_Read: File_Read failed" << std::endl;
    		return 1;
    	}

    	return 0;
    }

	int Directory_Write(const char * path, unsigned int offset, unsigned int size, const char * buffer)
	{
    	std::cout << "[DirectoryLayer] Writing Directory: " << path << std::endl;
    	std::cout << "[DirectoryLayer] offset: " << offset << " size: " << size << std::endl;
		fileLayer->RunCleaner();

    	unsigned int inum = GetINum(path);
    	if (inum == IFILE_INUM)
    	{
			std::cerr << "[DirectoryLayer] ERROR: Directory_Write: attempting to write iFile" << std::endl;
    		return 1;
    	}

    	if (inum == INUM_NOT_FOUND)
    	{
			std::cerr << "[DirectoryLayer] ERROR: Directory_Write: inum not found for path " << path << std::endl;
    		return 1;
    	}

    	if (fileLayer->File_Write(inum, offset, size, buffer) != 0)
    	{
			std::cerr << "[DirectoryLayer] ERROR: Directory_Write: File_Write failed" << std::endl;
    		return 1;
    	}

    	return 0;
	}

	int Directory_GetAttr(const char * path, struct stat * stbuf)
	{
    	std::cout << "[DirectoryLayer] Directory_GetAttr: " << path << std::endl;
		fileLayer->RunCleaner();

    	memset(stbuf, 0, sizeof(struct stat));

		unsigned int inum = GetINum(path);
		if (inum == INUM_NOT_FOUND)
		{
			return -ENOENT;
		}

		return fileLayer->File_GetAttr(inum, stbuf);
	}

	int Directory_Exists(const char * path)
	{
		fileLayer->RunCleaner();
		return GetINum(path) != INUM_NOT_FOUND ? 0 : -ENOENT;
	}

	int Directory_Truncate(const char * path, unsigned int size)
	{
    	std::cout << "[DirectoryLayer] Directory_Truncate path: " << path << " size: " << size << std::endl;
		fileLayer->RunCleaner();

		unsigned int inum = GetINum(path);

		if (inum == INUM_NOT_FOUND)
		{
			return -ENOENT;
		}

		return fileLayer->File_Truncate(inum, size);
	}

	int Directory_Chmod(const char * path, mode_t mode)
	{
    	std::cout << "[DirectoryLayer] Directory_Chmod path: " << path << " mode: " << mode << std::endl;
		fileLayer->RunCleaner();

		unsigned int inum = GetINum(path);
		if (inum == INUM_NOT_FOUND)
		{ 
			return -ENOENT;
		}

		return fileLayer->File_Chmod(inum, mode) == 0 ? 0 : -EIO;
	}

	int Directory_Chown(const char * path, uid_t uid, gid_t gid)
	{
    	std::cout << "[DirectoryLayer] Directory_Chown path: " << path << " uid: " << uid << " gid: " << gid << std::endl;
		fileLayer->RunCleaner();

		unsigned int inum = GetINum(path);
		if (inum == INUM_NOT_FOUND)
		{ 
			return -ENOENT;
		}

		return fileLayer->File_Chown(inum, uid, gid) == 0 ? 0 : -EIO;
	}

	int Directory_Link(const char * from, const char * to)
	{
		// get inum of from
		std::cout << "[DirectoryLayer] Hard Linking from: " << from << " to: " << to << std::endl;
		fileLayer->RunCleaner();

		unsigned int fromINum = GetINum(from);
    	if (fromINum == INUM_NOT_FOUND)
    	{
    		return -ENOENT;
    	}

		// add entry in dir
		std::string toFileNameStr      = GetLastPathElement(to);
		std::string toDirectoryPathStr = GetDirectoryPath(to);
		const char * toFileName        = toFileNameStr.c_str();
		const char * toDirectoryPath   = toDirectoryPathStr.c_str();

		unsigned int toDirectoryINum = GetINum(toDirectoryPath);
    	if (toDirectoryINum == INUM_NOT_FOUND)
    	{
    		return -ENOENT;
    	}

    	DirectoryList * toDirectory = ReadDirectory(toDirectoryINum);
    	if (toDirectory == NULL)
    	{
    		return -EIO;
    	}

    	toDirectory->AddFile(toFileName, fromINum);

    	if (WriteDirectory(toDirectory) != 0)
    	{
			std::cerr << "[DirectoryLayer] Directory_Link ERROR: writeDirectory failed" << std::endl;
    		return 1;
    	}

    	FreeDirectory(toDirectory);

		// update inode of inums hardlinks
    	fileLayer->File_AddLink(fromINum);
		return 0;
	}

	int Directory_Symlink(const char * to, const char * from)
	{
		// symlink named 'from' evaluated to 'to'
		std::cout << "[DirectoryLayer] Soft Linking to: " << to << " from: " << from << std::endl;
		fileLayer->RunCleaner();
    	
    	unsigned int symlinkINum;
		if (CreateFile(from, FileType::Symlink, 0777, &symlinkINum) != 0)
		{
			return 1;
		}

		return Directory_Write(from, 0, strlen(to), to);
	}

	int Directory_Readlink(const char * path, char * buf, size_t size)
	{
		std::cout << "[DirectoryLayer] Reading link: " << path << " size: " << size << std::endl;
		fileLayer->RunCleaner();

		return Directory_Read(path, 0, size, buf);
	}

	int Directory_Unlink(const char * path)
	{
		std::cout << "[DirectoryLayer] Unlinking: " << path << std::endl;
		fileLayer->RunCleaner();

		unsigned int inum = GetINum(path);
		if (inum == INUM_NOT_FOUND)
    	{
    		return -ENOENT;
    	}

		std::string linkNameStr          = GetLastPathElement(path);
		std::string linkDirectoryPathStr = GetDirectoryPath(path);
		const char * linkName            = linkNameStr.c_str();
		const char * linkDirectoryPath   = linkDirectoryPathStr.c_str();

		unsigned int linkDirectoryINum = GetINum(linkDirectoryPath);
    	if (linkDirectoryINum == INUM_NOT_FOUND)
    	{
    		return -ENOENT;
    	}

    	DirectoryList * directory = ReadDirectory(linkDirectoryINum);
    	if (directory == NULL || strcmp(linkName, ".") == 0 || strcmp(linkName, "..") == 0)
    	{
    		return -EIO;
    	}

    	directory->RemoveFile(linkName);

    	if (WriteDirectory(directory) != 0)
    	{
			std::cerr << "[DirectoryLayer] Directory_Unlink ERROR: writeDirectory failed" << std::endl;
    		return 1;
    	}

    	FreeDirectory(directory);

    	if (fileLayer->File_GetFileType(inum) == FileType::Symlink)
    	{
    		return fileLayer->File_Free(inum);
    	}

    	return fileLayer->File_RemoveLink(inum);
	}

	int Directory_Rmdir(const char * path)
	{
		std::cout << "[DirectoryLayer] Removing Directory: " << path << std::endl;
		fileLayer->RunCleaner();

		unsigned int inum = GetINum(path);
		if (inum == INUM_NOT_FOUND)
		{
			std::cerr << "[DirectoryLayer] Directory_Rmdir Directory not found: " << path << std::endl;
			return -ENOENT;
		}

		DirectoryList * directory = ReadDirectory(inum);
		if (directory == NULL)
		{
			return -EIO;
		}

		if (!directory->IsEmpty())
		{
			std::cerr << "[DirectoryLayer] Directory_Rmdir Directory not empty: " << path << std::endl;
			return -ENOTEMPTY;
		}

		return Directory_Unlink(path);
	}


	int Directory_Rename(const char * from, const char * to)
	{
		unsigned int fromINum = GetINum(from);
		fileLayer->RunCleaner();

		// remove old 'from' dir entry
		std::string fromFileNameStr      = GetLastPathElement(from);
		std::string fromDirectoryPathStr = GetDirectoryPath(from);
		const char * fromFileName        = fromFileNameStr.c_str();
		const char * fromDirectoryPath   = fromDirectoryPathStr.c_str();

		unsigned int fromDirectoryINum = GetINum(fromDirectoryPath);
    	if (fromDirectoryINum == INUM_NOT_FOUND)
    	{
    		std::cerr << "[DirectoryLayer] Directory_Rename ERROR: Directory " << fromDirectoryPath << " doesnt exist" << std::endl;
    		return 1;
    	}

    	DirectoryList * fromDirectory = ReadDirectory(fromDirectoryINum);
    	if (fromDirectory == NULL || strcmp(fromFileName, ".") == 0 || strcmp(fromFileName, "..") == 0)
    	{
    		std::cerr << "[DirectoryLayer] Directory_Rename ERROR: Directory " << fromDirectoryPath << " read failed" << std::endl;
    		return 1;
    	}

    	fromDirectory->RemoveFile(fromFileName);

    	if (WriteDirectory(fromDirectory) != 0)
    	{
			std::cerr << "[DirectoryLayer] Directory_Unlink ERROR: writeDirectory failed" << std::endl;
    		return 1;
    	}

    	FreeDirectory(fromDirectory);

    	// add new 'to' entry
    	std::string toFileNameStr      = GetLastPathElement(to);
		std::string toDirectoryPathStr = GetDirectoryPath(to);
		const char * toFileName        = toFileNameStr.c_str();
		const char * toDirectoryPath   = toDirectoryPathStr.c_str();

		unsigned int toDirectoryINum = GetINum(toDirectoryPath);
    	if (toDirectoryINum == INUM_NOT_FOUND)
    	{
    		std::cerr << "[DirectoryLayer] Directory_Rename ERROR: Directory " << toDirectoryPath << " doesnt exist" << std::endl;
    		return 1;
    	}

    	DirectoryList * toDirectory = ReadDirectory(toDirectoryINum);
    	if (toDirectory == NULL|| strcmp(toFileName, ".") == 0 || strcmp(toFileName, "..") == 0)
    	{
    		std::cerr << "[DirectoryLayer] Directory_Rename ERROR: Directory " << toDirectoryPath << " read failed" << std::endl;
    		return 1;
    	}

    	toDirectory->AddFile(toFileName, fromINum);

    	if (WriteDirectory(toDirectory) != 0)
    	{
			std::cerr << "[DirectoryLayer] Directory_Unlink ERROR: writeDirectory failed" << std::endl;
    		return 1;
    	}

    	FreeDirectory(toDirectory);
    	return 0;
	}

	int Directory_CheckPermissions(const char * path, int flags)
	{
		std::cout << "[DirectoryLayer] Checking permissions: " << path << std::endl;
		fileLayer->RunCleaner();

		struct stat stbuf; memset(&stbuf, 0, sizeof(struct stat));
		Directory_GetAttr(path, &stbuf);

		uid_t uid = getuid();
		uid_t gid = getgid();
	    uid_t fileuid = stbuf.st_uid;
	    gid_t filegid = stbuf.st_gid;

		if (uid == 0)
		{
			return 0;
		}

		mode_t permissions = stbuf.st_mode;
		int ret            = 0;

		if (uid == fileuid)
		{
			if (flags & R_OK)
			{
				ret = (permissions & S_IRUSR) > 0 ? 0 : 1;
			}
			if (flags & W_OK)
			{
				ret = (permissions & S_IWUSR) > 0 ? 0 : 1;
			}
			if (flags & X_OK)
			{
				ret = (permissions & S_IXUSR) > 0 ? 0 : 1;
			}
		}
		else if (gid == filegid)
		{
			if (flags & R_OK)
			{
				ret = (permissions & S_IRGRP) > 0 ? 0 : 1;
			}
			if (flags & W_OK)
			{
				ret = (permissions & S_IWGRP) > 0 ? 0 : 1;
			}
			if (flags & X_OK)
			{
				ret = (permissions & S_IXGRP) > 0 ? 0 : 1;
			}
		}
		else
		{
			if (flags & R_OK)
			{
				ret = (permissions & S_IROTH) > 0 ? 0 : 1;
			}
			if (flags & W_OK)
			{
				ret = (permissions & S_IWOTH) > 0 ? 0 : 1;
			}
			if (flags & X_OK)
			{
				ret = (permissions & S_IXOTH) > 0 ? 0 : 1;
			}
		}

		return ret != 0 ? -EACCES : 0;
	}

private:
	unsigned int GetINum(const char * path)
	{
		unsigned int pathLen   = strlen(path) + 1;
		char * pathCopy        = (char *) malloc(pathLen);

		memset(pathCopy, '\0', pathLen);
		strcpy(pathCopy, path);

	    char delimiterString[2] = { PATH_DELIMITER, '\0' };
    	char * currToken        = strtok(pathCopy, delimiterString); 

    	if (currToken == NULL)
    	{
    		return ROOT_DIRECTORY_INUM;
    	}

		DirectoryList * currDir = ReadDirectory(ROOT_DIRECTORY_INUM); 

		while (currToken != NULL)
    	{
    		if (!currDir->ContainsFile(currToken))
    		{
				std::cerr << "[DirectoryLayer] ERROR GetINum(): Directory " << currDir->directoryNameINumPair.name << " does not contain entry " << currToken << std::endl;
    			return INUM_NOT_FOUND;
    		}

    		unsigned int nextINum = currDir->GetINum(currToken);
        	char * nextToken      = strtok(NULL, delimiterString); 

        	if (nextToken == NULL)
        	{
    			std::cout << "[DirectoryLayer] Found INum for path " << path << " : " << nextINum << std::endl;
        		FreeDirectory(currDir);
        		return nextINum;
        	}

        	FreeDirectory(currDir);
        	currDir = ReadDirectory(nextINum);
        	if (currDir == NULL)
        	{
				std::cerr << "[DirectoryLayer] ERROR GetINum(): Unable to read directory " << currToken << std::endl;
        		return INUM_NOT_FOUND;
        	}

        	currToken = nextToken;
    	}

    	return INUM_NOT_FOUND;
	}

	DirectoryList * ReadDirectory(unsigned int inum)
	{
    	std::cout << "[DirectoryLayer] Reading directory with INum " << inum << std::endl;
    	FileType fileType = fileLayer->File_GetFileType(inum);
    	switch(fileType)
    	{
	        case FileType::Directory:
    			break;
    		case FileType::File:
	        case FileType::Symlink:
	        	std::cerr << "[DirectoryLayer] ERROR ReadDirectory(): inum " << inum << " not a directory" << std::endl;
				return NULL;
	        default:
	        	std::cerr << "[DirectoryLayer] ERROR ReadDirectory() unknown file type: " << fileType << std::endl;
        		throw;
    	}

		// read directory list struct
		DirectoryList * toRet = (DirectoryList *) malloc(sizeof(DirectoryList));
		memset(toRet, 0, sizeof(DirectoryList));
		if (fileLayer->File_Read(inum, 0, sizeof(DirectoryList), toRet) != 0)
		{
			std::cerr << "[DirectoryLayer] ERROR: Failed reading directory list on ReadDirectory" << std::endl;
			free(toRet);
	        return NULL;
		}

		// read directory entries
		toRet->directoryEntries = (DirectoryEntry *)malloc(toRet->size * sizeof(DirectoryEntry));
		memset(toRet->directoryEntries, 0, toRet->size * sizeof(DirectoryEntry));

		unsigned int entriesOffset = sizeof(DirectoryList);
		if (fileLayer->File_Read(inum, entriesOffset, toRet->size * sizeof(DirectoryEntry), toRet->directoryEntries) != 0)
		{
			std::cerr << "[DirectoryLayer] ERROR: Failed reading directory entries on ReadDirectory" << std::endl;
			FreeDirectory(toRet);
	        return NULL;
		}

		return toRet;
	}

	int WriteDirectory(DirectoryList * directory)
	{
    	std::cout << "[DirectoryLayer] Writing directory " << directory->directoryNameINumPair.name << std::endl;

		unsigned int inum      = directory->directoryNameINumPair.inum;
		unsigned int byteSize  = sizeof(DirectoryList) + directory->size * sizeof(DirectoryEntry);
		void * directoryBuffer = malloc(byteSize);
		memset(directoryBuffer, 0, byteSize);
		memcpy(directoryBuffer, directory, sizeof(DirectoryList));
		memcpy((char *)directoryBuffer + sizeof(DirectoryList), directory->directoryEntries, directory->size * sizeof(DirectoryEntry));

		int ret = 0;
		if (fileLayer->File_Write(inum, 0, byteSize, directoryBuffer) != 0)
		{
			std::cerr << "[DirectoryLayer] ERROR: Failed writing directory on WriteDirectory" << std::endl;
			ret = 1;
		}

		free(directoryBuffer);
		return ret;
	}

	void FreeDirectory(DirectoryList * directory)
	{
		free(directory->directoryEntries);
		free(directory);
	}

	int CreateFile(const char * path, FileType fileType, mode_t mode, unsigned int * inumOut)
	{
		std::string fileNameStr      = GetLastPathElement(path);
		std::string directoryPathStr = GetDirectoryPath(path);
		const char * fileName        = fileNameStr.c_str();
		const char * directoryPath   = directoryPathStr.c_str();

		if (strlen(fileName) >= MAX_FILE_NAME_LENGTH)
    	{
    		std::cerr << "[DirectoryLayer] CreateFile ERROR: File name length > MAX_FILE_NAME_LENGTH" << std::endl;
    		return 1;
    	}

    	unsigned int fileInum;
    	if (fileLayer->File_Create(fileType, mode, &fileInum) != 0)
    	{
    		std::cerr << "[DirectoryLayer] CreateFile ERROR: File_Create failed" << std::endl;
    		return 1;
    	}

    	unsigned int directoryINum = GetINum(directoryPath);
    	if (directoryINum == INUM_NOT_FOUND)
    	{
    		std::cerr << "[DirectoryLayer] CreateFile ERROR: Directory " << path << " doesnt exist" << std::endl;
    		return 1;
    	}

    	DirectoryList * directory = ReadDirectory(directoryINum);
    	if (directory == NULL)
    	{
    		std::cerr << "[DirectoryLayer] CreateFile ERROR: Directory " << path << " read failed" << std::endl;
    		return 1;
    	}
    	
    	directory->AddFile(fileName, fileInum);

    	if (WriteDirectory(directory) != 0)
    	{
			std::cerr << "[DirectoryLayer] CreateFile ERROR: writeDirectory failed" << std::endl;
    		return 1;
    	}

    	FreeDirectory(directory);
    	*inumOut = fileInum;
		return 0;
	}

	std::string GetLastPathElement(const char * pathStr)
	{
		std::string path(pathStr);
		size_t found = path.find_last_of("/");
		return path.substr(found + 1);
	}

	std::string GetDirectoryPath(const char * pathStr)
	{
		std::string path(pathStr);
		size_t found = path.find_last_of("/");
  		return path.substr(0, found);
	}
};