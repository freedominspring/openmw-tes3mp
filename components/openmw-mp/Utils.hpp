//
// Created by koncord on 24.01.16.
//

#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <sstream>

#if (defined __WIN32__ || defined _WIN32 || defined WIN32)
#define __WINDOWS
#endif

#ifdef __WINDOWS
int setenv(const char *name, const char *value, int overwrite);
#endif

namespace Utils
{
    std::string convertPath(std::string str);

    void timestamp();

    int progress_func(double TotalToDownload, double NowDownloaded);

    bool DoubleCompare(double a, double b, double epsilon);

    std::string str_replace(const std::string &source, const char *find, const char *replace);

    std::string toString(int num);

    std::string &RemoveExtension(std::string &file);

    long int FileLength(const char *file);

    unsigned int crc32checksum(const std::string &file);


    void printWithWidth(std::ostringstream &sstr, std::string str, size_t width);
    std::string intToHexStr(unsigned val);
}
#endif //UTILS_HPP
