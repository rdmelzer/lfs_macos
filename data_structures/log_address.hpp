#pragma once

typedef struct LogAddress
{
    unsigned int logSegment;
    unsigned int blockNumber;

    bool operator !=(LogAddress& rhs)
    {
    	return logSegment != rhs.logSegment || blockNumber != rhs.blockNumber;
    }

    void Print()
    {
        std::cout << "\t\t[LogAddress] logSegment: " << logSegment << std::endl;
        std::cout << "\t\t[LogAddress] blockNumber: " << blockNumber << std::endl;
    }
} LogAddress;