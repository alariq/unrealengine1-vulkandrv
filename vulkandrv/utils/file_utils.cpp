#include "file_utils.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h>
#include <string.h>
#include <cassert>

#include "stream.h"
#include "logging.h"

namespace filesystem {

#ifdef LINUX_BUILD
const char kPathSeparatorAsChar = '/';
const char *const kPathSeparator = "/";
#else
const char kPathSeparatorAsChar = '\\';
const char *const kPathSeparator = "\\";
#endif

uint64_t get_file_mod_time_ms(const char* fname)
{
    struct stat fi = {0};
    stat(fname, &fi);
#if defined(PLATFORM_WINDOWS)
	return fi.st_mtime * 100;
#else
	return fi.st_mtim.tv_sec * 1000 + fi.st_mtim.tv_nsec / 1000000;
#endif
}

std::string get_path(const char* fname)
{
    const char* pathend = strrchr(fname, filesystem::kPathSeparatorAsChar);
    if(!pathend)
        return std::string();
    else
        return std::string(fname, pathend - fname);
}


const char* loadfile(const char* fname, size_t* out_size/* = nullptr*/)
{
    assert(fname);
    stream* pstream = stream::makeFileStream();
    if(0 != pstream->open(fname,"rb"))
    {
        log_error("Can't open %s \n", fname);
        delete pstream;
        return 0;
    }

    pstream->seek(0, stream::S_END);
    size_t size = pstream->tell();
    pstream->seek(0, stream::S_SET);

    char* pdata = new char[size + 1];
    size_t rv = pstream->read(pdata, 1, size);
    assert(rv==size);
    pdata[size] = '\0';
    if(out_size)
        *out_size = size;

    pstream->close();
    delete pstream;

    return pdata;
}

}


