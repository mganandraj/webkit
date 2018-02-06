/*
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
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

#pragma once

#include <fstream>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Ref.h>

#include <wtf/text/WTFString.h>

#include <wtf/ReadStoreImplementation.h>
#include <wtf/MemoryMappedFileUtils.h>

using namespace WTF;

namespace WTF {

class UnlinkedFunctionExecutable;
class ProgramExecutable;
class FunctionExecutable;
class VM;    

class ReadStoreImplementationOnMemoryMappedFile : public ReadStoreImplementation {
public:
    bool isAvailable() override;
    void readBytes(char*, size_t) override;
    void readVector(char**, size_t) override;
    void seekOffset(size_t offset) override;
    void seekOffsetFromEnd(size_t offset) override;
    size_t getSize() override;
    void destroy() override;

    WTF_EXPORT static Ref<ReadStoreImplementationOnMemoryMappedFile> create(const char* byteCodeStorePath);
private:
    WTF_MAKE_NONCOPYABLE(ReadStoreImplementationOnMemoryMappedFile);

    ~ReadStoreImplementationOnMemoryMappedFile();
    ReadStoreImplementationOnMemoryMappedFile();
    void finishCreation(const char* byteCodeStorePath);
    //void unmap();

    //uint8_t* m_mappedBuffer {nullptr};
    //size_t m_mappedSize {0} ;
    //bool m_memMappingSucceeded = false;
    RefPtr<FileMapping> m_FileMapping;

    String m_storeFilePath;

    size_t m_pointer = 0;
};

} // namespace JSC
