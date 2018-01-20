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

#include "FunctionExecutableStore.h"
#include "UnlinkedFunctionCodeBlockStore.h"

#include "ByteCodeReadStore.h"
#include "ByteCodeWriteStore.h"
#include "ByteCodeStoreMacros.h"

namespace JSC {

Ref<FunctionExecutableStore> FunctionExecutableStore::create(FunctionExecutable& functionExecutable) {
    return WTF::adoptRef(*new FunctionExecutableStore(functionExecutable));
}

void FunctionExecutableStore::load(VM&, ByteCodeReadStore&) {
    ASSERT(0);
}

void FunctionExecutableStore::save(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    
    ParserError error;
    UnlinkedFunctionCodeBlock* unlinkedCodeBlockForCall = nullptr;

    //dataLogLn("Saving function : ", this->unlinkedExecutable()->name().string(), " : ",
    //  this->unlinkedExecutable()->ecmaName().string(),  " : ",
    //    this->unlinkedExecutable()->inferredName().string(), " : ",
    //    this->source().firstLine().oneBasedInt(),  " : ",
    //    this->source().startColumn().oneBasedInt()
    //);

    unlinkedCodeBlockForCall = m_functionExecutable.m_unlinkedExecutable->m_unlinkedCodeBlockForCall.get();
    if(unlinkedCodeBlockForCall == nullptr && JSC::Options::enableBytecodeGenerationWhileCaching()) { // generate for calls if not avaiable and configured to generate ..
        // Class constructor can't be called ...
        if(!m_functionExecutable.isClassConstructorFunction()) {
            //dataLogLn("Generating .. ");
            unlinkedCodeBlockForCall = 
                m_functionExecutable.m_unlinkedExecutable->unlinkedCodeBlockFor(
                    vm, m_functionExecutable.m_source, CodeSpecializationKind::CodeForCall, DebuggerMode::DebuggerOff, error, 
                        m_functionExecutable.parseMode());
        } 
        //else {
        //    dataLogLn("Skipping class constructor .. ");
        //}
    }

    if(!unlinkedCodeBlockForCall) {
        // dataLogLn("Skip writing as no codeblock for call.. ");
        return;
    }

    // Write functions.
    for(size_t i=0; i<unlinkedCodeBlockForCall->numberOfFunctionDecls(); i++) {
        UnlinkedFunctionExecutable* ufunc = unlinkedCodeBlockForCall->functionDecl(i);
        FunctionExecutable* func = ufunc->link(vm, m_functionExecutable.source());
        
        Ref<FunctionExecutableStore> functionExecutableStore = FunctionExecutableStore::create(*func);
        functionExecutableStore->save(vm, byteCodeCache);
    }

    // Write functions expressions.
    for(size_t i=0; i<unlinkedCodeBlockForCall->numberOfFunctionExprs(); i++) {
        UnlinkedFunctionExecutable* ufunc = unlinkedCodeBlockForCall->functionExpr(i);
        FunctionExecutable* func = ufunc->link(vm, m_functionExecutable.source());
        
        Ref<FunctionExecutableStore> functionExecutableStore = FunctionExecutableStore::create(*func);
        functionExecutableStore->save(vm, byteCodeCache);
    }
    
    const char* functionPrelogue = "FFF";

    // All the descendants are written .. Now write self.
    m_functionExecutable.unlinkedExecutable()->setByteCodeBundleOffsetForCall(byteCodeCache.currentWritePosition());

    WRITEVECTOR8(functionPrelogue, 3);
    
    // Write recordParse
    ScriptExecutableStore::save(vm, byteCodeCache);
    
    // Write codeblock.
    //unlinkedCodeBlockForCall->save(vm, byteCodeCache);
    Ref<UnlinkedFunctionCodeBlockStore> functionCodeBlockStore = UnlinkedFunctionCodeBlockStore::create(*unlinkedCodeBlockForCall);
    functionCodeBlockStore->save(vm, byteCodeCache);
}

}
