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

#include "ByteCodeStoreUtils.h"

#include "ByteCodeReadStore.h"
#include <wtf/ReadStoreImplementationMMap.h>

#include <wtf/DataLog.h>

namespace JSC {

/*static */bool ByteCodeReadStore::validateStoreMagicBytes(ReadStoreImplementation&) {
    // Verify "MSOJSC"
    return true;
}

/*static */bool ByteCodeReadStore::trySeekEntryPoint(ReadStoreImplementation& readStoreImpl) {
    readStoreImpl.seekOffsetFromEnd(sizeof(size_t));
    size_t programOffset;
    readStoreImpl.readBytes(reinterpret_cast<char*>(&programOffset), sizeof(size_t));
    ASSERT(programOffset < readStoreImpl.getSize());

    readStoreImpl.seekOffset(programOffset);

    char prologueChar;
    readStoreImpl.readBytes(reinterpret_cast<char*>(&prologueChar), 1);
    if(prologueChar != 'P')
        return false;

    readStoreImpl.readBytes(reinterpret_cast<char*>(&prologueChar), 1);
    if(prologueChar != 'P')
        return false;

    readStoreImpl.readBytes(reinterpret_cast<char*>(&prologueChar), 1);
    if(prologueChar != 'P')
        return false;

    return true;
}

// Returns empty store if failed.
/*static*/ void ByteCodeReadStore::tryCreateForProgram(ProgramExecutable& program) {

    const SourceCode& programSource = program.source();
	std::string byteCodeStoreFilePath = ByteCodeStoreUtils::getByteCodeStorePathForSourceCode(programSource) ;
	if (byteCodeStoreFilePath.length() == 0) {
		dataLogLn("No source url !!");
		return;
	}

    Ref<ReadStoreImplementation> readStoreImpl = ReadStoreImplementationOnMemoryMappedFile::create(byteCodeStoreFilePath.c_str());

    if(!readStoreImpl->isAvailable()) {
        dataLogLn("Bytecode store not available.");
        return;
    }

    if(!validateStoreMagicBytes(readStoreImpl.get())) {
        dataLogLn("Failed to validate the store's magic header.");
        return;
    }

    if(!trySeekEntryPoint(readStoreImpl.get())) {
        dataLogLn("Failed to seek to entry point code block in the store.");
        return;
    }

    program.setByteCodeCache(WTF::adoptRef(*new ByteCodeReadStore(readStoreImpl.leakRef())));
}

ByteCodeReadStore::ByteCodeReadStore(ReadStoreImplementation& storeImplementation)
    : m_storeImplementation(WTF::adoptRef(storeImplementation))
{}

bool ByteCodeReadStore::prepareForFunction(VM&, FunctionExecutable* function, CodeSpecializationKind specializationKind) {
    
    if(function->unlinkedExecutable()->byteCodeBundleOffsetForCall() == 0) {
            return false;
    }

    switch(specializationKind){
		case CodeSpecializationKind::CodeForCall:
            m_storeImplementation->seekOffset(function->unlinkedExecutable()->byteCodeBundleOffsetForCall());
			break;
		//case CodeSpecializationKind::CodeForConstruct:
        //    m_storeImplementation->seekOffset(function->unlinkedExecutable()->byteCodeBundleOffsetForConstruct());
		//	break;
		default:
            ASSERT(0);
            return false;
	}

    // Ensure function prelogue
    char prologueChar;
	m_storeImplementation->readBytes(&prologueChar, 1);
	if(prologueChar != 'F') return false;
    
    m_storeImplementation->readBytes(&prologueChar, 1);
	if(prologueChar != 'F') return false;
    
    m_storeImplementation->readBytes(&prologueChar, 1);
	if(prologueChar != 'F') return false;

    return true;
}

void ByteCodeReadStore::readBytes(char* buffer, size_t size) {
    m_storeImplementation->readBytes(buffer, size);
}

void ByteCodeReadStore::readVector(char** buffer, size_t size) {
    m_storeImplementation->readVector(buffer, size);
}

void ByteCodeReadStore::seekOffset(size_t offset) {
    m_storeImplementation->seekOffset(offset);
}

} // end namespace JSC