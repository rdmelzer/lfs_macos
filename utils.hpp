#pragma once

#include <time.h>

bool isNumber(std::string s) 
{
    if (s.size() == 0)
    {
        return false;
    }

    for (unsigned int i = 0; i < s.size(); i++)
    {
        if ((s[i] >= '0' && s[i] <= '9') == false)
        {
            return false;
        }
    }

    return true;
}

bool isPrefix(std::string a, std::string b)
{
    return std::mismatch(a.begin(), a.end(), b.begin()).first == a.end();
}

unsigned long long NanosSinceEpoch()
{
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return (unsigned long long) spec.tv_sec * 1e9L + (unsigned long long) spec.tv_nsec;
}