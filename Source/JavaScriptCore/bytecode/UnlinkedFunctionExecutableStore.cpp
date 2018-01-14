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

#include "UnlinkedFunctionExecutableStore.h"

namespace JSC {

Ref<UnlinkedFunctionExecutableStore> UnlinkedFunctionExecutableStore::create(UnlinkedFunctionExecutable& unlinkedFunctionExecutable) {
    return WTF::adoptRef(*new UnlinkedFunctionExecutableStore(unlinkedFunctionExecutable));
}

void UnlinkedFunctionExecutableStore::saveHeader(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_UNLINKEDFUNCTIONEXECUTABLE);

    WRITEATOMICIDENTIFIER(m_unlinkedFunctionExecutable.m_name);

    WRITEATOMICIDENTIFIER(m_unlinkedFunctionExecutable.m_ecmaName);

    WRITEATOMICIDENTIFIER(m_unlinkedFunctionExecutable.m_inferredName);

    if(!m_unlinkedFunctionExecutable.m_parentSourceOverride.isNull()) {
        StringView source = m_unlinkedFunctionExecutable.m_parentSourceOverride.provider()->source();
        
        unsigned length = source.length();
        WRITEFIELD(length);

        bool is8Bit = source.is8Bit();
        WRITEFIELD(is8Bit);

        if(source.is8Bit()) {
            WRITEVECTOR8(source.characters8(), length);
        } else {
            WRITEVECTOR8(source.characters16(), length);
        }
    } else {
        unsigned sourceLength = 0;
        WRITEFIELD(sourceLength);
    }

    //ofstream<<std::string(reinterpret_cast<const char* >(m_sourceURLDirective.characters8()), m_sourceURLDirective.length());
    //ofstream << " ";
    //ofstream<<std::string(reinterpret_cast<const char* >(m_sourceMappingURLDirective.characters8()), m_sourceMappingURLDirective.length());
    //ofstream << " ";

    WRITEFIELD(m_unlinkedFunctionExecutable.m_byteCodeBundleOffsetForCall);
    //WRITEFIELD(m_byteCodeBundleOffsetForConstruct);
    
    WRITEFIELD(m_unlinkedFunctionExecutable.m_firstLineOffset);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_lineCount);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_unlinkedFunctionNameStart);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_unlinkedBodyStartColumn);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_unlinkedBodyEndColumn);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_startOffset);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_sourceLength);
   
    WRITEFIELD(m_unlinkedFunctionExecutable.m_parametersStartOffset);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_typeProfilingStartOffset);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_typeProfilingEndOffset);
    WRITEFIELD(m_unlinkedFunctionExecutable.m_parameterCount);
    
    uint16_t codeFeatures = static_cast<uint16_t>(m_unlinkedFunctionExecutable.m_features);
    WRITEFIELD(codeFeatures);
    
    uint32_t parseMode = static_cast<uint32_t>(m_unlinkedFunctionExecutable.m_sourceParseMode);
    WRITEFIELD(parseMode);
    
    uint16_t funcMetadata = 0;
    funcMetadata |= m_unlinkedFunctionExecutable.m_isInStrictContext;
    funcMetadata |= m_unlinkedFunctionExecutable.m_hasCapturedVariables << 1;
    funcMetadata |= m_unlinkedFunctionExecutable.m_isBuiltinFunction << 2;
    funcMetadata |= m_unlinkedFunctionExecutable.m_constructAbility << 3;
    funcMetadata |= m_unlinkedFunctionExecutable.m_constructorKind << 4;
    funcMetadata |= m_unlinkedFunctionExecutable.m_functionMode << 6;
    funcMetadata |= m_unlinkedFunctionExecutable.m_scriptMode << 8;
    funcMetadata |= m_unlinkedFunctionExecutable.m_superBinding << 9;
    funcMetadata |= m_unlinkedFunctionExecutable.m_derivedContextType << 10;

    WRITEFIELD(funcMetadata);

    // TODO Parent Scope TDZ
    // m_parentScopeTDZVariables->;

}

void UnlinkedFunctionExecutableStore::loadHeader(VM& vm, ByteCodeReadStore& byteCodeCache) {
    VERIFYMAGIC(MAGIC_UNLINKEDFUNCTIONEXECUTABLE);
    
    READATOMICIDENTIFIER(m_unlinkedFunctionExecutable.m_name);

    READATOMICIDENTIFIER(m_unlinkedFunctionExecutable.m_ecmaName);

    READATOMICIDENTIFIER(m_unlinkedFunctionExecutable.m_inferredName);
    
    {
        unsigned sourceLength;
        READFIELD(sourceLength);

        if(sourceLength > 0) {

            bool isSource8bit;
            READFIELD(isSource8bit);

            if(isSource8bit) {
                char* _data;
                READVECTOR8(&_data, sourceLength);
                m_unlinkedFunctionExecutable.m_parentSourceOverride = makeSource(StringImpl::create(reinterpret_cast<const LChar*>(_data), sourceLength), { });
            } else {
                char* _data;
                READVECTOR16(&_data, sourceLength);
                m_unlinkedFunctionExecutable.m_parentSourceOverride = makeSource(StringImpl::create(reinterpret_cast<const UChar*>(_data), sourceLength), { });
            }
        }
    }

    READFIELD(m_unlinkedFunctionExecutable.m_byteCodeBundleOffsetForCall);
    //READFIELD(m_byteCodeBundleOffsetForConstruct);

    READFIELD(m_unlinkedFunctionExecutable.m_firstLineOffset);
    READFIELD(m_unlinkedFunctionExecutable.m_lineCount);
    READFIELD(m_unlinkedFunctionExecutable.m_unlinkedFunctionNameStart);
    READFIELD(m_unlinkedFunctionExecutable.m_unlinkedBodyStartColumn);
    READFIELD(m_unlinkedFunctionExecutable.m_unlinkedBodyEndColumn);
    READFIELD(m_unlinkedFunctionExecutable.m_startOffset);
    READFIELD(m_unlinkedFunctionExecutable.m_sourceLength);
    
    READFIELD(m_unlinkedFunctionExecutable.m_parametersStartOffset);
    READFIELD(m_unlinkedFunctionExecutable.m_typeProfilingStartOffset);
    READFIELD(m_unlinkedFunctionExecutable.m_typeProfilingEndOffset);
    READFIELD(m_unlinkedFunctionExecutable.m_parameterCount);
    
    uint16_t codeFeatures;
    READFIELD(codeFeatures);
    m_unlinkedFunctionExecutable.m_features = static_cast<CodeFeatures>(codeFeatures);
    
    uint32_t parseMode;
    READFIELD(parseMode);
    m_unlinkedFunctionExecutable.m_sourceParseMode = static_cast<SourceParseMode>(parseMode);

    uint16_t funcMetadata;
    READFIELD(funcMetadata);

    m_unlinkedFunctionExecutable.m_isInStrictContext = funcMetadata & 0x0001;
    m_unlinkedFunctionExecutable.m_hasCapturedVariables = (funcMetadata & 0x0002) >> 1;
    m_unlinkedFunctionExecutable.m_isBuiltinFunction = (funcMetadata & 0x0004) >> 2;
    m_unlinkedFunctionExecutable.m_constructAbility = (funcMetadata & 0x0008) >> 3;
    m_unlinkedFunctionExecutable.m_constructorKind = (funcMetadata & 0x0030) >> 4;
    m_unlinkedFunctionExecutable.m_functionMode = (funcMetadata & 0x00C0) >> 6;
    m_unlinkedFunctionExecutable.m_scriptMode = (funcMetadata & 0x0100) >> 8;
    m_unlinkedFunctionExecutable.m_superBinding = (funcMetadata & 0x0200) >> 9;
    m_unlinkedFunctionExecutable.m_derivedContextType = (funcMetadata & 0x0400) >> 10;
}

UnlinkedFunctionCodeBlock* UnlinkedFunctionExecutableStore::loadCodeblock(VM& vm, SourceParseMode parseMode, ByteCodeReadStore& byteCodeCache) {
    CodeFeatures features;
    bool hasCapturedVariables;
    int lastLine;
    unsigned endColumn;

    VERIFYMAGIC(MAGIC_SCRIPT);
    
    READFIELD(features);
    READFIELD(hasCapturedVariables);
    READFIELD(lastLine);
    READFIELD(endColumn);

    m_unlinkedFunctionExecutable.recordParse(features, hasCapturedVariables);

    bool usesEval = features & EvalFeature;
    bool isStrictMode = features & StrictModeFeature;

    //JSParserBuiltinMode builtinMode = isBuiltinFunction() ? JSParserBuiltinMode::Builtin : JSParserBuiltinMode::NotBuiltin;
    // JSParserStrictMode strictMode = isInStrictContext() ? JSParserStrictMode::Strict : JSParserStrictMode::NotStrict;
    JSParserScriptMode _scriptMode = m_unlinkedFunctionExecutable.scriptMode();

    bool isClassContext = m_unlinkedFunctionExecutable.superBinding() == SuperBinding::Needed;

    UnlinkedFunctionCodeBlock* unlinkedCodeblock = UnlinkedFunctionCodeBlock::create(&vm, FunctionCode, 
        ExecutableInfo(usesEval, isStrictMode, 
        /*specializationKind == CodeForConstruct*/false, false /* !UnlinkedBuiltinFunction*/, 
        m_unlinkedFunctionExecutable.constructorKind(), _scriptMode, m_unlinkedFunctionExecutable.superBinding(),
        parseMode, m_unlinkedFunctionExecutable.derivedContextType(), false, isClassContext, 
        EvalContextType::None), 
        DebuggerOff);

    Ref<UnlinkedFunctionCodeBlockStore> functionCodeBlockStore = UnlinkedFunctionCodeBlockStore::create(*unlinkedCodeblock);
    functionCodeBlockStore->load(vm, byteCodeCache);

    return unlinkedCodeblock;
}

}