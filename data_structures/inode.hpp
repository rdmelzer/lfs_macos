#pragma once

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "log_address.hpp"
#include "file_type.hpp"

#define INITIAL_IFILE_SIZE 1
#define IFILE_INUM 0
#define ROOT_DIRECTORY_INUM (IFILE_INUM + 1)
#define EMPTY_DIRECT_BLOCK_ADDRESS UINT_MAX

typedef struct INode
{
    bool           inUse;
    unsigned int   inum;
    FileType       fileType;
    unsigned int   fileSize;        // in bytes
    nlink_t        nlinks;
    uid_t          uid;             // user
    gid_t          gid;             // group
    unsigned short permissions;     // 9 bits - rwx rwx rwx
    LogAddress     directBlocks[4]; // First 4 blocks of file
    LogAddress     indirectBlock;
    time_t         atime;   // Time of last access. 
    time_t         mtime;   // Time of last data modification. 
    time_t         ctime;   // Time of last status change 

    bool operator !=(INode& rhs)
    {
        if (inUse         != rhs.inUse                  ||
            inum          != rhs.inum                   ||
            fileType      != rhs.fileType               ||
            fileSize      != rhs.fileSize               ||
            nlinks        != rhs.nlinks                 ||
            uid           != rhs.uid                    ||
            gid           != rhs.gid                    ||
            atime         != rhs.atime                  ||
            mtime         != rhs.mtime                  ||
            ctime         != rhs.ctime                  ||
            permissions   != rhs.permissions            ||
            indirectBlock != rhs.indirectBlock)
        {
            return true;
        }

        for (int b = 0; b < 4; ++b)
        {
            if (directBlocks[b] != rhs.directBlocks[b])
            {
                return true;
            }
        }

        return false;
    }

    void Print()
    {
        std::cout << "\t[iNode] inUse: " << inUse << std::endl;
        std::cout << "\t[iNode] inum: " << inum << std::endl;

        std::cout << "\t[iNode] fileType: " << GetFileTypeString(fileType) << std::endl;
        std::cout << "\t[iNode] fileSize: " << fileSize << std::endl;
        std::cout << "\t[iNode] nlinks: " << nlinks << std::endl;
        std::cout << "\t[iNode] uid: " << uid << std::endl;
        std::cout << "\t[iNode] gid: " << gid << std::endl;
        std::cout << "\t[iNode] permissions: " << permissions << std::endl;

        for (int b = 0; b < 4; ++b)
        {
            std::cout << "\t[iNode] direct block " << b << std::endl;
            directBlocks[b].Print();
        }

        std::cout << "\t[iNode] indirect block: " << std::endl;
        indirectBlock.Print();
    }
} inode;