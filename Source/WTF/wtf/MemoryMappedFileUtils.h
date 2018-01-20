#pragma once

namespace WTF {

class String;

WTF_EXPORT bool mapFileSegmentForRead(int fd, size_t offset, size_t size, uint8_t** data);
WTF_EXPORT bool mapWholeFileForRead(const String& localPath, uint8_t** data, size_t* size);

WTF_EXPORT bool unmapFile(void* address, size_t length);

//size_t getFileSize(int fd);

}
