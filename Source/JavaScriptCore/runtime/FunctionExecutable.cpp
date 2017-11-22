/*
 * Copyright (C) 2009, 2010, 2013, 2015-2016 Apple Inc. All rights reserved.
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

#include "BatchedTransitionOptimizer.h"
#include "CodeBlock.h"
#include "Debugger.h"
#include "FunctionCodeBlock.h"
#include "JIT.h"
#include "JSCInlines.h"
#include "LLIntEntrypoint.h"
#include "Parser.h"
#include "TypeProfiler.h"
#include "VMInlines.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo FunctionExecutable::s_info = { "FunctionExecutable", &ScriptExecutable::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionExecutable) };

FunctionExecutable::FunctionExecutable(VM& vm, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, unsigned lastLine, unsigned endColumn, Intrinsic intrinsic)
    : ScriptExecutable(vm.functionExecutableStructure.get(), vm, source, unlinkedExecutable->isInStrictContext(), unlinkedExecutable->derivedContextType(), false, EvalContextType::None, intrinsic)
    , m_unlinkedExecutable(vm, this, unlinkedExecutable)
{
    RELEASE_ASSERT(!source.isNull());
    ASSERT(source.length());
    m_lastLine = lastLine;
    ASSERT(endColumn != UINT_MAX);
    m_endColumn = endColumn;
    m_parametersStartOffset = unlinkedExecutable->parametersStartOffset();
    m_typeProfilingStartOffset = unlinkedExecutable->typeProfilingStartOffset();
    m_typeProfilingEndOffset = unlinkedExecutable->typeProfilingEndOffset();
}

void FunctionExecutable::save(VM& vm, const char* prefix, const char* suffix){
    // Get file name to write to.
    UnlinkedFunctionExecutable* ue = this->unlinkedExecutable();
    const Identifier& nameid = ue->inferredName();
    const char* fileName = nameid.ascii().data();
    
    std::string repoPath("c:\\tmp\\jsc\\script\\");
    std::string newprefix(prefix);
    newprefix.append("_");
    newprefix.append(fileName);
    newprefix.append("_");
    newprefix.append(suffix);

    // First generate unlinked byte codes.

    ParserError error;
        
    // First write for call.
    UnlinkedFunctionCodeBlock* unlinkedCodeBlockForCall;
    {

        std::string callPath(repoPath);
        callPath.append(newprefix);
        callPath.append("_call");
        callPath.append(".jsb");
    
        std::ofstream ofs(callPath, std::ios::binary);
        
        unlinkedCodeBlockForCall = 
            this->m_unlinkedExecutable->unlinkedCodeBlockFor(
                vm, this->m_source, CodeSpecializationKind::CodeForCall, DebuggerMode::DebuggerOff, error, 
                    parseMode());

        recordParse(
            m_unlinkedExecutable->features(), 
            m_unlinkedExecutable->hasCapturedVariables(),
            lastLine(), endColumn()); 

        // Write recordParse
        ScriptExecutable::save(vm, ofs);

        // Write codeblock.
        unlinkedCodeBlockForCall->save(vm, ofs);
    }    

    // Now write for construct if needed.
    if(constructAbility() == ConstructAbility::CanConstruct) {

        std::string ctrPath(repoPath);
        ctrPath.append(newprefix);
        ctrPath.append("_ctr");
        ctrPath.append(".jsb");
    
        std::ofstream ofs(ctrPath, std::ios::binary);

        UnlinkedFunctionCodeBlock* unlinkedCodeBlock = 
            this->m_unlinkedExecutable->unlinkedCodeBlockFor(
                vm, this->m_source, CodeSpecializationKind::CodeForConstruct , DebuggerMode::DebuggerOff, error, 
                    parseMode());

        recordParse(
            m_unlinkedExecutable->features(), 
            m_unlinkedExecutable->hasCapturedVariables(),
            lastLine(), endColumn()); 

        // Write recordParse
        ScriptExecutable::save(vm, ofs);

        // Write codeblock.
        unlinkedCodeBlock->save(vm, ofs);
    }

    // End of writing codeblocks.

    // Write functions.
    for(int i=0; i<unlinkedCodeBlockForCall->numberOfFunctionDecls(); i++) {
        UnlinkedFunctionExecutable* ufunc = unlinkedCodeBlockForCall->functionDecl(i);
        FunctionExecutable* func = ufunc->link(vm, this->source());
        std::string suffix("d_");
        
        //suffix.append(std::to_string(i));
        std::stringstream ss;
        ss << i;
        suffix.append(ss.str());

        func->save(vm, newprefix.c_str(), suffix.c_str());
    }

    // Write functions.
    for(int i=0; i<unlinkedCodeBlockForCall->numberOfFunctionExprs(); i++) {
        UnlinkedFunctionExecutable* ufunc = unlinkedCodeBlockForCall->functionExpr(i);
        FunctionExecutable* func = ufunc->link(vm, this->source());
        std::string suffix("x_");
        
        //suffix.append(std::to_string(i));
        std::stringstream ss;
        ss << i;
        suffix.append(ss.str());

        func->save(vm, newprefix.c_str(), suffix.c_str());
    }
}

UnlinkedProgramCodeBlock* FunctionExecutable::load(VM& vm, const char* prefix){
    return nullptr; 
}


void FunctionExecutable::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    m_singletonFunction.set(vm, this, InferredValue::create(vm));
}

void FunctionExecutable::destroy(JSCell* cell)
{
    static_cast<FunctionExecutable*>(cell)->FunctionExecutable::~FunctionExecutable();
}

FunctionCodeBlock* FunctionExecutable::baselineCodeBlockFor(CodeSpecializationKind kind)
{
    FunctionCodeBlock* result;
    if (kind == CodeForCall)
        result = m_codeBlockForCall.get();
    else {
        RELEASE_ASSERT(kind == CodeForConstruct);
        result = m_codeBlockForConstruct.get();
    }
    if (!result)
        return 0;
    return static_cast<FunctionCodeBlock*>(result->baselineAlternative());
}

void FunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    FunctionExecutable* thisObject = jsCast<FunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    ScriptExecutable::visitChildren(thisObject, visitor);
    if (FunctionCodeBlock* codeBlockForCall = thisObject->m_codeBlockForCall.get())
        codeBlockForCall->visitWeakly(visitor);
    if (FunctionCodeBlock* codeBlockForConstruct = thisObject->m_codeBlockForConstruct.get())
        codeBlockForConstruct->visitWeakly(visitor);
    visitor.append(thisObject->m_unlinkedExecutable);
    visitor.append(thisObject->m_singletonFunction);
    visitor.append(thisObject->m_cachedPolyProtoStructure);
}

FunctionExecutable* FunctionExecutable::fromGlobalCode(
    const Identifier& name, ExecState& exec, const SourceCode& source, 
    JSObject*& exception, int overrideLineNumber)
{
    UnlinkedFunctionExecutable* unlinkedExecutable = 
        UnlinkedFunctionExecutable::fromGlobalCode(
            name, exec, source, exception, overrideLineNumber);
    if (!unlinkedExecutable)
        return nullptr;

    return unlinkedExecutable->link(exec.vm(), source, overrideLineNumber);
}

} // namespace JSC
