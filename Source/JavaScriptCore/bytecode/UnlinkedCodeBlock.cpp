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

#include "UnlinkedCodeBlock.h"

#include "BytecodeGenerator.h"
#include "BytecodeRewriter.h"
#include "ClassInfo.h"
#include "CodeCache.h"
#include "ExecutableInfo.h"
#include "FunctionOverrides.h"
#include "JSCInlines.h"
#include "JSString.h"
#include "Parser.h"
#include "PreciseJumpTargetsInlines.h"
#include "SourceProvider.h"
#include "Structure.h"
#include "SymbolTable.h"
#include "UnlinkedEvalCodeBlock.h"
#include "UnlinkedFunctionCodeBlock.h"
#include "UnlinkedInstructionStream.h"
#include "UnlinkedModuleProgramCodeBlock.h"
#include "UnlinkedProgramCodeBlock.h"
#include <wtf/DataLog.h>

namespace JSC {

const ClassInfo UnlinkedCodeBlock::s_info = { "UnlinkedCodeBlock", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(UnlinkedCodeBlock) };

UnlinkedCodeBlock::UnlinkedCodeBlock(VM* vm, Structure* structure, CodeType codeType, const ExecutableInfo& info, DebuggerMode debuggerMode)
    : Base(*vm, structure)
    , m_numVars(0)
    , m_numCalleeLocals(0)
    , m_numParameters(0)
    , m_globalObjectRegister(VirtualRegister())
    , m_usesEval(info.usesEval())
    , m_isStrictMode(info.isStrictMode())
    , m_isConstructor(info.isConstructor())
    , m_hasCapturedVariables(false)
    , m_isBuiltinFunction(info.isBuiltinFunction())
    , m_superBinding(static_cast<unsigned>(info.superBinding()))
    , m_scriptMode(static_cast<unsigned>(info.scriptMode()))
    , m_isArrowFunctionContext(info.isArrowFunctionContext())
    , m_isClassContext(info.isClassContext())
    , m_wasCompiledWithDebuggingOpcodes(debuggerMode == DebuggerMode::DebuggerOn || Options::forceDebuggerBytecodeGeneration())
    , m_constructorKind(static_cast<unsigned>(info.constructorKind()))
    , m_derivedContextType(static_cast<unsigned>(info.derivedContextType()))
    , m_evalContextType(static_cast<unsigned>(info.evalContextType()))
    , m_lineCount(0)
    , m_endColumn(UINT_MAX)
    , m_didOptimize(MixedTriState)
    , m_parseMode(info.parseMode())
    , m_features(0)
    , m_codeType(codeType)
    , m_arrayProfileCount(0)
    , m_arrayAllocationProfileCount(0)
    , m_objectAllocationProfileCount(0)
    , m_valueProfileCount(0)
    , m_llintCallLinkInfoCount(0)
{
    for (auto& constantRegisterIndex : m_linkTimeConstants)
        constantRegisterIndex = 0;
    ASSERT(m_constructorKind == static_cast<unsigned>(info.constructorKind()));
}

void UnlinkedCodeBlock::load(VM& vm, std::ifstream& ifs, const char* prefix){
    int index; ifs >> index;
    ASSERT(index == 2);

    ifs >> m_numParameters;
    ifs.get(); // newline

    ifs >> index;
    ASSERT (index == 3);

    unsigned instructioncount;
    ifs >> instructioncount;

    int size;
    ifs >> size;

	 // Read an empty space.
	 char space;
	 ifs.read(&space, 1);

    RefCountedArray<unsigned char> data(size);
	ifs.read(reinterpret_cast<char*>(data.begin()), size);
	 //for(int i=0; i<size; i++) {
    //    ifs >> data[i];
    //}

    m_unlinkedInstructions = std::make_unique<UnlinkedInstructionStream>(data, instructioncount);
    
	 int c = ifs.get(); // newline
	 //c = ifs.get(); // newline
	 //c = ifs.get(); // newline
	 //c = ifs.get(); // newline
	 //c = ifs.get(); // newline


    ifs >> index;
    ASSERT (index == 4);

    int regoffset;
    ifs >> regoffset;
    m_thisRegister=VirtualRegister(regoffset);
    ifs >> regoffset;
    m_scopeRegister=VirtualRegister(regoffset);
    ifs >> regoffset;
    m_globalObjectRegister=VirtualRegister(regoffset);

    ifs.get(); // newline

    ifs >> index;
    ASSERT (index == 5);

    int numIdentifiers;
    ifs >> numIdentifiers;
    for(int i=0; i<numIdentifiers; i++) {
        std::string id;
        ifs >> id;
        // TODO :: Handle ES6 symbols
        m_identifiers.append(Identifier::fromString(&vm, reinterpret_cast<const LChar *>(id.c_str()), static_cast<int>(id.size())));
    }

    ifs.get(); // newline

    ifs >> index;
    ASSERT (index == 6);

    int numBitVectors;
    ifs >> numBitVectors;
    for(int i=0; i<numBitVectors; i++) {
        uint32_t bits;
        ifs >> bits;
        BitVector bitVector;
        *(bitVector.bits()) = bits;
        m_bitVectors.append(bitVector);
    }

    ifs.get(); // newline
    
    ifs >> index;
    ASSERT (index == 7);

    int numConstants;
    ifs >> numConstants;
    for(int i=0; i<numConstants; i++) {
        
        uint16_t constTypeVal;
        ifs >> constTypeVal;
        ConstantType constantType = static_cast<ConstantType>(constTypeVal);

        m_constantRegisters.append(WriteBarrier<Unknown>());
        
        switch (constantType) {

            case ConstantType::NonCellValue:
            {
                uint64_t constant;
                uint32_t constRepresentation;

                ifs >> constant;
                ifs >> constRepresentation;        
    
                m_constantRegisters.last().set(vm, this, JSValue::decode(static_cast<EncodedJSValue>(constant)));  
                m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));
            }
                break;
            case ConstantType::Empty:
            {
                std::string constant;
                uint32_t constRepresentation;

                ifs >> constant;
                ifs >> constRepresentation;

                m_constantRegisters.last().set(vm, this, JSValue());  
                m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));

            }
                break;
            case ConstantType::String:
            {
                // std::string constant;
                uint32_t constRepresentation;

					 int length;
					 ifs >> length;

                    // Read an empty space.
                    char space;
                    ifs.read(&space, 1);

					 RefCountedArray<unsigned char> data(length);
					 ifs.read(reinterpret_cast<char*>(data.begin()), length);

                // ifs >> constant;
                ifs >> constRepresentation;

                JSString* jsString = jsOwnedString(&vm, Identifier::fromString(&vm, reinterpret_cast<const LChar*>(data.data()), length).string());
                
                JSValue jsValue(jsString);
                
                m_constantRegisters.last().set(vm, this, jsValue);  
                m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));
            }
                break;
            default:
                ASSERT(0);
                //throw "Unknown constant type.";
        }

    }

    ifs.get(); // newline

    ifs >> index;
    ASSERT (index == 8);

    ifs.get(); // newline
    
    ifs >> index;
    ASSERT (index == 9);

    int numLinktimeConstants;
    ifs >> numLinktimeConstants;
    for(int i=0; i<numLinktimeConstants; i++) {
        unsigned linkTimeConstant;
        ifs >> linkTimeConstant;
        m_linkTimeConstants[i] = linkTimeConstant;
    }

    ifs.get(); // newline
    
    ifs >> index;
    ASSERT (index == 10);

    ifs >> m_arrayProfileCount;
    ifs >> m_arrayAllocationProfileCount; 
    ifs >> m_objectAllocationProfileCount; 
    ifs >> m_valueProfileCount;
    ifs >> m_llintCallLinkInfoCount;

    ifs.get(); // newline

    // misc
    ifs >> index;
    ASSERT (index == 11);
    
    ifs >> m_numVars; 
    ifs >> m_numCapturedVars;
    ifs >> m_numCalleeLocals;
    
    // misc
    ifs >> index;
    ASSERT (index == 12);

    int numFuncDecls;
    ifs >> numFuncDecls;

    for(int i=0; i<numFuncDecls; i++) {
        ifs.get(); // newline
        m_functionDecls.append(WriteBarrier<UnlinkedFunctionExecutable>());
        std::string suffix("d_");
        
        // TODO
        // suffix.append(std::to_string(i));
        std::stringstream ss;
        ss << i;
        suffix.append(ss.str());

        m_functionDecls.last().set(vm, this, UnlinkedFunctionExecutable::create(&vm, ifs, prefix, suffix.c_str()));
    }
    
    // misc
    ifs >> index;
    ASSERT (index == 13);

    int numFuncExprs;
    ifs >> numFuncExprs;

    for(int i=0; i<numFuncExprs; i++) {
        ifs.get(); // newline
        m_functionExprs.append(WriteBarrier<UnlinkedFunctionExecutable>());
        std::string suffix("x_");
        
        // TODO ..
        //suffix.append(std::to_string(i));
        std::stringstream ss;
        ss << i;
        suffix.append(ss.str());

        m_functionExprs.last().set(vm, this, UnlinkedFunctionExecutable::create(&vm, ifs, prefix, suffix.c_str()));
    }

    ifs.get(); // newline    

    ifs >> index;
    ASSERT (index == 14);

    int numSwitchTables;
    ifs >> numSwitchTables;

    if(numSwitchTables > 0) {
        createRareDataIfNecessary();
    }

    for(int i=0; i<numSwitchTables; i++) {
        m_rareData->m_switchJumpTables.append(UnlinkedSimpleJumpTable());
        
        int numBranchOffsets;
        ifs >> numBranchOffsets;
        
        for(int i=0; i< numBranchOffsets; i++) {
            int32_t branchOffset;
            ifs >> branchOffset;
            m_rareData->m_switchJumpTables.last().branchOffsets.append(branchOffset);
        }

        ifs >> m_rareData->m_switchJumpTables.last().min;
    }

    ifs.get(); // newline    
    
    ifs >> index;
    ASSERT (index == 15);

    int numStringSwitchTables;
    ifs >> numStringSwitchTables;

	 if (numStringSwitchTables > 0) {
		 createRareDataIfNecessary();
	 }

    for(int i=0; i<numStringSwitchTables; i++) {
        m_rareData->m_stringSwitchJumpTables.append(UnlinkedStringJumpTable());
        m_rareData->m_stringSwitchJumpTables.last().load(vm, ifs);
    }

    ifs.get(); // newline    

    ifs >> index;
    ASSERT (index == 16);

    int numberOfExceptionHandlers;
    ifs >> numberOfExceptionHandlers;

	 if (numberOfExceptionHandlers > 0) {
		 createRareDataIfNecessary();
	 }

    for(int i=0; i<numberOfExceptionHandlers; i++) {

        uint32_t start;
        uint32_t end;
        uint32_t target;
        uint32_t handlerTypeVal;

        ifs >> start;
        ifs >> end;
        ifs >> target;
        ifs >> handlerTypeVal;

        m_rareData->m_exceptionHandlers.append(UnlinkedHandlerInfo(start, end, target, static_cast<HandlerType>(handlerTypeVal)));
    }

    ifs.get(); // newline    
    
    ifs >> index;
    ASSERT (index == 17);

    int numbRegexps;
    ifs >> numbRegexps;

        if (numbRegexps > 0) {
            createRareDataIfNecessary();
        }

    for(int i=0; i<numbRegexps; i++) {

        std::string patternString;
        /*RegExpFlags*/ uint16_t flagsVal;
        /*RegExpState*/ uint16_t stateVal;
        
        ifs >> patternString;
        ifs >> flagsVal;
        ifs >> stateVal;

        WTF::String pattern(reinterpret_cast<const char*>(patternString.c_str()), patternString.length());
        m_rareData->m_regexps.append(WriteBarrier<RegExp>(vm, this, 
            RegExp::create(vm, pattern, static_cast<RegExpFlags>(flagsVal))));
        
        // TODO :: SHould we reset the state 
    }

    ifs.get(); // newline    
    
    ifs >> index;
    ASSERT (index == 18) ;

    int numConstantBuffers;
    ifs >> numConstantBuffers;

    if (numConstantBuffers > 0) {
        createRareDataIfNecessary();
    }

    for(int i=0; i<numConstantBuffers; i++) {
        int numConstants;
        ifs >> numConstants;
        m_rareData->m_constantBuffers.append(Vector<JSValue>(numConstants));
        
        for(int i=0; i<numConstants; i++) {
            uint16_t constTypeVal;
            ifs >> constTypeVal;
            ConstantType constantType = static_cast<ConstantType>(constTypeVal);
    
            switch (constantType) {
    
                case ConstantType::NonCellValue:
                {
                    uint64_t constant;
                    ifs >> constant;
                    
                    m_rareData->m_constantBuffers.last().append(JSValue::decode(static_cast<EncodedJSValue>(constant)));  
                }
                    break;

                    case ConstantType::Empty:
                {
                    std::string constant;    
                    ifs >> constant;
    
                    m_rareData->m_constantBuffers.last().append(JSValue());  
    
                }
                    break;

                case ConstantType::String:
                {
                    int constRegisterIndex;
                    ifs >> constRegisterIndex;
    
                    m_rareData->m_constantBuffers.last().append(m_constantRegisters[constRegisterIndex].get());
                }
                    break;

                default:
                    ASSERT(0);
                    //throw "Unknown constant type.";
            }
         

            //int64_t jsValueEnc;
            //ifs >> jsValueEnc;

            //m_rareData->m_constantBuffers.last().append(JSValue::decode(static_cast<EncodedJSValue>(jsValueEnc)));
        }
    }
}

void UnlinkedCodeBlock::save(VM& vm, std::ofstream&stream){

    
    stream << "2 " << m_numParameters << std::endl;
    
    // end of metadata

    // start instuctions

    stream<< "3 " << m_unlinkedInstructions->m_instructionCount;
    stream<<" ";
    stream<<m_unlinkedInstructions->m_data.size();
    stream<<" ";

    
    stream.write(reinterpret_cast<char*>(m_unlinkedInstructions->m_data.data()), m_unlinkedInstructions->m_data.size());
    //for(int idx=0; idx < m_unlinkedInstructions->m_data.size(); idx++){
    //    stream<<m_unlinkedInstructions->m_data.at(idx);
    //}

    stream<<std::endl;

    /// End of instructions

    // Start (virtual) registers

    stream << "4 " << m_thisRegister.offset() << " " << m_scopeRegister.offset() 
        << " " <<  m_globalObjectRegister.offset() << std::endl;

    // end registers

    stream << "5 ";

    // TODO :: Serialize bytecode liveness

    stream<<m_identifiers.size()<<" ";
    for (auto &identifier : m_identifiers){
        stream<<std::string(reinterpret_cast<const char* >(identifier.string().characters8()), identifier.string().length());
		stream << " ";
    }

    stream<<std::endl;

    /// End of identifiers

    stream << "6 ";

    stream<<m_bitVectors.size()<<" ";
    // Bit vectors
    for (auto &bitVector : m_bitVectors){
        stream << *bitVector.bits();
        stream << " ";
    }

    stream<<std::endl;
    
    // Constants

    stream << "7 ";

    stream<<constantRegisters().size()<<" ";
    if (!constantRegisters().isEmpty()) {
        size_t i = 0;
        for (const auto& constant : constantRegisters()) {
            
            JSValue value = constant.get();
            if(value.isCell()) {
                if(value.isEmpty()) {
                    stream<<static_cast<int16_t>(ConstantType::Empty);
                    stream << " ";

                    stream<<"X";
                    stream << " ";
                    stream<<static_cast<int32_t>(constantsSourceCodeRepresentation()[i]);
                    stream << " ";
                }
                else if(value.isString()) {
                    stream<<static_cast<int16_t>(ConstantType::String);
                    stream << " ";

                    const String& constStr = static_cast<const JSString*>(value.asCell())->tryGetValue();
                    std::string str(reinterpret_cast<const char* >(constStr.characters8()), constStr.length());

						  stream << constStr.length();
						  stream << " ";
						  stream.write(reinterpret_cast<const char* >(constStr.characters8()), constStr.length());
						  stream << " ";

                          stream<<static_cast<int32_t>(constantsSourceCodeRepresentation()[i]);
                    stream << " ";
                }
                else {
                    // We don't support yet.
                    ASSERT(0);
                }
            }
            else {
                stream<<static_cast<int16_t>(ConstantType::NonCellValue);
                stream << " ";
                stream<<static_cast<int64_t>(JSValue::encode(constant.get()));
                stream << " ";
                stream<<static_cast<int32_t>(constantsSourceCodeRepresentation()[i]);
                stream << " ";
            }

            ++i;
        }
    }

    stream<<std::endl;    

    stream << "8 ";

    for (auto &constIdSetEntry : m_constantIdentifierSets){
        unsigned val = constIdSetEntry.second;
        stream << val;
        IdentifierSet idSet = constIdSetEntry.first;
        stream << "{";
        for (auto &identifier : idSet){
            stream<<std::string(reinterpret_cast<const char* >(identifier->characters8()), identifier->length());
            stream<<" ";
        }
        stream << "}";
    }

    stream<<std::endl;

    // lint time constants

    stream << "9 ";

    stream<<m_linkTimeConstants.size()<<" ";
    for (auto linkTimeConstant : m_linkTimeConstants) {
        stream<<linkTimeConstant << " ";
    }

    stream<<std::endl;

    // Profile counts

    stream << "10 ";

    stream << m_arrayProfileCount << " " << m_arrayAllocationProfileCount << " " 
        << m_objectAllocationProfileCount << " " << m_valueProfileCount << " " 
        << m_llintCallLinkInfoCount << std::endl;


    // misc
    stream << "11 ";

    stream << m_numVars << " " << m_numCapturedVars << " " 
    << m_numCalleeLocals << std::endl;

    stream << "12 " << m_functionDecls.size() << std::endl;
    for(WriteBarrier<UnlinkedFunctionExecutable> f : m_functionDecls) {
        UnlinkedFunctionExecutable* ufexe = f.get();
        ufexe->saveNonCode(vm, stream);
        stream << std::endl;
    }

    stream << "13 " << m_functionExprs.size() << std::endl;
    for(WriteBarrier<UnlinkedFunctionExecutable> f : m_functionExprs) {
        UnlinkedFunctionExecutable* ufexe = f.get();
        ufexe->saveNonCode(vm, stream);
        stream << std::endl;
    }

    stream << "14 " << numberOfSwitchJumpTables() << " " ;
    for(int i=0; i<numberOfSwitchJumpTables(); i++) {
        stream << switchJumpTable(i).branchOffsets.size() << " ";
        for (int branchOffset : switchJumpTable(i).branchOffsets) {
            stream << branchOffset << " ";
        }
        stream << switchJumpTable(i).min << " ";
    }

    stream << std::endl;

    stream << "15 " << numberOfStringSwitchJumpTables() << " " ;
    for(int i=0; i<numberOfStringSwitchJumpTables(); i++) {
        stringSwitchJumpTable(i).save(vm, stream);
    }

    stream << std::endl;

    stream << "16 " << numberOfExceptionHandlers() << " " ;
    for(int i=0; i<numberOfExceptionHandlers(); i++) {
        UnlinkedHandlerInfo& unlinkedHandlerInfo(exceptionHandler(i));
        
        stream << unlinkedHandlerInfo.start << " " << unlinkedHandlerInfo.end << 
            " " << unlinkedHandlerInfo.target << " " << unlinkedHandlerInfo.typeBits << " ";

    }

    stream << std::endl;

    stream << "17 " << numberOfRegExps() << " " ;
    for(int i=0; i<numberOfRegExps(); i++) {
        RegExp* reg = regexp(i);
        reg->save(vm, stream);
    }

    stream << std::endl;


    stream << "18 " << constantBufferCount() << " " ;
    for(int i=0; i<constantBufferCount(); i++) {
        ConstantBuffer& buffer = constantBuffer(i);
        stream << buffer.size() << " ";
        
        for(JSValue value : buffer) {
            if(value.isCell()) {
                
                // Copied from constant regs.
                if(value.isEmpty()) {
                    stream<<static_cast<int16_t>(ConstantType::Empty);
                    stream << " ";

                    stream<<"X";
                    stream << " ";
                }
                else if(value.isString()) {

                    // Store index into constant regs.

                    JSString* bufferVal = JSC::asString(value);
                    const String& bufferStringVal = bufferVal->tryGetValue();

                    int constantIndex = 0;
                    for(auto constant : m_constantRegisters) {
                        JSValue jsValue = constant.get();
                    
                        if(jsValue.isString()) {

                            JSString* jsString = JSC::asString(jsValue);
                            const String& stringVal = jsString->tryGetValue();

                            if(WTF::equal(stringVal.impl(), bufferStringVal.impl())) {
                                break;
                            }
                        }
                        constantIndex++;
                    }

                    if(constantIndex >= m_constantRegisters.size()) {
                        // Couldn't find the constant.. Bail out.
                       //"Can't find the constant in the set of constant regiseter.. I can't handle this.";
                        ASSERT(0);
                    }

                    stream<<static_cast<int16_t>(ConstantType::String);
                    stream << " ";
    
                    stream<<constantIndex;
                    stream << " ";

                }
                else {
                    ASSERT(0);
                    //throw "We don't support this constant type";
                }

                
            }
            else {
                stream<<static_cast<int16_t>(ConstantType::NonCellValue);
                stream << " ";

                EncodedJSValue valueEnc = JSValue::encode(value);
                stream << valueEnc << " ";
            }
        }
    }

    stream << std::endl;
}

void UnlinkedStringJumpTable::save(VM& vm, std::ofstream&stream) {
    stream << offsetTable.size() << " ";
    for( auto& entry : offsetTable) {
        RefPtr<StringImpl> key = entry.key;
        int32_t value = entry.value.branchOffset;
		  std::string keystr(reinterpret_cast<const char*>(key->characters8()), key->length());

        stream << keystr << " ";
        stream << value << " ";
    }
}

void UnlinkedStringJumpTable::load(VM& vm, std::ifstream&stream) {
    int32_t offsetTableSize;
    stream >> offsetTableSize;

    for(int i=0; i < offsetTableSize; i++) {
        std::string keystr;
        int32_t value;

        stream >> keystr;
        stream >> value;

        struct OffsetLocation location;
        location.branchOffset = value;

        //Ref<StringImpl> key = ;
        //RefPtr<StringImpl> keyPtr (StringImpl::create(keystr.c_str(), keystr.length()));
        offsetTable.add(StringImpl::create(keystr.c_str(), keystr.length()), location);
    }
}


void UnlinkedCodeBlock::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedCodeBlock* thisObject = jsCast<UnlinkedCodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    auto locker = holdLock(*thisObject);
    for (FunctionExpressionVector::iterator ptr = thisObject->m_functionDecls.begin(), end = thisObject->m_functionDecls.end(); ptr != end; ++ptr)
        visitor.append(*ptr);
    for (FunctionExpressionVector::iterator ptr = thisObject->m_functionExprs.begin(), end = thisObject->m_functionExprs.end(); ptr != end; ++ptr)
        visitor.append(*ptr);
    visitor.appendValues(thisObject->m_constantRegisters.data(), thisObject->m_constantRegisters.size());
    if (thisObject->m_unlinkedInstructions)
        visitor.reportExtraMemoryVisited(thisObject->m_unlinkedInstructions->sizeInBytes());
    if (thisObject->m_rareData) {
        for (size_t i = 0, end = thisObject->m_rareData->m_regexps.size(); i != end; i++)
            visitor.append(thisObject->m_rareData->m_regexps[i]);
    }
}

size_t UnlinkedCodeBlock::estimatedSize(JSCell* cell)
{
    UnlinkedCodeBlock* thisObject = jsCast<UnlinkedCodeBlock*>(cell);
    size_t extraSize = thisObject->m_unlinkedInstructions ? thisObject->m_unlinkedInstructions->sizeInBytes() : 0;
    return Base::estimatedSize(cell) + extraSize;
}

int UnlinkedCodeBlock::lineNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    ASSERT(bytecodeOffset < instructions().count());
    int divot;
    int startOffset;
    int endOffset;
    unsigned line;
    unsigned column;
    expressionRangeForBytecodeOffset(bytecodeOffset, divot, startOffset, endOffset, line, column);
    return line;
}

inline void UnlinkedCodeBlock::getLineAndColumn(const ExpressionRangeInfo& info,
    unsigned& line, unsigned& column) const
{
    switch (info.mode) {
    case ExpressionRangeInfo::FatLineMode:
        info.decodeFatLineMode(line, column);
        break;
    case ExpressionRangeInfo::FatColumnMode:
        info.decodeFatColumnMode(line, column);
        break;
    case ExpressionRangeInfo::FatLineAndColumnMode: {
        unsigned fatIndex = info.position;
        ExpressionRangeInfo::FatPosition& fatPos = m_rareData->m_expressionInfoFatPositions[fatIndex];
        line = fatPos.line;
        column = fatPos.column;
        break;
    }
    } // switch
}

#ifndef NDEBUG
static void dumpLineColumnEntry(size_t index, const UnlinkedInstructionStream& instructionStream, unsigned instructionOffset, unsigned line, unsigned column)
{
    const auto& instructions = instructionStream.unpackForDebugging();
    OpcodeID opcode = instructions[instructionOffset].u.opcode;
    const char* event = "";
    if (opcode == op_debug) {
        switch (instructions[instructionOffset + 1].u.operand) {
        case WillExecuteProgram: event = " WillExecuteProgram"; break;
        case DidExecuteProgram: event = " DidExecuteProgram"; break;
        case DidEnterCallFrame: event = " DidEnterCallFrame"; break;
        case DidReachBreakpoint: event = " DidReachBreakpoint"; break;
        case WillLeaveCallFrame: event = " WillLeaveCallFrame"; break;
        case WillExecuteStatement: event = " WillExecuteStatement"; break;
        case WillExecuteExpression: event = " WillExecuteExpression"; break;
        }
    }
    dataLogF("  [%zu] pc %u @ line %u col %u : %s%s\n", index, instructionOffset, line, column, opcodeNames[opcode], event);
}

void UnlinkedCodeBlock::dumpExpressionRangeInfo()
{
    Vector<ExpressionRangeInfo>& expressionInfo = m_expressionInfo;

    size_t size = m_expressionInfo.size();
    dataLogF("UnlinkedCodeBlock %p expressionRangeInfo[%zu] {\n", this, size);
    for (size_t i = 0; i < size; i++) {
        ExpressionRangeInfo& info = expressionInfo[i];
        unsigned line;
        unsigned column;
        getLineAndColumn(info, line, column);
        dumpLineColumnEntry(i, instructions(), info.instructionOffset, line, column);
    }
    dataLog("}\n");
}
#endif

void UnlinkedCodeBlock::expressionRangeForBytecodeOffset(unsigned bytecodeOffset,
    int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column) const
{
    ASSERT(bytecodeOffset < instructions().count());

    if (!m_expressionInfo.size()) {
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        line = 0;
        column = 0;
        return;
    }

    const Vector<ExpressionRangeInfo>& expressionInfo = m_expressionInfo;

    int low = 0;
    int high = expressionInfo.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (expressionInfo[mid].instructionOffset <= bytecodeOffset)
            low = mid + 1;
        else
            high = mid;
    }

    if (!low)
        low = 1;

    const ExpressionRangeInfo& info = expressionInfo[low - 1];
    startOffset = info.startOffset;
    endOffset = info.endOffset;
    divot = info.divotPoint;
    getLineAndColumn(info, line, column);
}

void UnlinkedCodeBlock::addExpressionInfo(unsigned instructionOffset,
    int divot, int startOffset, int endOffset, unsigned line, unsigned column)
{
    if (divot > ExpressionRangeInfo::MaxDivot) {
        // Overflow has occurred, we can only give line number info for errors for this region
        divot = 0;
        startOffset = 0;
        endOffset = 0;
    } else if (startOffset > ExpressionRangeInfo::MaxOffset) {
        // If the start offset is out of bounds we clear both offsets
        // so we only get the divot marker. Error message will have to be reduced
        // to line and charPosition number.
        startOffset = 0;
        endOffset = 0;
    } else if (endOffset > ExpressionRangeInfo::MaxOffset) {
        // The end offset is only used for additional context, and is much more likely
        // to overflow (eg. function call arguments) so we are willing to drop it without
        // dropping the rest of the range.
        endOffset = 0;
    }

    unsigned positionMode =
        (line <= ExpressionRangeInfo::MaxFatLineModeLine && column <= ExpressionRangeInfo::MaxFatLineModeColumn) 
        ? ExpressionRangeInfo::FatLineMode
        : (line <= ExpressionRangeInfo::MaxFatColumnModeLine && column <= ExpressionRangeInfo::MaxFatColumnModeColumn)
        ? ExpressionRangeInfo::FatColumnMode
        : ExpressionRangeInfo::FatLineAndColumnMode;

    ExpressionRangeInfo info;
    info.instructionOffset = instructionOffset;
    info.divotPoint = divot;
    info.startOffset = startOffset;
    info.endOffset = endOffset;

    info.mode = positionMode;
    switch (positionMode) {
    case ExpressionRangeInfo::FatLineMode:
        info.encodeFatLineMode(line, column);
        break;
    case ExpressionRangeInfo::FatColumnMode:
        info.encodeFatColumnMode(line, column);
        break;
    case ExpressionRangeInfo::FatLineAndColumnMode: {
        createRareDataIfNecessary();
        unsigned fatIndex = m_rareData->m_expressionInfoFatPositions.size();
        ExpressionRangeInfo::FatPosition fatPos = { line, column };
        m_rareData->m_expressionInfoFatPositions.append(fatPos);
        info.position = fatIndex;
    }
    } // switch

    m_expressionInfo.append(info);
}

bool UnlinkedCodeBlock::typeProfilerExpressionInfoForBytecodeOffset(unsigned bytecodeOffset, unsigned& startDivot, unsigned& endDivot)
{
    static const bool verbose = false;
    if (!m_rareData) {
        if (verbose)
            dataLogF("Don't have assignment info for offset:%u\n", bytecodeOffset);
        startDivot = UINT_MAX;
        endDivot = UINT_MAX;
        return false;
    }

    auto iter = m_rareData->m_typeProfilerInfoMap.find(bytecodeOffset);
    if (iter == m_rareData->m_typeProfilerInfoMap.end()) {
        if (verbose)
            dataLogF("Don't have assignment info for offset:%u\n", bytecodeOffset);
        startDivot = UINT_MAX;
        endDivot = UINT_MAX;
        return false;
    }
    
    RareData::TypeProfilerExpressionRange& range = iter->value;
    startDivot = range.m_startDivot;
    endDivot = range.m_endDivot;
    return true;
}

void UnlinkedCodeBlock::addTypeProfilerExpressionInfo(unsigned instructionOffset, unsigned startDivot, unsigned endDivot)
{
    createRareDataIfNecessary();
    RareData::TypeProfilerExpressionRange range;
    range.m_startDivot = startDivot;
    range.m_endDivot = endDivot;
    m_rareData->m_typeProfilerInfoMap.set(instructionOffset, range);
}

UnlinkedCodeBlock::~UnlinkedCodeBlock()
{
}

void UnlinkedCodeBlock::setInstructions(std::unique_ptr<UnlinkedInstructionStream> instructions)
{
    ASSERT(instructions);
    {
        auto locker = holdLock(*this);
        m_unlinkedInstructions = WTFMove(instructions);
    }
    Heap::heap(this)->reportExtraMemoryAllocated(m_unlinkedInstructions->sizeInBytes());
}

const UnlinkedInstructionStream& UnlinkedCodeBlock::instructions() const
{
    ASSERT(m_unlinkedInstructions.get());
    return *m_unlinkedInstructions;
}

UnlinkedHandlerInfo* UnlinkedCodeBlock::handlerForBytecodeOffset(unsigned bytecodeOffset, RequiredHandler requiredHandler)
{
    return handlerForIndex(bytecodeOffset, requiredHandler);
}

UnlinkedHandlerInfo* UnlinkedCodeBlock::handlerForIndex(unsigned index, RequiredHandler requiredHandler)
{
    if (!m_rareData)
        return nullptr;
    return UnlinkedHandlerInfo::handlerForIndex(m_rareData->m_exceptionHandlers, index, requiredHandler);
}

void UnlinkedCodeBlock::applyModification(BytecodeRewriter& rewriter)
{
    // Before applying the changes, we adjust the jumps based on the original bytecode offset, the offset to the jump target, and
    // the insertion information.

    BytecodeGraph<UnlinkedCodeBlock>& graph = rewriter.graph();
    UnlinkedInstruction* instructionsBegin = graph.instructions().begin();

    for (int bytecodeOffset = 0, instructionCount = graph.instructions().size(); bytecodeOffset < instructionCount;) {
        UnlinkedInstruction* current = instructionsBegin + bytecodeOffset;
        OpcodeID opcodeID = current[0].u.opcode;
        extractStoredJumpTargetsForBytecodeOffset(this, instructionsBegin, bytecodeOffset, [&](int32_t& relativeOffset) {
            relativeOffset = rewriter.adjustJumpTarget(bytecodeOffset, bytecodeOffset + relativeOffset);
        });
        bytecodeOffset += opcodeLength(opcodeID);
    }

    // Then, exception handlers should be adjusted.
    if (m_rareData) {
        for (UnlinkedHandlerInfo& handler : m_rareData->m_exceptionHandlers) {
            handler.target = rewriter.adjustAbsoluteOffset(handler.target);
            handler.start = rewriter.adjustAbsoluteOffset(handler.start);
            handler.end = rewriter.adjustAbsoluteOffset(handler.end);
        }

        for (size_t i = 0; i < m_rareData->m_opProfileControlFlowBytecodeOffsets.size(); ++i)
            m_rareData->m_opProfileControlFlowBytecodeOffsets[i] = rewriter.adjustAbsoluteOffset(m_rareData->m_opProfileControlFlowBytecodeOffsets[i]);

        if (!m_rareData->m_typeProfilerInfoMap.isEmpty()) {
            HashMap<unsigned, RareData::TypeProfilerExpressionRange> adjustedTypeProfilerInfoMap;
            for (auto& entry : m_rareData->m_typeProfilerInfoMap)
                adjustedTypeProfilerInfoMap.set(rewriter.adjustAbsoluteOffset(entry.key), entry.value);
            m_rareData->m_typeProfilerInfoMap.swap(adjustedTypeProfilerInfoMap);
        }
    }

    for (size_t i = 0; i < m_propertyAccessInstructions.size(); ++i)
        m_propertyAccessInstructions[i] = rewriter.adjustAbsoluteOffset(m_propertyAccessInstructions[i]);

    for (size_t i = 0; i < m_expressionInfo.size(); ++i)
        m_expressionInfo[i].instructionOffset = rewriter.adjustAbsoluteOffset(m_expressionInfo[i].instructionOffset);

    // Then, modify the unlinked instructions.
    rewriter.applyModification();

    // And recompute the jump target based on the modified unlinked instructions.
    m_jumpTargets.clear();
    recomputePreciseJumpTargets(this, graph.instructions().begin(), graph.instructions().size(), m_jumpTargets);
}

void UnlinkedCodeBlock::shrinkToFit()
{
    auto locker = holdLock(*this);
    
    m_jumpTargets.shrinkToFit();
    m_identifiers.shrinkToFit();
    m_bitVectors.shrinkToFit();
    m_constantRegisters.shrinkToFit();
    m_constantsSourceCodeRepresentation.shrinkToFit();
    m_functionDecls.shrinkToFit();
    m_functionExprs.shrinkToFit();
    m_propertyAccessInstructions.shrinkToFit();
    m_expressionInfo.shrinkToFit();

    if (m_rareData) {
        m_rareData->m_exceptionHandlers.shrinkToFit();
        m_rareData->m_regexps.shrinkToFit();
        m_rareData->m_constantBuffers.shrinkToFit();
        m_rareData->m_switchJumpTables.shrinkToFit();
        m_rareData->m_stringSwitchJumpTables.shrinkToFit();
        m_rareData->m_expressionInfoFatPositions.shrinkToFit();
        m_rareData->m_opProfileControlFlowBytecodeOffsets.shrinkToFit();
    }
}

void UnlinkedCodeBlock::dump(PrintStream&) const
{
}

} // namespace JSC
