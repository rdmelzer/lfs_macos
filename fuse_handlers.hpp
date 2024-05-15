#pragma once

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#include <fuse.h>
#include "layers/fuse.hpp"

IFuseLayer * fuseLayer;

void LFS_Start(char * flashFile, unsigned int cacheSize, unsigned int checkpointInterval, unsigned int cleaningStart, unsigned int cleaningEnd)
{
    fuseLayer = new FuseLayer(flashFile, cacheSize, checkpointInterval, cleaningStart, cleaningEnd);
}

void * lfs_init(struct fuse_conn_info *conn)
{
    return fuseLayer->Fuse_Init(conn);
}

void lfs_destroy(void * private_data)
{
    delete fuseLayer;
}

int lfs_statfs(const char* path, struct statvfs* stbuf)
{
    return fuseLayer->Fuse_Statfs(path, stbuf);
}

int lfs_getattr(const char *path, struct stat *st)
{
    return fuseLayer->Fuse_Getattr(path, st);
}

int lfs_readlink(const char* path, char* buf, size_t size)
{
    return fuseLayer->Fuse_Readlink(path, buf, size);
}

int lfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    return fuseLayer->Fuse_Read(path, buf, size, offset, fi);
}

int lfs_opendir(const char* path, struct fuse_file_info* fi)
{
    return fuseLayer->Fuse_Opendir(path, fi);
}

int lfs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
    return fuseLayer->Fuse_Readdir(path, buf, filler, offset, fi);
}

int lfs_releasedir(const char* path, struct fuse_file_info *fi)
{
    return fuseLayer->Fuse_Releasedir(path, fi);
}

int lfs_mkdir(const char* path, mode_t mode)
{
    return fuseLayer->Fuse_Mkdir(path, mode);
}

int lfs_symlink(const char* to, const char* from)
{
    return fuseLayer->Fuse_Symlink(to, from);
}

int lfs_unlink(const char* path)
{
    return fuseLayer->Fuse_Unlink(path);
}

int lfs_rmdir(const char* path)
{
    return fuseLayer->Fuse_Rmdir(path);
}

int lfs_rename(const char* from, const char* to)
{
    return fuseLayer->Fuse_Rename(from, to);
}

int lfs_link(const char* from, const char* to)
{
    return fuseLayer->Fuse_Link(from, to);
}

int lfs_chmod(const char* path, mode_t mode)
{
    return fuseLayer->Fuse_Chmod(path, mode);
}

int lfs_chown(const char* path, uid_t uid, gid_t gid)
{
    return fuseLayer->Fuse_Chown(path, uid, gid);
}

int lfs_truncate(const char* path, off_t size)
{
    return fuseLayer->Fuse_Truncate(path, size);
}

int lfs_create(const char* path, mode_t mode, struct fuse_file_info *fi)
{
    return fuseLayer->Fuse_Create(path, mode, fi);
}

int lfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    return fuseLayer->Fuse_Write(path, buf, size, offset, fi);
}

int lfs_release(const char* path, struct fuse_file_info *fi)
{
    return fuseLayer->Fuse_Release(path, fi);
}

int lfs_access(const char * path, int mask)
{
    return fuseLayer->Fuse_Access(path, mask);
}

int lfs_open(const char* path, struct fuse_file_info* fi)
{
    return fuseLayer->Fuse_Open(path, fi);
}