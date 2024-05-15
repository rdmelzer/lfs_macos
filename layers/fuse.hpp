#pragma once

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/statvfs.h>
#include "directory.hpp"

class IFuseLayer
{
public:
	virtual ~IFuseLayer(){}
	virtual void * Fuse_Init(struct fuse_conn_info *conn) = 0;
	virtual void Fuse_Destroy(void * private_data) = 0;
	virtual int Fuse_Statfs(const char * path, struct statvfs * stbuf) = 0;
	virtual int Fuse_Getattr(const char * path, struct stat * st) = 0;
	virtual int Fuse_Readlink(const char * path, char * buf, size_t size) = 0;
	virtual int Fuse_Read(const char * path, char *buf, size_t size, off_t offset, struct fuse_file_info * fi) = 0;
	virtual int Fuse_Access(const char * path, int mask) = 0;
	virtual int Fuse_Opendir(const char * path, struct fuse_file_info * fi) = 0;
	virtual int Fuse_Readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi) = 0;
	virtual int Fuse_Releasedir(const char * path, struct fuse_file_info *fi) = 0;
	virtual int Fuse_Mkdir(const char * path, mode_t mode) = 0;
	virtual int Fuse_Symlink(const char * to, const char * from) = 0; 
	virtual int Fuse_Unlink(const char * path) = 0;
	virtual int Fuse_Rmdir(const char * path) = 0;
	virtual int Fuse_Rename(const char * from, const char * to) = 0;
	virtual int Fuse_Link(const char * from, const char * to) = 0;
	virtual int Fuse_Chmod(const char * path, mode_t mode) = 0;
	virtual int Fuse_Chown(const char * path, uid_t uid, gid_t gid) = 0;
	virtual int Fuse_Truncate(const char * path, off_t size) = 0;
	virtual int Fuse_Create(const char * path, mode_t mode, struct fuse_file_info *) = 0;
	virtual int Fuse_Write(const char * path, const char *buf, size_t size, off_t offset, struct fuse_file_info * fi) = 0;
	virtual int Fuse_Release(const char * path, struct fuse_file_info * fi) = 0;
	virtual int Fuse_Open(const char * path, struct fuse_file_info * fi) = 0;
};

class FuseLayer : public IFuseLayer
{
private:
	IDirectoryLayer * directoryLayer;

public:
	FuseLayer(char * flashFile, unsigned int cacheSize, unsigned int checkpointInterval, unsigned int cleaningStart, unsigned int cleaningEnd)
	{
		directoryLayer = new DirectoryLayer(flashFile, cacheSize, checkpointInterval, cleaningStart, cleaningEnd);
	}
	
	~FuseLayer()
	{
		delete directoryLayer;
	}
	
	void * Fuse_Init(struct fuse_conn_info *conn)
	{
		directoryLayer->Init();
	    return NULL;
	}

	void Fuse_Destroy(void * private_data)
	{
	}

	int Fuse_Statfs(const char * path, struct statvfs * stbuf) // have function for this in lower layer
	{
	    stbuf->f_fsid = 0; /* file system ID */
	    stbuf->f_flag = 0; /* mount flags */
		return directoryLayer->Directory_Statfs(stbuf);
	}

	int Fuse_Getattr(const char *path, struct stat *st)
	{
	    return directoryLayer->Directory_GetAttr(path, st);
	}

	int Fuse_Link(const char * from, const char * to)
	{
	    return directoryLayer->Directory_Link(from, to);
	}

	int Fuse_Readlink(const char * path, char * buf, size_t size)
	{
	    return directoryLayer->Directory_Readlink(path, buf, size);
	}

	int Fuse_Symlink(const char * to, const char * from)
	{
	    return directoryLayer->Directory_Symlink(to, from);
	}

	int Fuse_Unlink(const char * path)
	{
	    return directoryLayer->Directory_Unlink(path);
	}

	int Fuse_Read(const char * path, char *buf, size_t size, off_t offset, struct fuse_file_info * fi)
	{
	    return directoryLayer->Directory_Read(path, offset, size, buf) != 0 ? 0 : size;
	}

	int Fuse_Access(const char * path, int mask)
	{
		int ret = directoryLayer->Directory_Exists(path);
		if (ret != 0)
		{
			return -ENOENT;
		}
		
		if (mask == F_OK)
		{
	    	return 0;
		}
		else
		{
			return directoryLayer->Directory_CheckPermissions(path, mask);
		}
		
	}

	int Fuse_Readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi)
	{
	    char ** files;
	    unsigned int numFiles = 0;
	  	int ret = directoryLayer->Directory_Readdir(path, &files, &numFiles);
	    if (ret != 0)
	    {
	    	return ret;
	    }

	    for (int i = 0; i < numFiles; ++i)
	    {
	        filler(buf, files[i], NULL, 0);
	    }

	    free(files);
	    return 0;
	}

	int Fuse_Mkdir(const char * path, mode_t mode)
	{
	    return directoryLayer->Directory_Mkdir(path, mode) == 0 ? 0 : -1;
	}

	int Fuse_Chmod(const char * path, mode_t mode)
	{
		return directoryLayer->Directory_Chmod(path, mode);
	}

	int Fuse_Chown(const char * path, uid_t uid, gid_t gid)
	{
		return directoryLayer->Directory_Chown(path, uid, gid);
	}

	int Fuse_Truncate(const char * path, off_t size)
	{
	    return directoryLayer->Directory_Truncate(path, size) != 0 ? -1 : 0;
	}

	int Fuse_Create(const char * path, mode_t mode, struct fuse_file_info *)
	{
	    return directoryLayer->Directory_Create(path, mode) != 0 ? -1 : 0;
	}

	int Fuse_Write(const char * path, const char *buf, size_t size, off_t offset, struct fuse_file_info * fi)
	{
	    return directoryLayer->Directory_Write(path, offset, size, buf) != 0 ? 0 : size;
	}

	int Fuse_Opendir(const char * path, struct fuse_file_info * fi)
	{
		return directoryLayer->Directory_Exists(path) != 0;
	}

	int Fuse_Open(const char * path, struct fuse_file_info * fi)
	{
	    return directoryLayer->Directory_Exists(path) != 0;
	}

	int Fuse_Release(const char * path, struct fuse_file_info *fi)
	{
	    return 0; // do nothing
	}

	int Fuse_Releasedir(const char * path, struct fuse_file_info *fi)
	{
	    return 0; // do nothing
	}

	int Fuse_Rmdir(const char * path)
	{
	    return directoryLayer->Directory_Rmdir(path) != 0 ? -1 : 0;
	}

	int Fuse_Rename(const char * from, const char * to)
	{
	    return directoryLayer->Directory_Rename(from, to);
	}
};