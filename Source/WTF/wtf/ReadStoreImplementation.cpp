/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include <wtf/ReadStoreImplementationMMap.h>

#include <wtf/text/WTFString.h>
#include "MemoryMappedFileUtils.h"

#include <wtf/DataLog.h>

namespace WTF {

/*static */Ref<ReadStoreImplementationOnMemoryMappedFile> ReadStoreImplementationOnMemoryMappedFile::create(const char* byteCodeStorePath) {
    Ref<ReadStoreImplementationOnMemoryMappedFile> instance(WTF::adoptRef(*new ReadStoreImplementationOnMemoryMappedFile()));
    instance->finishCreation(byteCodeStorePath);
    return instance;
}

ReadStoreImplementationOnMemoryMappedFile::ReadStoreImplementationOnMemoryMappedFile() {
    // Nothing for now ..
}

void ReadStoreImplementationOnMemoryMappedFile::finishCreation(const char* byteCodeStorePath) {
    String pathString(byteCodeStorePath);
    m_memMappingSucceeded = mapWholeFileForRead(byteCodeStorePath, &m_mappedBuffer, &m_mappedSize);
}

ReadStoreImplementationOnMemoryMappedFile::~ReadStoreImplementationOnMemoryMappedFile() {
    if(isAvailable() && m_mappedBuffer != nullptr) {
        unmapFile(m_mappedBuffer, m_mappedSize);
    }
}

bool ReadStoreImplementationOnMemoryMappedFile::isAvailable() {
    return m_memMappingSucceeded && m_mappedBuffer != nullptr && m_mappedSize > 0;
}

void ReadStoreImplementationOnMemoryMappedFile::readBytes(char* buffer, size_t size) {
    ASSERT(m_mappedBuffer);
    ASSERT(m_pointer + size <= m_mappedSize);

    std::memcpy(buffer, m_mappedBuffer + m_pointer, size);
    m_pointer += size;
}

void ReadStoreImplementationOnMemoryMappedFile::readVector(char** buffer, size_t size) {
    ASSERT(m_mappedBuffer);
    ASSERT(m_pointer + size <= m_mappedSize);

    *buffer = reinterpret_cast<char*> (m_mappedBuffer + m_pointer);
    m_pointer += size;
}

void ReadStoreImplementationOnMemoryMappedFile::seekOffsetFromEnd(size_t offset) {
    ASSERT(m_mappedBuffer);
    m_pointer = getSize() - offset;
}

void ReadStoreImplementationOnMemoryMappedFile::seekOffset(size_t offset) {
    ASSERT(m_mappedBuffer);
    ASSERT(offset <= m_mappedSize);

    m_pointer = offset;
}

size_t ReadStoreImplementationOnMemoryMappedFile::getSize() {
    return m_mappedSize;
}

}
