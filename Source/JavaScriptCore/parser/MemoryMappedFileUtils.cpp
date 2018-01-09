#include "config.h"
#include "MemoryMappedFileUtils.h"

#if OS(WINDOWS)
    
#include "windows.h"
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/CString.h>

bool mapFileSegmentForRead(int fd, size_t offset, size_t size, uint8_t** data) {
    ASSERT(0);
    return false;
}

bool mapWholeFileForRead(const String& localPath, uint8_t** data, size_t* size) {
	const char* localPathCStr= localPath.ascii().data();
    OFSTRUCT of;
	HANDLE hFile = (HANDLE)OpenFile(localPathCStr, &of, OF_READ);
    if(hFile == INVALID_HANDLE_VALUE) {
        dataLogLn("mapWholeFileForRead : File can't be opened for mapping ...");
        return false;
    }
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    *size = static_cast<size_t>(fileSize.QuadPart);
        
    HANDLE hMMFile = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if(hMMFile == NULL) {
        dataLogLn("mapWholeFileForRead : File opened but failed to memory map ...");
        return false;
    }
            
    void* lpMMFile = MapViewOfFile(hMMFile, FILE_MAP_READ, 0, 0, 0);
    if(lpMMFile == nullptr) {
        dataLogLn("mapWholeFileForRead : mapped address is nullptr ...");
        return false;
    }

    *data = reinterpret_cast<uint8_t*>(lpMMFile);
    dataLogLn("File mapping completed ...");
    return true;
}

bool unmapFile(void* address, size_t length) {
    UnmapViewOfFile (address);
    return true;
    // TODO :: close handle.    
}

#else // Not windows.. must be POSIX

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

bool mapFileSegmentForRead(int fd, size_t offset, size_t size, uint8_t** data) {
    const static auto ps = getpagesize();
    dataLogLn("mapFileSegmentForRead : ", fd, " : ", offset, " : ", size, " : ", ps);

    size_t pageOff;             // The offset in the mmaped region to the data.
    off_t mapOff;               // The offset in the file to the mmaped region.  
    size_t newsize;

    if (offset != 0) {
        auto d  = lldiv(offset, ps);
  
        mapOff  = d.quot;
        pageOff = d.rem;
        newsize    = size + pageOff;  
    } else {
        mapOff  = 0;
        pageOff = 0;
        newsize    = size;
    }

    dataLogLn("mapWholeFileForRead adjusted : ", fd, " : ", mapOff, " : ", pageOff, " : ", newsize);

    void* mapped = mmap(0, newsize, PROT_READ, MAP_SHARED, fd, mapOff * ps);

    dataLogLn("mapWholeFileForRead mapped address : ", reinterpret_cast<uint32_t>(mapped));

    *data = reinterpret_cast<uint8_t*>(mapped) + pageOff;

    dataLogLn("mapWholeFileForRead 1 :", (*data)[0], (*data)[1]);

    char buffer[20];
    memcpy(buffer, *data, 10);
    buffer[10]='\0';

    dataLogLn("mapWholeFileForRead mapped address : ", reinterpret_cast<uint32_t>(mapped), " : " , buffer);

    return true;
}

bool mapWholeFileForRead(const String& localPath, uint8_t** data, size_t* size) {
    const char* localPathCStr= localPath.ascii().data();
    int fd = open(localPathCStr, O_RDONLY);
    if (fd < 0) {
        dataLogLn("ByteCode store doesn't exist : ", localPathCStr);
        return false;
    }
    
    struct stat sb;    
    int ret = fstat(fd, &sb);
    if(ret < 0) {
        int errnoval = errno;        
        dataLogLn("Unable to stat byte code store file : ", ret, " : ", errnoval);
        return false;
    }

    dataLogLn("Mapped file Size: ", (uint64_t)sb.st_size);
    *size = static_cast<size_t>(sb.st_size);
    
    void* mappedAddr = mmap(NULL, sb.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (mappedAddr == MAP_FAILED) {
        int errnoval = errno;        
        dataLogLn("Unable to map bytecode store file : ", errnoval);
        return false;
    }

    *data = reinterpret_cast<uint8_t*>(mappedAddr);
    dataLogLn("File mapping completed : ", reinterpret_cast<uint32_t>(*data));

    return true;
}    

bool unmapFile(void* address, size_t length) {
    int ret = munmap(address, length);
    dataLogLnIf(ret < 0, "Failed unmap memory mapped region !!");
    return (ret == 0);
}

#endif
