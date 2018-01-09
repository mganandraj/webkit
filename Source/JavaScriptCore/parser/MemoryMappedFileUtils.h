#pragma once

bool mapFileSegmentForRead(int fd, size_t offset, size_t size, uint8_t** data);
bool mapWholeFileForRead(const String& localPath, uint8_t** data, size_t* size);

bool unmapFile(void* address, size_t length);

//size_t getFileSize(int fd);
