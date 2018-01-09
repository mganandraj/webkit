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

#include "ByteCodeWriteStore.h"
#include "WriteStoreImplementationOnFileStream.h"

#include "ByteCodeStoreUtils.h"
#include "MemoryMappedFileUtils.h"

#include <wtf/DataLog.h>

namespace JSC {

/*static*/ Ref<WriteStoreImplementationOnFileStream> WriteStoreImplementationOnFileStream::create(const char* byteCodeStorePath) {
    Ref<WriteStoreImplementationOnFileStream> writeStoreImplementation(WTF::adoptRef(*new WriteStoreImplementationOnFileStream()));
    writeStoreImplementation->finishCreation(byteCodeStorePath);
    return writeStoreImplementation;
}

void WriteStoreImplementationOnFileStream::finishCreation(const char* byteCodeStorePath) {
    stream.open(byteCodeStorePath, std::ios::binary | std::ios::out); // overwrite
    ASSERT(!stream.fail() && stream.is_open());    
}

WriteStoreImplementationOnFileStream::WriteStoreImplementationOnFileStream() {}

void WriteStoreImplementationOnFileStream::writeBytes(const char* buffer, size_t size) {
    ASSERT(stream.is_open());
	stream.write(buffer, size);
}

size_t WriteStoreImplementationOnFileStream::currentWritePosition() {
    ASSERT(stream.is_open());
    return static_cast<unsigned>(stream.tellp());
}

/*static */RefPtr<ByteCodeWriteStore> ByteCodeWriteStore::createForProgram(ProgramExecutable& program) {
    const SourceCode& programSource = program.source();
    std::string writeStorePath = ByteCodeStoreUtils::getByteCodeStorePathForSourceCode(programSource) ;

    RefPtr<ByteCodeWriteStore> writeStore = nullptr;
    Ref<WriteStoreImplementationOnFileStream> writeStoreImplmentation = WriteStoreImplementationOnFileStream::create(writeStorePath.c_str());
    writeStore = WTF::adoptRef(new ByteCodeWriteStore(writeStoreImplmentation.leakRef()));
    return writeStore;
}

ByteCodeWriteStore::ByteCodeWriteStore(WriteStoreImplementation& storeImplementation)
    : m_storeImplementation (WTF::adoptRef(storeImplementation))
{}

size_t ByteCodeWriteStore::currentWritePosition() {
    return m_storeImplementation->currentWritePosition();
}

void ByteCodeWriteStore::writeBytes(const char* buffer, size_t size) {
    m_storeImplementation->writeBytes(buffer, size);
}

} // end namespace JSC