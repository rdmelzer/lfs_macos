#pragma once

enum FileType
{
	File,
	Directory,
    Symlink
};

std::string GetFileTypeString(FileType fileType)
{
    switch (fileType)
    {
        case FileType::File:
            return "File";
        case FileType::Directory:
            return "Directory";
        case FileType::Symlink:
            return "Symlink";
        default:
            std::cerr << "[GetFileTypeString] unknown file type: " << fileType << std::endl;
            throw;
    }
}

mode_t GetFileTypeMode(FileType fileType)
{
    switch (fileType)
    {
        case FileType::File:
            return S_IFREG;
        case FileType::Directory:
            return S_IFDIR;
        case FileType::Symlink:
            return S_IFLNK;
        default:
            std::cerr << "[GetFileTypeMode] unknown file type: " << fileType << std::endl;
            throw;
    }
}