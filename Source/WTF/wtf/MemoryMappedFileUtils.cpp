#include "config.h"

#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/CString.h>
#include <wtf/MemoryMappedFileUtils.h>

#include <wtf/dataLog.h>

#if OS(WINDOWS)
    
#include "windows.h"

namespace WTF {

void FileMapping::unmap() {
    UnmapViewOfFile (m_mappedAddress);
    CloseHandle(m_hMMFile);
    CloseHandle(m_hFile);

    m_mappedAddress = nullptr;
    m_mappedSize = 0;
}

uint8_t* FileMapping::getBuffer() {
    return reinterpret_cast<uint8_t*>(m_mappedAddress);
}

size_t FileMapping::getSize() {
    return m_mappedSize;
}

FileMapping::~FileMapping() {
    if(m_mappedAddress) {
        unmap();
    }
}

/*static */RefPtr<FileMapping> FileMapping::createForFile(const String& localPath) {
    
    const char* localPathCStr= localPath.ascii().data();
    OFSTRUCT of;
	HANDLE hFile = (HANDLE)OpenFile(localPathCStr, &of, OF_READ);
    if(hFile == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    size_t size = static_cast<size_t>(fileSize.QuadPart);
        
    HANDLE hMMFile = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if(hMMFile == NULL) {
        dataLogLn("mapWholeFileForRead : File opened but failed to memory map ...");
        return nullptr;
    }
            
    void* lpMMFile = MapViewOfFile(hMMFile, FILE_MAP_READ, 0, 0, 0);
    if(lpMMFile == nullptr) {
        dataLogLn("mapWholeFileForRead : mapped address is nullptr ...");
        return nullptr;
    }
    
    FileMapping* fileMapping = new FileMapping();
    fileMapping->m_hFile = hFile;
    fileMapping->m_hMMFile = hMMFile;
    fileMapping->m_mappedAddress = lpMMFile;
    fileMapping->m_mappedSize = size;

    return WTF::adoptRef(fileMapping);
}

/*static */RefPtr<FileMapping> FileMapping::createForFileSegment(int fd, size_t offset, size_t size) {
    ASSERT(0); // Not implemented for windows.
    return nullptr;
}

}

#else // Not windows.. must be POSIX

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

namespace WTF {

void FileMapping::unmap() {
    int ret = munmap(m_mappedAddress, m_mappedSize);
    dataLogLnIf(ret < 0, "Failed unmap memory mapped region !!");
    
    
    m_mappedAddress = nullptr;
    m_mappedSize = 0;
}

uint8_t* FileMapping::getBuffer() {
    return reinterpret_cast<uint8_t*>(m_mappedAddress);
}

size_t FileMapping::getSize() {
    return m_mappedSize;
}

FileMapping::~FileMapping() {
    if(m_mappedAddress) {
        unmap();
    }
}

/*static */RefPtr<FileMapping> FileMapping::createForFile(const String& localPath) {
    const char* localPathCStr= localPath.ascii().data();
    int fd = open(localPathCStr, O_RDONLY);
    if (fd < 0) {
        dataLogLn("ByteCode store doesn't exist : ", localPathCStr);
        return nullptr;
    }
    
    struct stat sb;    
    int ret = fstat(fd, &sb);
    if(ret < 0) {
        int errnoval = errno;        
        dataLogLn("Unable to stat byte code store file : ", ret, " : ", errnoval);
        return nullptr;
    }

    dataLogLn("Mapped file Size: ", (uint64_t)sb.st_size);
    size_t size = static_cast<size_t>(sb.st_size);
    
    void* mappedAddress = mmap(NULL, sb.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (mappedAddress == MAP_FAILED) {
        int errnoval = errno;        
        dataLogLn("Unable to map bytecode store file : ", errnoval);
        return nullptr;
    }

    //*data = reinterpret_cast<uint8_t*>(mappedAddr);
    dataLogLn("File mapping completed : ", reinterpret_cast<uint32_t>(mappedAddress));

    // fd can be closed now.
    close(fd);

    FileMapping* fileMapping = new FileMapping();
    fileMapping->m_mappedAddress = mappedAddress;
    fileMapping->m_mappedSize = size;

    return WTF::adoptRef(fileMapping);
}

/*static */RefPtr<FileMapping> FileMapping::createForFileSegment(int fd, size_t offset, size_t size) {
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

    // *data = reinterpret_cast<uint8_t*>(mapped) + pageOff;
    // dataLogLn("mapWholeFileForRead 1 :", (*data)[0], (*data)[1]);

    // fd can be closed now.
    close(fd);

    //dataLogLn("mapWholeFileForRead mapped address : ", reinterpret_cast<uint32_t>(mapped), " : " , buffer);
    // return true;

    FileMapping* fileMapping = new FileMapping();
    fileMapping->m_mappedAddress = reinterpret_cast<uint8_t*>(mapped) + pageOff;
    fileMapping->m_mappedSize = size;

    return WTF::adoptRef(fileMapping);


}

/*
WTF_EXPORT bool mapWholeFileForRead(const String& localPath, uint8_t** data, size_t* size) {
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

    // fd can be closed now.
    close(fd);

    return true;
}    

WTF_EXPORT bool unmapFile(void* address, size_t length) {
    int ret = munmap(address, length);
    dataLogLnIf(ret < 0, "Failed unmap memory mapped region !!");
    return (ret == 0);
}

bool deleteFile(const String& localPath){
    return unlink(localPath.ascii().data()) == 0;
}
*/

}

#endif