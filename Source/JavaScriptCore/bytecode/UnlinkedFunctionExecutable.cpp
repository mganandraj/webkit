/*
 * Copyright (C) 2012-2013, 2015-2016 Apple Inc. All Rights Reserved.
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
#include "UnlinkedFunctionExecutable.h"

#include "BytecodeGenerator.h"
#include "ClassInfo.h"
#include "CodeCache.h"
#include "Debugger.h"
#include "ExecutableInfo.h"
#include "FunctionOverrides.h"
#include "JSCInlines.h"
#include "Parser.h"
#include "SourceProvider.h"
#include "Structure.h"
#include "UnlinkedFunctionCodeBlock.h"

#include "ByteCodeProvider.h"

namespace JSC {

static_assert(sizeof(UnlinkedFunctionExecutable) <= 256, "UnlinkedFunctionExecutable should fit in a 256-byte cell.");

const ClassInfo UnlinkedFunctionExecutable::s_info = { "UnlinkedFunctionExecutable", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(UnlinkedFunctionExecutable) };

static UnlinkedFunctionCodeBlock* generateUnlinkedFunctionCodeBlock(
    VM& vm, UnlinkedFunctionExecutable* executable, const SourceCode& source,
    CodeSpecializationKind kind, DebuggerMode debuggerMode,
    UnlinkedFunctionKind functionKind, ParserError& error, SourceParseMode parseMode)
{
    JSParserBuiltinMode builtinMode = executable->isBuiltinFunction() ? JSParserBuiltinMode::Builtin : JSParserBuiltinMode::NotBuiltin;
    JSParserStrictMode strictMode = executable->isInStrictContext() ? JSParserStrictMode::Strict : JSParserStrictMode::NotStrict;
    JSParserScriptMode scriptMode = executable->scriptMode();
    ASSERT(isFunctionParseMode(executable->parseMode()));
    std::unique_ptr<FunctionNode> function = parse<FunctionNode>(
        &vm, source, executable->name(), builtinMode, strictMode, scriptMode, executable->parseMode(), executable->superBinding(), error, nullptr);

    if (!function) {
        ASSERT(error.isValid());
        return nullptr;
    }

    function->finishParsing(executable->name(), executable->functionMode());
    executable->recordParse(function->features(), function->hasCapturedVariables());

    bool isClassContext = executable->superBinding() == SuperBinding::Needed;

    UnlinkedFunctionCodeBlock* result = UnlinkedFunctionCodeBlock::create(&vm, FunctionCode, ExecutableInfo(function->usesEval(), function->isStrictMode(), kind == CodeForConstruct, functionKind == UnlinkedBuiltinFunction, executable->constructorKind(), scriptMode, executable->superBinding(), parseMode, executable->derivedContextType(), false, isClassContext, EvalContextType::FunctionEvalContext), debuggerMode);

    error = BytecodeGenerator::generate(vm, function.get(), result, debuggerMode, executable->parentScopeTDZVariables());

    if (error.isValid())
        return nullptr;
    return result;
}

UnlinkedFunctionExecutable::UnlinkedFunctionExecutable(VM* vm, Structure* structure, std::ifstream& stream)
    : Base(*vm, structure)
    , m_isLoadingByteCodes(true)
{
    this->loadNonCode(*vm, stream);

    // Make sure these bitfields are adequately wide.
    //ASSERT(m_constructAbility == static_cast<unsigned>(constructAbility));
    //ASSERT(m_constructorKind == static_cast<unsigned>(node->constructorKind()));
    //ASSERT(m_functionMode == static_cast<unsigned>(node->functionMode()));
    //ASSERT(m_scriptMode == static_cast<unsigned>(scriptMode));
    //ASSERT(m_superBinding == static_cast<unsigned>(node->superBinding()));
    //ASSERT(m_derivedContextType == static_cast<unsigned>(derivedContextType));

    // TODO
    //m_parentScopeTDZVariables.swap(parentScopeTDZVariables);    
}

UnlinkedFunctionExecutable::UnlinkedFunctionExecutable(VM* vm, Structure* structure, const SourceCode& parentSource, SourceCode&& parentSourceOverride, FunctionMetadataNode* node, UnlinkedFunctionKind kind, ConstructAbility constructAbility, JSParserScriptMode scriptMode, VariableEnvironment& parentScopeTDZVariables, DerivedContextType derivedContextType)
    : Base(*vm, structure)
    , m_firstLineOffset(node->firstLine() - parentSource.firstLine().oneBasedInt())
    , m_lineCount(node->lastLine() - node->firstLine())
    , m_unlinkedFunctionNameStart(node->functionNameStart() - parentSource.startOffset())
    , m_unlinkedBodyStartColumn(node->startColumn())
    , m_unlinkedBodyEndColumn(m_lineCount ? node->endColumn() : node->endColumn() - node->startColumn())
    , m_startOffset(node->source().startOffset() - parentSource.startOffset())
    , m_sourceLength(node->source().length())
    , m_parametersStartOffset(node->parametersStart())
    , m_typeProfilingStartOffset(node->functionKeywordStart())
    , m_typeProfilingEndOffset(node->startStartOffset() + node->source().length() - 1)
    , m_parameterCount(node->parameterCount())
    , m_features(0)
    , m_sourceParseMode(node->parseMode())
    , m_isInStrictContext(node->isInStrictContext())
    , m_hasCapturedVariables(false)
    , m_isBuiltinFunction(kind == UnlinkedBuiltinFunction)
    , m_constructAbility(static_cast<unsigned>(constructAbility))
    , m_constructorKind(static_cast<unsigned>(node->constructorKind()))
    , m_functionMode(static_cast<unsigned>(node->functionMode()))
    , m_scriptMode(static_cast<unsigned>(scriptMode))
    , m_superBinding(static_cast<unsigned>(node->superBinding()))
    , m_derivedContextType(static_cast<unsigned>(derivedContextType))
    , m_name(node->ident())
    , m_ecmaName(node->ecmaName())
    , m_inferredName(node->inferredName())
    , m_parentSourceOverride(WTFMove(parentSourceOverride))
    , m_classSource(node->classSource())
    , m_byteCodeBundleOffsetForCall(0)
    , m_byteCodeBundleOffsetForConstruct(0)
    , m_isLoadingByteCodes(false)
{
    // Make sure these bitfields are adequately wide.
    ASSERT(m_constructAbility == static_cast<unsigned>(constructAbility));
    ASSERT(m_constructorKind == static_cast<unsigned>(node->constructorKind()));
    ASSERT(m_functionMode == static_cast<unsigned>(node->functionMode()));
    ASSERT(m_scriptMode == static_cast<unsigned>(scriptMode));
    ASSERT(m_superBinding == static_cast<unsigned>(node->superBinding()));
    ASSERT(m_derivedContextType == static_cast<unsigned>(derivedContextType));

    m_parentScopeTDZVariables.swap(parentScopeTDZVariables);
}

void UnlinkedFunctionExecutable::saveNonCode(VM&vm, std::ofstream& ofstream){
    
    ofstream << "UF ";

    std::string name(reinterpret_cast<const char* >(m_name.string().characters8()), m_name.string().length());
    if(name.empty())
        name.assign("<>");

    ofstream<<name;
    ofstream << " ";

    std::string ecmaName(reinterpret_cast<const char* >(m_ecmaName.string().characters8()), m_ecmaName.string().length());
    if(ecmaName.empty())
        ecmaName.assign("<>");

    ofstream<<ecmaName;
    ofstream << " ";

    std::string inferredName(reinterpret_cast<const char* >(m_inferredName.string().characters8()), m_inferredName.string().length());
    if(inferredName.empty())
        inferredName.assign("<>");
    
    ofstream << inferredName;
    ofstream << " ";

    //ofstream<<std::string(reinterpret_cast<const char* >(m_parentSourceOverride.string().characters8()), m_parentSourceOverride.string().length());
    //ofstream << " ";
    //ofstream<<std::string(reinterpret_cast<const char* >(m_classSource.string().characters8()), m_classSource.string().length());
    //ofstream << " ";

    //ofstream<<std::string(reinterpret_cast<const char* >(m_sourceURLDirective.characters8()), m_sourceURLDirective.length());
    //ofstream << " ";
    //ofstream<<std::string(reinterpret_cast<const char* >(m_sourceMappingURLDirective.characters8()), m_sourceMappingURLDirective.length());
    //ofstream << " ";

    ofstream << m_byteCodeBundleOffsetForCall << " " << m_byteCodeBundleOffsetForConstruct << " ";

    ofstream << m_firstLineOffset << " ";
    ofstream << m_lineCount << " ";
    ofstream << m_unlinkedFunctionNameStart << " ";
    ofstream << m_unlinkedBodyStartColumn << " ";
    ofstream << m_unlinkedBodyEndColumn << " ";
    ofstream << m_startOffset << " ";
    ofstream << m_sourceLength << " ";
    
    ofstream << m_parametersStartOffset << " ";
    ofstream << m_typeProfilingStartOffset << " ";
    ofstream << m_typeProfilingEndOffset << " ";
    ofstream << m_parameterCount << " ";
    
    ofstream <<  static_cast<uint16_t>(m_features) << " ";
    
    ofstream <<  static_cast<uint32_t>(m_sourceParseMode) << " ";
    
    uint16_t funcMetadata = 0;
    funcMetadata |= m_isInStrictContext;
    funcMetadata |= m_hasCapturedVariables << 1;
    funcMetadata |= m_isBuiltinFunction << 2;
    funcMetadata |= m_constructAbility << 3;
    funcMetadata |= m_constructorKind << 4;
    funcMetadata |= m_functionMode << 6;
    funcMetadata |= m_scriptMode << 8;
    funcMetadata |= m_superBinding << 9;
    funcMetadata |= m_derivedContextType << 10;

    ofstream <<  funcMetadata << " ";

    // TODO Parent Scope TDZ
    // m_parentScopeTDZVariables->;
}

void UnlinkedFunctionExecutable::loadNonCode(VM&vm, std::ifstream& ifstream){
    std::string index; 
    ifstream >> index;
    ASSERT(index[0] == 'U');

    // TODO :: Handle ES6 symbols
    std::string token;
    ifstream >> token;
    m_name = Identifier::fromString(&vm, reinterpret_cast<const LChar *>(token.c_str()), static_cast<int>(token.size()));
    ifstream >> token;
    m_ecmaName = Identifier::fromString(&vm, reinterpret_cast<const LChar *>(token.c_str()), static_cast<int>(token.size()));
    ifstream >> token;
    m_inferredName = Identifier::fromString(&vm, reinterpret_cast<const LChar *>(token.c_str()), static_cast<int>(token.size()));
    //ifstream >> token;
    //m_parentSourceOverride = Identifier::fromString(&vm, reinterpret_cast<const LChar *>(token.c_str()), static_cast<int>(token.size()));
    //ifstream >> token;
    //m_classSource = Identifier::fromString(&vm, reinterpret_cast<const LChar *>(token.c_str()), static_cast<int>(token.size()));

    //ifstream >> token;
    //m_sourceURLDirective = String(reinterpret_cast<const LChar *>(token.c_str()), static_cast<int>(token.size()));
    //ifstream >> token;
    //m_sourceMappingURLDirective = String(reinterpret_cast<const LChar *>(token.c_str()), static_cast<int>(token.size()));

    ifstream >> m_byteCodeBundleOffsetForCall;
    ifstream >> m_byteCodeBundleOffsetForConstruct;

    ifstream >> m_firstLineOffset;
    ifstream >> m_lineCount;
    ifstream >> m_unlinkedFunctionNameStart;
    ifstream >> m_unlinkedBodyStartColumn;
    ifstream >> m_unlinkedBodyEndColumn;
    ifstream >> m_startOffset;
    ifstream >> m_sourceLength;
  //  ifstream >> m_sourceOffset;

    ifstream >> m_parametersStartOffset;
    ifstream >> m_typeProfilingStartOffset;
    ifstream >> m_typeProfilingEndOffset;
    ifstream >> m_parameterCount;
    
    ifstream >>  m_features;
    
    uint32_t parseMode;
    ifstream >>  parseMode;
    m_sourceParseMode = static_cast<SourceParseMode>(parseMode);

    uint16_t funcMetadata = 0;
    ifstream >> funcMetadata;

    m_isInStrictContext = funcMetadata & 0x0001;
    m_hasCapturedVariables = (funcMetadata & 0x0002) >> 1;
    m_isBuiltinFunction = (funcMetadata & 0x0004) >> 2;
    m_constructAbility = (funcMetadata & 0x0008) >> 3;
    m_constructorKind = (funcMetadata & 0x0030) >> 4;
    m_functionMode = (funcMetadata & 0x00C0) >> 6;
    m_scriptMode = (funcMetadata & 0x0100) >> 8;
    m_superBinding = (funcMetadata & 0x0200) >> 9;
    m_derivedContextType = (funcMetadata & 0x0400) >> 10;
}

void UnlinkedFunctionExecutable::saveCode(VM& vm, std::ofstream&){
    // We are saving through FunctionExecutable.
}

UnlinkedFunctionCodeBlock* UnlinkedFunctionExecutable::loadCode(VM& vm, CodeSpecializationKind specializationKind,
    DebuggerMode debuggerMode, bool isBuiltin, ParserError& error, SourceParseMode parseMode)
{
    std::ifstream& ifs(vm.byteCodeProvider().getReadStream(""));    

    CodeFeatures features;
    bool hasCapturedVariables;
    int lastLine;
    unsigned endColumn;

    switch(specializationKind){
        case CodeSpecializationKind::CodeForCall:
            ifs.seekg(m_byteCodeBundleOffsetForCall);
            break;

        case CodeSpecializationKind::CodeForConstruct:
            ifs.seekg(m_byteCodeBundleOffsetForConstruct);
            break;
        default:
            ASSERT(0);
    }
    
	 int index; ifs >> index;
	 ASSERT(index == 1);

    ifs >> features;
    ifs >> hasCapturedVariables;
    ifs >> lastLine;
    ifs >> endColumn;

    this->recordParse(features, hasCapturedVariables);

    bool usesEval = features & EvalFeature;
    bool isStrictMode = features & StrictModeFeature;

    JSParserBuiltinMode builtinMode = isBuiltinFunction() ? JSParserBuiltinMode::Builtin : JSParserBuiltinMode::NotBuiltin;
    JSParserStrictMode strictMode = isInStrictContext() ? JSParserStrictMode::Strict : JSParserStrictMode::NotStrict;
    JSParserScriptMode _scriptMode = scriptMode();

    bool isClassContext = superBinding() == SuperBinding::Needed;

    UnlinkedFunctionCodeBlock* unlinkedCodeblock = UnlinkedFunctionCodeBlock::create(&vm, FunctionCode, 
        ExecutableInfo(usesEval, isStrictMode, 
        specializationKind == CodeForConstruct, false /* !UnlinkedBuiltinFunction*/, 
        this->constructorKind(), _scriptMode, this->superBinding(),
        parseMode, this->derivedContextType(), false, isClassContext, 
        EvalContextType::FunctionEvalContext), 
        debuggerMode);
    
    //std::string funcName(reinterpret_cast<const char* >(m_name.string().characters8()), m_name.string().length());
    //dataLogLn("Loading codeblock for : ", funcName.c_str());

    unlinkedCodeblock->load(vm, ifs);

    return unlinkedCodeblock;
}   

void UnlinkedFunctionExecutable::destroy(JSCell* cell)
{
    static_cast<UnlinkedFunctionExecutable*>(cell)->~UnlinkedFunctionExecutable();
}

void UnlinkedFunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedFunctionExecutable* thisObject = jsCast<UnlinkedFunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_unlinkedCodeBlockForCall);
    visitor.append(thisObject->m_unlinkedCodeBlockForConstruct);
}

FunctionExecutable* UnlinkedFunctionExecutable::link(VM& vm, const SourceCode& passedParentSource, std::optional<int> overrideLineNumber, Intrinsic intrinsic)
{
    const SourceCode& parentSource = m_parentSourceOverride.isNull() ? passedParentSource : m_parentSourceOverride;
    unsigned firstLine = parentSource.firstLine().oneBasedInt() + m_firstLineOffset;
    unsigned startOffset = parentSource.startOffset() + m_startOffset;
    unsigned lineCount = m_lineCount;

    unsigned startColumn = linkedStartColumn(parentSource.startColumn().oneBasedInt());
    unsigned endColumn = linkedEndColumn(startColumn);

    SourceCode source(parentSource.provider(), startOffset, startOffset + m_sourceLength, firstLine, startColumn);
    FunctionOverrides::OverrideInfo overrideInfo;
    bool hasFunctionOverride = false;

    if (UNLIKELY(Options::functionOverrides())) {
        hasFunctionOverride = FunctionOverrides::initializeOverrideFor(source, overrideInfo);
        if (UNLIKELY(hasFunctionOverride)) {
            firstLine = overrideInfo.firstLine;
            lineCount = overrideInfo.lineCount;
            startColumn = overrideInfo.startColumn;
            endColumn = overrideInfo.endColumn;
            source = overrideInfo.sourceCode;
        }
    }

    FunctionExecutable* result = FunctionExecutable::create(vm, source, this, firstLine + lineCount, endColumn, intrinsic);
    if (overrideLineNumber)
        result->setOverrideLineNumber(*overrideLineNumber);

    if (UNLIKELY(hasFunctionOverride)) {
        result->overrideParameterAndTypeProfilingStartEndOffsets(
            overrideInfo.parametersStartOffset,
            overrideInfo.typeProfilingStartOffset,
            overrideInfo.typeProfilingEndOffset);
    }

    return result;
}

UnlinkedFunctionExecutable* UnlinkedFunctionExecutable::fromGlobalCode(
    const Identifier& name, ExecState& exec, const SourceCode& source, 
    JSObject*& exception, int overrideLineNumber)
{
    ParserError error;
    VM& vm = exec.vm();
    auto& globalObject = *exec.lexicalGlobalObject();
    CodeCache* codeCache = vm.codeCache();
    DebuggerMode debuggerMode = globalObject.hasInteractiveDebugger() ? DebuggerOn : DebuggerOff;
    UnlinkedFunctionExecutable* executable = codeCache->getUnlinkedGlobalFunctionExecutable(vm, name, source, debuggerMode, error);

    if (globalObject.hasDebugger())
        globalObject.debugger()->sourceParsed(&exec, source.provider(), error.line(), error.message());

    if (error.isValid()) {
        exception = error.toErrorObject(&globalObject, source, overrideLineNumber);
        return nullptr;
    }

    return executable;
}

UnlinkedFunctionCodeBlock* UnlinkedFunctionExecutable::unlinkedCodeBlockFor(
    VM& vm, const SourceCode& source, CodeSpecializationKind specializationKind, 
    DebuggerMode debuggerMode, ParserError& error, SourceParseMode parseMode)
{
    switch (specializationKind) {
    case CodeForCall:
        if (UnlinkedFunctionCodeBlock* codeBlock = m_unlinkedCodeBlockForCall.get())
            return codeBlock;
        break;
    case CodeForConstruct:
        if (UnlinkedFunctionCodeBlock* codeBlock = m_unlinkedCodeBlockForConstruct.get())
            return codeBlock;
        break;
    }

    UnlinkedFunctionCodeBlock* result = nullptr;

    if(m_isLoadingByteCodes){
        result = this->loadCode(vm, specializationKind, debuggerMode, 
            isBuiltinFunction() ? UnlinkedBuiltinFunction : UnlinkedNormalFunction, 
            error, parseMode); 
    }
    else {
        result = generateUnlinkedFunctionCodeBlock(
            vm, this, source, specializationKind, debuggerMode, 
            isBuiltinFunction() ? UnlinkedBuiltinFunction : UnlinkedNormalFunction, 
            error, parseMode);
    }

    if (error.isValid())
        return nullptr;

    switch (specializationKind) {
    case CodeForCall:
        m_unlinkedCodeBlockForCall.set(vm, this, result);
        break;
    case CodeForConstruct:
        m_unlinkedCodeBlockForConstruct.set(vm, this, result);
        break;
    }
    return result;
}

void UnlinkedFunctionExecutable::setInvalidTypeProfilingOffsets()
{
    m_typeProfilingStartOffset = std::numeric_limits<unsigned>::max();
    m_typeProfilingEndOffset = std::numeric_limits<unsigned>::max();
}

} // namespace JSC
