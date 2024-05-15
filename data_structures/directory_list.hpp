#pragma once

#include <string.h>
#include <iostream>

#define INUM_NOT_FOUND UINT_MAX
#define MAX_FILE_NAME_LENGTH 255

static const char PATH_DELIMITER = '/';

typedef struct DirectoryEntry
{
	char             name[MAX_FILE_NAME_LENGTH];
	unsigned int     inum;
} DirectoryEntry;

typedef struct DirectoryList
{
	DirectoryEntry   directoryNameINumPair;
	unsigned int     size;
	DirectoryEntry * directoryEntries;

	DirectoryList(const char * name, unsigned int inum)
	{
		memset(directoryNameINumPair.name, '\0', MAX_FILE_NAME_LENGTH);

		strcpy(directoryNameINumPair.name, name);
		directoryNameINumPair.inum = inum;
		size = 2; 

		directoryEntries = (DirectoryEntry *)malloc((size) * sizeof(DirectoryEntry));
		memset(directoryEntries, 0, (size) * sizeof(DirectoryEntry));
		strcpy(directoryEntries[0].name, ".");
		strcpy(directoryEntries[1].name, "..");
		directoryEntries[0].inum = inum;
		directoryEntries[1].inum = inum;
	}

	int AddFile(const char * fileName, unsigned int inum) // CHECK FOR DUPLICATE FILES???
	{
    	std::cout << "[DirectoryList] AddFile: " << fileName << " " << inum << std::endl;

		DirectoryEntry * oldEntries = directoryEntries;
		DirectoryEntry * newEntries = (DirectoryEntry *)malloc((size + 1) * sizeof(DirectoryEntry));
		memset(newEntries, 0, (size + 1) * sizeof(DirectoryEntry));
		memcpy(newEntries, oldEntries, size * sizeof(DirectoryEntry));

		strcpy(newEntries[size].name, fileName);
		newEntries[size].inum = inum;

		directoryEntries = newEntries;
		size++;
		free(oldEntries);
		return 0;
	}

	int RemoveFile(const char * fileName)
	{
    	std::cout << "[DirectoryList] RemoveFile: " << fileName << std::endl;
    	if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0)
    	{
    		std::cout << "[DirectoryList] Cannot remove: " << fileName << std::endl;
    		return 1;
    	}

    	DirectoryEntry * oldEntries = directoryEntries;
		DirectoryEntry * newEntries = (DirectoryEntry *)malloc((size - 1) * sizeof(DirectoryEntry));
		memset(newEntries, 0, (size - 1) * sizeof(DirectoryEntry));

		bool found = false;
		for (int i = 0, copyIndex = 0; i < size; i++)
		{
			char * entryName = directoryEntries[i].name;
			if (strncmp(fileName, entryName, strlen(entryName) + 1) == 0)
			{
				found = true;
				continue;
			}

			memcpy(&newEntries[copyIndex], &oldEntries[i], sizeof(DirectoryEntry));
			copyIndex++;
		}

		if (found == true)
		{
			directoryEntries = newEntries;
			size--;
			free(oldEntries);
			return 0;
		}

		return 1;
	}

	bool ContainsFile(const char * fileName)
	{
		for (int i = 0; i < size; i++)
		{
			char * entryName = directoryEntries[i].name;
			if (strncmp(fileName, entryName, strlen(entryName) + 1) == 0)
			{
				return true;
			}
		}

		return false;
	}

	unsigned int GetINum(const char * fileName)
	{
		for (int i = 0; i < size; i++)
		{
			char * entryName = directoryEntries[i].name;
			if (strncmp(fileName, entryName, strlen(entryName) + 1) == 0)
			{
				return directoryEntries[i].inum;
			}
		}

		return INUM_NOT_FOUND;
	}

	bool IsEmpty()
	{
		if (size != 2)
		{
			return false;
		}

		return ContainsFile(".") && ContainsFile("..");
	}

	void Print()
	{
    	std::cout << "[DirectoryList] Printing Directory: " << directoryNameINumPair.name << std::endl;

		for (int i = 0; i < size; ++i)
		{
    		std::cout << "[DirectoryList] \tname: " << directoryEntries[i].name << std::endl;
    		std::cout << "[DirectoryList] \tinum: " << directoryEntries[i].inum << std::endl;
		}
	}
} DirectoryList;