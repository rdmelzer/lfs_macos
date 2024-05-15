#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#include <fuse.h>
#include <string.h>
#include <iostream>
#include "fuse_handlers.hpp"
#include "utils.hpp"

static struct fuse_operations lfs_oper = {
    .getattr    = lfs_getattr,
    .readlink   = lfs_readlink,
    .mkdir      = lfs_mkdir,
    .unlink     = lfs_unlink,
    .rmdir      = lfs_rmdir,
    .symlink    = lfs_symlink,
    .rename     = lfs_rename,
    .link       = lfs_link,
    .chmod      = lfs_chmod,
    .chown      = lfs_chown,
    .truncate   = lfs_truncate,
    .open       = lfs_open,
    .read       = lfs_read,
    .write      = lfs_write,
    .statfs     = lfs_statfs,
    .release    = lfs_release,
    .opendir    = lfs_opendir,
    .readdir    = lfs_readdir,
    .releasedir = lfs_releasedir,
    .init       = lfs_init,
    .destroy    = lfs_destroy,
    .access     = lfs_access,
    .create     = lfs_create,
    .flag_nullpath_ok = 1,
};

int optionCheck(char * str)
{
    std::string s = str;
    std::string cache = "--cache=";
    std::string interval = "--interval=";
    std::string start = "--start=";
    std::string stop = "--stop=";

    if (s.compare("-f") == 0 ||
        s.compare("-s") == 0 ||
        s.compare("-i") == 0 ||
        s.compare("-c") == 0 ||
        s.compare("-C") == 0 ||
        isPrefix(cache, s)   ||
        isPrefix(interval, s)||
        isPrefix(start, s)   ||
        isPrefix(stop, s))
    {
       return 0;
    } 

    return 1;
}

int parseArgs(int argc, char **argv, unsigned int *cache_size, unsigned int *checkpoint_interval, unsigned int *cleaning_start, unsigned int *cleaning_end)
{
    if (argc < 3)
    {
        std::cerr << "Usage: lfs [options] file mountpoint" << std::endl;
        return 1;
    }

    else if (argc > 3)
    {
        for (int i = 1; i < argc - 2; i++)
        {
            if (optionCheck(argv[i]) != 0)
            {
                std::cerr << "Invalid option: " << argv[i] << "\nValid options: -f, -s, -i, -c, -C, --cache=num, --interval=num, --start=num, --stop=num" << std::endl;
                return 1;
            }

            std::string option = argv[i];
            std::string arg = argv[i + 1];
            std::string delimiter = "=";
            std::string token = option.substr(option.find(delimiter) + 1, option.size());

            if (isPrefix("--", option))
            {
                if (!isNumber(token))
                {
                    std::cerr << "Invalid argument: " << token << " --- Must be a number!" << std::endl;
                    return 1;
                }

                if (isPrefix("--cache=", option))
                {
                    *cache_size = stoi(token);
                    continue;
                } 
                else if (isPrefix("--interval=", option))
                {
                    *checkpoint_interval = stoi(token);
                    continue;
                }
                else if (isPrefix("--start=", option))
                {
                    *cleaning_start = stoi(token);
                    continue;
                }
                else if (isPrefix("--stop=", option))
                {
                    *cleaning_end = stoi(token);
                    continue;
                }
            }

            if(option.compare("-f") == 0)
            {
                continue;
            }

            if (!isNumber(arg))
            {
                std::cerr << "Invalid argument: " << arg << "\nMust be a number" << std::endl;
                return 1;
            }

            if(option.compare("-s") == 0)
            {
                *cache_size = stoi(arg);
            }
            else if(option.compare("-i") == 0)
            {
                *checkpoint_interval = stoi(arg);
            }
            else if(option.compare("-c") == 0)
            {
                *cleaning_start = stoi(arg);
            }
            else if(option.compare("-C") == 0)
            {
                *cleaning_end = stoi(arg);
            }

            i++;
        }
    }
    
    return 0;
}

bool runInForeground(int argc, char *argv[])
{
    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "-f") == 0)
        {
            return true;
        }
    }

    return false;
}

int main(int argc, char *argv[])
{
	unsigned int cacheSize = 4;
	unsigned int checkpointInterval = 4;
	unsigned int cleaningStart = 4;
	unsigned int cleaningEnd = 8;

	char * flashFile;
	char * mountPoint;

	if (parseArgs(argc, argv, &cacheSize, &checkpointInterval, &cleaningStart, &cleaningEnd) != 0)
    {
        return 1;
    }

    flashFile = argv[argc - 2];
    mountPoint = argv[argc - 1];

    LFS_Start(flashFile, cacheSize, checkpointInterval, cleaningStart, cleaningEnd);

    std::cout << "\t[LFS] flash file: " << flashFile << std::endl;
    std::cout << "\t[LFS] mount point: " << mountPoint << std::endl;

    // start fuse
	umask(0);
	char ** nargv = NULL;
    int nargc;

    char fOp[3] = "-f";
    char sOp[3] = "-s";
    char dOp[3] = "-d";


#define NARGS 3

    if (runInForeground(argc, argv))
    {
        nargc    = 5;
        nargv    = (char **) malloc(nargc * sizeof(char*));
        nargv[0] = argv[0];
        nargv[1] = fOp;
        nargv[2] = sOp;
        nargv[3] = dOp;
        nargv[4] = mountPoint;
    }
    else
    {
        nargc    = 4;
        nargv    = (char **) malloc(nargc * sizeof(char*));
        nargv[0] = argv[0];
        nargv[1] = sOp;
        nargv[2] = dOp;
        nargv[3] = mountPoint;
    }

    
    return fuse_main(nargc, nargv, &lfs_oper, NULL);
}