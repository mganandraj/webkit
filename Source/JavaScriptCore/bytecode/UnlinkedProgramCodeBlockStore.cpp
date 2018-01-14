/*
 * Copyright (C) 2012-2017 Apple Inc. All Rights Reserved.
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

#include "UnlinkedProgramCodeBlockStore.h"
#include "UnlinkedCodeBlockStore.h"

namespace JSC {

Ref<UnlinkedProgramCodeBlockStore> UnlinkedProgramCodeBlockStore::create(UnlinkedProgramCodeBlock& UnlinkedProgramCodeBlock) {
    return WTF::adoptRef(*new UnlinkedProgramCodeBlockStore(UnlinkedProgramCodeBlock));
}

void UnlinkedProgramCodeBlockStore::load(VM& vm, ByteCodeReadStore& byteCodeCache) {
    UnlinkedCodeBlockStore::load(byteCodeCache);

    m_unlinkedProgramCodeBlock.m_varDeclarations.load(vm, m_unlinkedProgramCodeBlock.identifiers(), byteCodeCache);
    m_unlinkedProgramCodeBlock.m_lexicalDeclarations.load(vm, m_unlinkedProgramCodeBlock.identifiers(), byteCodeCache);
}

void UnlinkedProgramCodeBlockStore::save(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    UnlinkedCodeBlockStore::save(byteCodeCache);

    m_unlinkedProgramCodeBlock.m_varDeclarations.save(vm, m_unlinkedProgramCodeBlock.identifiers(), byteCodeCache);
    m_unlinkedProgramCodeBlock.m_lexicalDeclarations.save(vm, m_unlinkedProgramCodeBlock.identifiers(), byteCodeCache);
}    

}