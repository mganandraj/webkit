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

#include <sstream>

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

void FunctionExecutable::save(VM& vm, std::ofstream& ofs){
    
    ParserError error;
    UnlinkedFunctionCodeBlock* unlinkedCodeBlockForCall = nullptr, *unlinkedCodeBlockForConstruct = nullptr;

    const Identifier& id =  this->m_unlinkedExecutable->ecmaName();
    std::string idstr(reinterpret_cast<const char* >(id.string().characters8()), id.string().length());

    // Don't proceed if this is a builtin.
    if(isBuiltinFunction()) {
        dataLogLn("# Skipping builtin : ", idstr.c_str());
        return;
    }

    if(constructAbility() == ConstructAbility::CanConstruct) {

        unlinkedCodeBlockForConstruct = 
            this->m_unlinkedExecutable->unlinkedCodeBlockFor(
                vm, this->m_source, CodeSpecializationKind::CodeForConstruct , DebuggerMode::DebuggerOff, error, 
                    parseMode());

    }

    // These cannot be called as of my knowledge...
    // 1. Class constructors
    // 2. ???

    if(!isClassConstructorFunction()) {
        unlinkedCodeBlockForCall = 
            this->m_unlinkedExecutable->unlinkedCodeBlockFor(
                vm, this->m_source, CodeSpecializationKind::CodeForCall, DebuggerMode::DebuggerOff, error, 
                    parseMode());
    }
    
    ASSERT(unlinkedCodeBlockForCall || unlinkedCodeBlockForConstruct);
    
    recordParse(
        m_unlinkedExecutable->features(), 
        m_unlinkedExecutable->hasCapturedVariables(),
        lastLine(), endColumn()); 

    // UnlinkedFunctionCodeBlock* eitherCodeBlock = unlinkedCodeBlockForConstruct ? unlinkedCodeBlockForConstruct : unlinkedCodeBlockForCall;


    // TODO :: Need to find a way to not write both
    if(unlinkedCodeBlockForConstruct) {

        // Write functions.
        for(int i=0; i<unlinkedCodeBlockForConstruct->numberOfFunctionDecls(); i++) {
            UnlinkedFunctionExecutable* ufunc = unlinkedCodeBlockForConstruct->functionDecl(i);
            FunctionExecutable* func = ufunc->link(vm, this->source());
            dataLogLn("#Saving: ", this->firstLine(), ": ", this->startColumn());
            func->save(vm, ofs);	
        }

        // Write functions expressions.
        for(int i=0; i<unlinkedCodeBlockForConstruct->numberOfFunctionExprs(); i++) {
            UnlinkedFunctionExecutable* ufunc = unlinkedCodeBlockForConstruct->functionExpr(i);
            FunctionExecutable* func = ufunc->link(vm, this->source());
            dataLogLn("#Saving: ", this->firstLine(), ": ", this->startColumn());
            func->save(vm, ofs);
        }

    }

    if(unlinkedCodeBlockForCall) {
        
        // Write functions.
        for(int i=0; i<unlinkedCodeBlockForCall->numberOfFunctionDecls(); i++) {
            UnlinkedFunctionExecutable* ufunc = unlinkedCodeBlockForCall->functionDecl(i);
            FunctionExecutable* func = ufunc->link(vm, this->source());
            func->save(vm, ofs);	
        }

        // Write functions expressions.
        for(int i=0; i<unlinkedCodeBlockForCall->numberOfFunctionExprs(); i++) {
            UnlinkedFunctionExecutable* ufunc = unlinkedCodeBlockForCall->functionExpr(i);
            FunctionExecutable* func = ufunc->link(vm, this->source());
            func->save(vm, ofs);
        }
    }
    
    // All the descendants are written .. Now write self.

    if(unlinkedCodeBlockForConstruct != nullptr) {

        this->unlinkedExecutable()->setByteCodeBundleOffsetForConstruct(static_cast<unsigned>(ofs.tellp()));
        
        // Write recordParse
        ScriptExecutable::save(vm, ofs);

        // Write codeblock.
        unlinkedCodeBlockForConstruct->save(vm, ofs);
    } 
    
    if(unlinkedCodeBlockForCall != nullptr) {

        this->unlinkedExecutable()->setByteCodeBundleOffsetForCall(static_cast<unsigned>(ofs.tellp()));
        
        // Write recordParse
        ScriptExecutable::save(vm, ofs);
        
        // Write codeblock.
        unlinkedCodeBlockForCall->save(vm, ofs);
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
