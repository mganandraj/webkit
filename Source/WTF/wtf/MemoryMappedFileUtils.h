#pragma once

#include <wtf/RefPtr.h>
#include <wtf/RefCounted.h>

#if OS(WINDOWS)
    
#include "windows.h"

namespace WTF {

class FileMapping : public WTF::RefCounted<FileMapping> {
public:
    WTF_EXPORT void unmap();
    WTF_EXPORT uint8_t* getBuffer();
    WTF_EXPORT size_t getSize();
    
    WTF_EXPORT ~FileMapping();

    WTF_EXPORT static RefPtr<FileMapping> FileMapping::createForFile(const String& localPath);
    WTF_EXPORT static RefPtr<FileMapping> FileMapping::createForFileSegment(int fd, size_t offset, size_t size);


private:
    HANDLE m_hFile;
    HANDLE m_hMMFile;
    void* m_mappedAddress {nullptr};
    size_t m_mappedSize {0};
};

}

#else

namespace WTF {

class FileMapping : public RefCounted<FileMapping> {
public:
    WTF_EXPORT void unmap();
    WTF_EXPORT uint8_t* getBuffer();
    WTF_EXPORT size_t getSize();
    
    WTF_EXPORT ~FileMapping();

    WTF_EXPORT static RefPtr<FileMapping> createForFile(const String& localPath);
    WTF_EXPORT static RefPtr<FileMapping> createForFileSegment(int fd, size_t offset, size_t size);

private:
    void* m_mappedAddress {nullptr};
    size_t m_mappedSize {0};
};

}

#endif