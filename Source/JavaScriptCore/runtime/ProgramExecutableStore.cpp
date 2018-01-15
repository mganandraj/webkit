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

#include "ProgramExecutableStore.h"
#include "FunctionExecutableStore.h"

#include "ByteCodeReadStore.h"
#include "ByteCodeWriteStore.h"

#include "ByteCodeStoreMacros.h"

namespace JSC {

Ref<ProgramExecutableStore> ProgramExecutableStore::create(ProgramExecutable& programExecutable) {
    return WTF::adoptRef(*new ProgramExecutableStore(programExecutable));
}

UnlinkedProgramCodeBlock* ProgramExecutableStore::load(VM& vm, ByteCodeReadStore& byteCodeCache) {
    //ByteCodeReadStore& byteCodeCache = m_programExecutable.getByteCodeCache();

    ScriptExecutableStore::load(vm, byteCodeCache);
    
    UnlinkedProgramCodeBlock* unlinkedCodeBlock = UnlinkedProgramCodeBlock::create(&vm, m_programExecutable.executableInfo(), DebuggerMode::DebuggerOff);
    unlinkedCodeBlock->recordParse(m_programExecutable.features(), false, m_programExecutable.lastLine(), m_programExecutable.endColumn());

    Ref<UnlinkedProgramCodeBlockStore> programCodeBlockStore = UnlinkedProgramCodeBlockStore::create(*unlinkedCodeBlock);
    programCodeBlockStore->load(vm, byteCodeCache);

    return unlinkedCodeBlock;

}

size_t ProgramExecutableStore::save(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    // Start with a magic .. It serves some purpose !!
    const char* magic = "MSOJSC";
    WRITEVECTOR8(magic, 6);
 
    // Write functions.
    for(size_t i=0; i<m_programExecutable.m_unlinkedProgramCodeBlock.get()->numberOfFunctionDecls(); i++) {
        UnlinkedFunctionExecutable* ufunc = m_programExecutable.m_unlinkedProgramCodeBlock.get()->functionDecl(i);
        FunctionExecutable* func = ufunc->link(vm, m_programExecutable.source());
        
        //func->save2(vm, byteCodeCache);
        Ref<FunctionExecutableStore> functionExecutableStore = FunctionExecutableStore::create(*func);
        functionExecutableStore->save(vm, byteCodeCache);
    }

    // Write function expressions.
    for(size_t i=0; i<m_programExecutable.m_unlinkedProgramCodeBlock.get()->numberOfFunctionExprs(); i++) {
        UnlinkedFunctionExecutable* ufunc = m_programExecutable.m_unlinkedProgramCodeBlock.get()->functionExpr(i);
        FunctionExecutable* func = ufunc->link(vm, m_programExecutable.source());
        
        //func->save2(vm, byteCodeCache);
        Ref<FunctionExecutableStore> functionExecutableStore = FunctionExecutableStore::create(*func);
        functionExecutableStore->save(vm, byteCodeCache);
    }

	size_t programIndex = byteCodeCache.currentWritePosition();
     
    const char* programPrelogue = "PPP";
    WRITEVECTOR8(programPrelogue, 3);

    // Write recordParse
    ScriptExecutableStore::save(vm, byteCodeCache);
    
    // Write codeblock.
    Ref<UnlinkedProgramCodeBlockStore> programCodeBlockStore = UnlinkedProgramCodeBlockStore::create(*m_programExecutable.m_unlinkedProgramCodeBlock.get());
    programCodeBlockStore->save(vm, byteCodeCache);

    return programIndex;

}

}