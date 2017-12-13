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

#include "BuiltinNames.h"

#include "ByteCodeProvider.h"

#include <sstream>

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

void UnlinkedCodeBlock::load(VM& vm) {

    VERIFYMAGIC(MAGIC_CODEBLOCK);
    READFIELD(m_numParameters);
    
    loadInstructions(vm);
    loadVirtualRegisters(vm);
    loadIdentifiers(vm);
    loadBitVectors(vm);
    loadConstants(vm);
    loadConstantIdentifierSets(vm);
    loadLinkTimeConstants(vm);
    loadProfileCounts(vm);
    loadMisc(vm);
    loadFunctionDecls(vm);
    loadFunctionExprs(vm);
    loadSwitchJumpTables(vm);
    loadStringSwitchJumpTables(vm);
    loadExceptionHandlers(vm);
    loadRegexps(vm);
    loadConstantBuffers(vm);
    loadPropertyAccessInstructions(vm);
    loadJumpTargets(vm);
    loadExpressionRangeInfos(vm);    

    VERIFYMAGIC(MAGIC_CODEBLOCK_END);

    //dataLogLndataLogLn("Loading completed ...");

    vm.byteCodeProvider().noCacheNow();
}

void UnlinkedCodeBlock::save(VM& vm) {

    WRITEMAGIC(MAGIC_CODEBLOCK);
    WRITEFIELD(m_numParameters);
    
    // instructions

    saveInstructions(vm);
    saveVirtualRegisters(vm);
    saveIdentifiers(vm);
    saveBitVectors(vm);
    saveConstants(vm);
    saveConstantIdentifierSets(vm);
    saveLinkTimeConstants(vm);
    saveProfileCounts(vm);
    saveMisc(vm);
    saveFunctionDecls(vm);
    saveFunctionExprs(vm);
    saveSwitchJumpTables(vm);
    saveStringSwitchJumpTables(vm);
    saveExceptionHandlers(vm);
    saveRegexps(vm);
    saveConstantBuffers(vm);
    savePropertyAccessInstructions(vm);
    saveJumpTargets(vm);
    saveExpressionRangeInfos(vm);

    WRITEMAGIC(MAGIC_CODEBLOCK_END);
}    

void UnlinkedCodeBlock::saveInstructions(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_INSTRUCTIONS);
    
    unsigned instructionCount = m_unlinkedInstructions->m_instructionCount;
    WRITEFIELD(instructionCount);
    
    size_t instructionStreamSize = m_unlinkedInstructions->m_data.size();
    WRITEFIELD(instructionStreamSize);
    vm.byteCodeProvider().outStream.write(reinterpret_cast<char*>(m_unlinkedInstructions->m_data.data()), instructionStreamSize);
}

void UnlinkedCodeBlock::loadInstructions(VM& vm) {
    VERIFYMAGIC(MAGIC_CODEBLOCK_INSTRUCTIONS);

    unsigned instructioncount;
    READFIELD(instructioncount);
    
    size_t instructionStreamSize;
    READFIELD(instructionStreamSize);

    RefCountedArray<unsigned char> data(instructionStreamSize);
    vm.byteCodeProvider().readBytes(reinterpret_cast<char*>(data.begin()), instructionStreamSize);
    
    m_unlinkedInstructions = std::make_unique<UnlinkedInstructionStream>(data, instructioncount);
}

void UnlinkedCodeBlock::saveVirtualRegisters(VM&vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_VIRTUALREGISTERS);
    
    int registerOffset = m_thisRegister.offset();
    WRITEFIELD(registerOffset);

    registerOffset = m_scopeRegister.offset();
    WRITEFIELD(registerOffset);

    registerOffset = m_globalObjectRegister.offset();
    WRITEFIELD(registerOffset);

}

void UnlinkedCodeBlock::loadVirtualRegisters(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_VIRTUALREGISTERS);
    
    int regoffset;
    READFIELD(regoffset);
    m_thisRegister=VirtualRegister(regoffset);
    READFIELD(regoffset);
    m_scopeRegister=VirtualRegister(regoffset);
    READFIELD(regoffset);
    m_globalObjectRegister=VirtualRegister(regoffset);
}

void UnlinkedCodeBlock::saveIdentifiers(VM& vm) {

    WRITEMAGIC(MAGIC_CODEBLOCK_IDENTIFIERS);
    
    size_t numIdentifiers = m_identifiers.size();
    WRITEFIELD(numIdentifiers);
    
    uint8_t identifierType;
    for (auto &identifier : m_identifiers) {
        if (identifier.isPrivateName()) {
            int privateNameIndex = vm.propertyNames->builtinNames().findPrivateNameIndex(identifier);

            if(privateNameIndex >= 0) {
                identifierType = static_cast<uint8_t>(IdentifierType::BuiltinPrivateName);
                WRITEFIELD(identifierType);
                WRITEFIELD(privateNameIndex);
            } else {
                identifierType = static_cast<uint8_t>(IdentifierType::PrivateName);
                WRITEFIELD(identifierType);

                int identifierLength = identifier.length();
                WRITEFIELD(identifierLength);
                
                // TODO :: this can be 16 ..
                vm.byteCodeProvider().outStream.write(reinterpret_cast<const char* >(identifier.string().characters8()), identifier.string().length());
            }
        }
        else if(identifier.isSymbol()) {
            int symbolIndex = vm.propertyNames->findCommonSymbol(identifier);

            if (symbolIndex >= 0) {
                identifierType = static_cast<uint8_t>(IdentifierType::WellKnownSymbol);
                WRITEFIELD(identifierType);
                WRITEFIELD(symbolIndex);
            }
            else {
                identifierType = static_cast<uint8_t>(IdentifierType::Symbol);
                WRITEFIELD(identifierType);

                int identifierLength = identifier.length();
                WRITEFIELD(identifierLength);
                
                // TODO :: this can be 16 ..
                vm.byteCodeProvider().outStream.write(reinterpret_cast<const char* >(identifier.string().characters8()), identifier.string().length());
            }
        } else {
            int commonIdIndex = vm.propertyNames->findCommonPropName(identifier);
            if(commonIdIndex >= 0) {
                identifierType = static_cast<uint8_t>(IdentifierType::CommonIdentifier);
                WRITEFIELD(identifierType);
                WRITEFIELD(commonIdIndex);
            } else {
                identifierType = static_cast<uint8_t>(IdentifierType::Normal);
                WRITEFIELD(identifierType);      
                
                int identifierLength = identifier.length();
                WRITEFIELD(identifierLength);

                // TODO :: This can be 16
                vm.byteCodeProvider().outStream.write(reinterpret_cast<const char* >(identifier.string().characters8()), identifier.string().length());
            }
        }
    }   
}

void UnlinkedCodeBlock::loadIdentifiers(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_IDENTIFIERS);

    size_t numIdentifiers;
    uint8_t identifierType;
    int identifierIndex;
    READFIELD(numIdentifiers);
    for(size_t i=0; i<numIdentifiers; i++) {
        READFIELD(identifierType);
        switch(static_cast<IdentifierType>(identifierType)) {
            case IdentifierType::CommonIdentifier:{
                READFIELD(identifierIndex);
                m_identifiers.append(vm.propertyNames->getCommonPropNameIdenfier(identifierIndex));
            }
            break;

            case IdentifierType::WellKnownSymbol: {
                READFIELD(identifierIndex);
                m_identifiers.append(vm.propertyNames->getCommonSymbolIdenfier(identifierIndex));
            }
            break;

            case IdentifierType::Symbol: {
					int idlength;
					READFIELD(idlength);

					ASSERT(idlength >= 0);
					if (idlength > 0) {
						ASSERT(0); // Need to implement this.
					}
					else {
						m_identifiers.append(Identifier::fromUid(PrivateName()));
					}
            }
            break; 

            case IdentifierType::PrivateName: {
                int idlength;
                READFIELD(idlength);
                
                ASSERT(idlength >=0 );
                if(idlength > 0) {
                    RefCountedArray<unsigned char> data(idlength);
                    vm.byteCodeProvider().readBytes(reinterpret_cast<char*>(data.begin()), idlength);

                    ASSERT(0); // Need to implement this.
                } else {
                    m_identifiers.append(Identifier::fromUid(PrivateName()));
                }
            }
            break;

            case IdentifierType::BuiltinPrivateName: {
                READFIELD(identifierIndex);
                ASSERT(identifierIndex >= 0);

                m_identifiers.append(vm.propertyNames->builtinNames().getPrivateNameIdentifier(identifierIndex));
            }
            break;

            case IdentifierType::Normal: {
                int idlength;
                READFIELD(idlength);
                        
                RefCountedArray<unsigned char> data(idlength);
                vm.byteCodeProvider().readBytes(reinterpret_cast<char*>(data.begin()), idlength);
        
                // TODO :: Handle 16
                m_identifiers.append(Identifier::fromString(&vm, reinterpret_cast<const LChar *>(data.data()), idlength));
            }
            break;

            default:
                ASSERT(0);
        }
    }
}

void UnlinkedCodeBlock::saveBitVectors(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_BITVECTORS);
    
    size_t bitVectorSize = m_bitVectors.size();
    WRITEFIELD(bitVectorSize);
    for (auto &bitVector : m_bitVectors){
        uintptr_t bits = *bitVector.bits();
        WRITEFIELD(bits);
        //stream << *bitVector.bits();
        //stream << " ";
    }
}

void UnlinkedCodeBlock::loadBitVectors(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_BITVECTORS);

    size_t numBitVectors;
    READFIELD(numBitVectors);
    for(size_t i=0; i<numBitVectors; i++) {
        uintptr_t bits;
        READFIELD(bits);

        BitVector bitVector;
        *(bitVector.bits()) = bits;
        m_bitVectors.append(bitVector);
    }
}

void UnlinkedCodeBlock::saveConstants(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_CONSTANTS);
    
    size_t constantRegistersSize = constantRegisters().size();
    WRITEFIELD(constantRegistersSize);
    
    if (!constantRegisters().isEmpty()) {
        size_t i = 0;
        for (const auto& constant : constantRegisters()) {
            
            JSValue value = constant.get();
            if(value.isCell()) {
                if(value.isEmpty()) {
                    uint8_t constantType = static_cast<uint8_t>(ConstantType::Empty);
                    WRITEFIELD(constantType);

                    int32_t sourceCodeRepresentation = static_cast<int32_t>(constantsSourceCodeRepresentation()[i]);
                    WRITEFIELD(sourceCodeRepresentation);
                }
                else if(value.isString()) {
                    uint8_t constantType = static_cast<uint8_t>(ConstantType::String);
                    WRITEFIELD(constantType);

                    const String& constStr = static_cast<const JSString*>(value.asCell())->tryGetValue();
                    bool is8bit = constStr.is8Bit();

                    WRITEFIELD(is8bit);
                    
                    unsigned length = constStr.length();
                    WRITEFIELD(length);

                    if(is8bit) {
                        vm.byteCodeProvider().outStream.write(reinterpret_cast<const char* >(constStr.characters8()), constStr.length());
                    } else {
                        vm.byteCodeProvider().outStream.write(reinterpret_cast<const char* >(constStr.characters16()), 2*constStr.length());
                    }

                    int32_t sourceCodeRepresentation = static_cast<int32_t>(constantsSourceCodeRepresentation()[i]);
                    WRITEFIELD(sourceCodeRepresentation);
                }
                else if (value.asCell()->classInfo(vm) == SymbolTable::info()) {
                    uint8_t constantType = static_cast<uint8_t>(ConstantType::SymbolTable);
                    WRITEFIELD(constantType);

                    SymbolTable* symbolTable = jsCast<SymbolTable*>(value);
                    symbolTable->save(vm, this->identifiers());

                    int32_t sourceCodeRepresentation = static_cast<int32_t>(constantsSourceCodeRepresentation()[i]);
                    WRITEFIELD(sourceCodeRepresentation);
                }
                else {
                    // We don't support yet.
                    ASSERT(0);
                }
            }
            else {
                uint8_t constantType = static_cast<uint8_t>(ConstantType::NonCellValue);
                WRITEFIELD(constantType);
                
                int64_t encodedJSValue = static_cast<int64_t>(JSValue::encode(constant.get()));
                WRITEFIELD(encodedJSValue);

                int32_t sourceCodeRepresentation = static_cast<int32_t>(constantsSourceCodeRepresentation()[i]);
                WRITEFIELD(sourceCodeRepresentation);
            }

            ++i;
        }
    }
}

void UnlinkedCodeBlock::loadConstants(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_CONSTANTS);

    size_t numConstantRegisters;
    READFIELD(numConstantRegisters);
    
    for(size_t i=0; i<numConstantRegisters; i++) {
        uint8_t constTypeVal;
        READFIELD(constTypeVal);
        
        m_constantRegisters.append(WriteBarrier<Unknown>());
        
        switch (static_cast<ConstantType>(constTypeVal)) {

            case ConstantType::NonCellValue:
            {
                uint64_t constant;
                uint32_t constRepresentation;

                READFIELD(constant);
                READFIELD(constRepresentation);

                m_constantRegisters.last().set(vm, this, JSValue::decode(static_cast<EncodedJSValue>(constant)));  
                m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));
            }
            break;

            case ConstantType::Empty:
            {
                uint32_t constRepresentation;
                READFIELD(constRepresentation);

                m_constantRegisters.last().set(vm, this, JSValue());  
                m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));

            }
            break;

            case ConstantType::String:
            {
                bool is8bit;
                READFIELD(is8bit);

                unsigned stringLength;
                READFIELD(stringLength);

                if(is8bit) {
                    RefCountedArray<unsigned char> data(stringLength);
                    vm.byteCodeProvider().readBytes(reinterpret_cast<char*>(data.begin()), stringLength);

                    JSString* jsString = jsOwnedString(&vm, Identifier::fromString(&vm, reinterpret_cast<const LChar*>(data.data()), stringLength).string());
                    m_constantRegisters.last().set(vm, this, JSValue(jsString));  
                } else {
                    RefCountedArray<unsigned char> data(2 * stringLength);
                    vm.byteCodeProvider().readBytes(reinterpret_cast<char*>(data.begin()), 2 * stringLength);

                    JSString* jsString = jsOwnedString(&vm, Identifier::fromString(&vm, reinterpret_cast<const UChar*>(data.data()), stringLength).string());
                    m_constantRegisters.last().set(vm, this, JSValue(jsString));  
                }

                uint32_t constRepresentation;
                READFIELD(constRepresentation);

                m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));
            }
            break;
            
            case ConstantType::SymbolTable:
            {
                SymbolTable* functionSymbolTable = SymbolTable::create(vm);
                functionSymbolTable->load(vm, this->identifiers());

                JSValue jsValue(functionSymbolTable);
                m_constantRegisters.last().set(vm, this, jsValue);

                uint32_t constRepresentation;
                READFIELD(constRepresentation);
                m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));
            }
            break;

            default:
                ASSERT(0);
        }
    }
}

void UnlinkedCodeBlock::saveConstantIdentifierSets(VM& vm) {
    
    WRITEMAGIC(MAGIC_CODEBLOCK_CONSTANTIDENTIFIERSETS);

    size_t numConstantIdentiferSets = m_constantIdentifierSets.size();
    WRITEFIELD(numConstantIdentiferSets);

   // for (auto &constIdSetEntry : m_constantIdentifierSets){
     //   unsigned val = constIdSetEntry.second;
       // WRITEFIELD(val);

     //   IdentifierSet idSet = constIdSetEntry.first;
       // for (auto &identifier : idSet){
            //unsigned length = identifier->length();
            //WRITEFIELD(length);
            //vm.byteCodeProvider().outStream.write(reinterpret_cast<const char* >(constStr.characters8()), length);
    //    }
   // }


}

void UnlinkedCodeBlock::loadConstantIdentifierSets(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_CONSTANTIDENTIFIERSETS);

    size_t numConstantIdentiferSets;
    READFIELD(numConstantIdentiferSets);

    // We haven't seen a use case yet.
    ASSERT(numConstantIdentiferSets == 0);
}

void UnlinkedCodeBlock::saveLinkTimeConstants(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_LINKTIMECONSTANTS);

    size_t numLinktimeConstants = m_linkTimeConstants.size();
    WRITEFIELD(numLinktimeConstants);

    for (unsigned linkTimeConstant : m_linkTimeConstants) {
        WRITEFIELD(linkTimeConstant);
    }
}

void UnlinkedCodeBlock::loadLinkTimeConstants(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_LINKTIMECONSTANTS);

    size_t numLinktimeConstants;
    READFIELD(numLinktimeConstants);
    for(size_t i=0; i<numLinktimeConstants; i++) {
        unsigned linkTimeConstant;
        READFIELD(linkTimeConstant);
        m_linkTimeConstants[i] = linkTimeConstant;
    }
}

void UnlinkedCodeBlock::saveProfileCounts(VM& vm) {

    WRITEMAGIC(MAGIC_CODEBLOCK_PROFILECOUNTS);

    WRITEFIELD(m_arrayProfileCount);
    WRITEFIELD(m_arrayAllocationProfileCount);
    WRITEFIELD(m_objectAllocationProfileCount);
    WRITEFIELD(m_valueProfileCount);
    WRITEFIELD(m_llintCallLinkInfoCount);

}

void UnlinkedCodeBlock::loadProfileCounts(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_PROFILECOUNTS);

    READFIELD(m_arrayProfileCount);
    READFIELD(m_arrayAllocationProfileCount);
    READFIELD(m_objectAllocationProfileCount);
    READFIELD(m_valueProfileCount);
    READFIELD(m_llintCallLinkInfoCount);

}

void UnlinkedCodeBlock::loadMisc(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_MISC);

    READFIELD(m_numVars);
    READFIELD(m_numCapturedVars);
    READFIELD(m_numCalleeLocals);

}

void UnlinkedCodeBlock::saveMisc(VM& vm) {
    
    WRITEMAGIC(MAGIC_CODEBLOCK_MISC);

    WRITEFIELD(m_numVars);
    WRITEFIELD(m_numCapturedVars);
    WRITEFIELD(m_numCalleeLocals);
}


void UnlinkedCodeBlock::loadFunctionDecls(VM& vm) {

    VERIFYMAGIC(MAGIC_CODEBLOCK_FUNCTIONDECLS);

    size_t numFuncDecls;
    READFIELD(numFuncDecls);

    for(size_t i=0; i<numFuncDecls; i++) {
        m_functionDecls.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_functionDecls.last().set(vm, this, UnlinkedFunctionExecutable::create(&vm));
    }
    
}

void UnlinkedCodeBlock::saveFunctionDecls(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_FUNCTIONDECLS);

    size_t numFunctionDecls = m_functionDecls.size();
    WRITEFIELD(numFunctionDecls);

    for(WriteBarrier<UnlinkedFunctionExecutable> f : m_functionDecls) {
        UnlinkedFunctionExecutable* ufexe = f.get();
        ufexe->saveNonCode(vm);
    }
}

void UnlinkedCodeBlock::loadFunctionExprs(VM& vm) {

    VERIFYMAGIC(MAGIC_CODEBLOCK_FUNCTIONEXPRS);

    size_t numFuncExprs;
    READFIELD(numFuncExprs);

    for(size_t i=0; i<numFuncExprs; i++) {
        m_functionExprs.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_functionExprs.last().set(vm, this, UnlinkedFunctionExecutable::create(&vm));
    }    
}

void UnlinkedCodeBlock::saveFunctionExprs(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_FUNCTIONEXPRS);

    size_t numFunctionExprs = m_functionExprs.size();
    WRITEFIELD(numFunctionExprs);

    for(WriteBarrier<UnlinkedFunctionExecutable> f : m_functionExprs) {
        UnlinkedFunctionExecutable* ufexe = f.get();
        ufexe->saveNonCode(vm);
    }
}

void UnlinkedCodeBlock::loadSwitchJumpTables(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_SWITCHJUMPTABLES);

    size_t numSwitchTables;
    READFIELD(numSwitchTables);

    if(numSwitchTables > 0) {
        createRareDataIfNecessary();
    }

    for(size_t i=0; i<numSwitchTables; i++) {
        m_rareData->m_switchJumpTables.append(UnlinkedSimpleJumpTable());
        
        size_t numBranchOffsets;
        READFIELD(numBranchOffsets);
        
        for(size_t i=0; i< numBranchOffsets; i++) {
            int32_t branchOffset;
            READFIELD(branchOffset);
            m_rareData->m_switchJumpTables.last().branchOffsets.append(branchOffset);
        }

        int32_t switchTableMin;
        READFIELD(switchTableMin);
        m_rareData->m_switchJumpTables.last().min = switchTableMin;
    }

}

void UnlinkedCodeBlock::saveSwitchJumpTables(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_SWITCHJUMPTABLES);

    size_t numSwitchJumpTables = numberOfSwitchJumpTables();
    WRITEFIELD(numSwitchJumpTables);

    for(size_t i=0; i<numberOfSwitchJumpTables(); i++) {
        size_t numbranchOffsets = switchJumpTable(i).branchOffsets.size();
        WRITEFIELD(numbranchOffsets);

        for (int32_t branchOffset : switchJumpTable(i).branchOffsets) {
            WRITEFIELD(branchOffset);
        }

        int32_t switchTableMin = switchJumpTable(i).min;
        WRITEFIELD(switchTableMin);
    }
    
}

void UnlinkedCodeBlock::loadStringSwitchJumpTables(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_STRINGSWITCHJUMPTABLES);

    size_t numStringSwitchTables;
    READFIELD(numStringSwitchTables);

	 if (numStringSwitchTables > 0) {
		 createRareDataIfNecessary();
	 }

    for(size_t i=0; i<numStringSwitchTables; i++) {
        m_rareData->m_stringSwitchJumpTables.append(UnlinkedStringJumpTable());
        m_rareData->m_stringSwitchJumpTables.last().load(vm);
    }
}

void UnlinkedCodeBlock::saveStringSwitchJumpTables(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_STRINGSWITCHJUMPTABLES);

    size_t numStringSwitchTables = numberOfStringSwitchJumpTables();
    WRITEFIELD(numStringSwitchTables);
    
    for(size_t i=0; i<numberOfStringSwitchJumpTables(); i++) {
        stringSwitchJumpTable(i).save(vm);
    }

    
}

void UnlinkedCodeBlock::loadExceptionHandlers(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_HANDLERS);    

    size_t numberOfExceptionHandlers;
    READFIELD(numberOfExceptionHandlers);

	 if (numberOfExceptionHandlers > 0) {
		 createRareDataIfNecessary();
	 }

    for(size_t i=0; i<numberOfExceptionHandlers; i++) {

        uint32_t start;
        uint32_t end;
        uint32_t target;
        uint8_t handlerTypeVal;

        READFIELD(start);
        READFIELD(end);
        READFIELD(target);
        READFIELD(handlerTypeVal);

        m_rareData->m_exceptionHandlers.append(UnlinkedHandlerInfo(start, end, target, static_cast<HandlerType>(handlerTypeVal)));
    }
}

void UnlinkedCodeBlock::saveExceptionHandlers(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_HANDLERS);

    size_t numHandlers = numberOfExceptionHandlers();
    WRITEFIELD(numHandlers);
    
    for(size_t i=0; i<numberOfExceptionHandlers(); i++) {
        UnlinkedHandlerInfo& unlinkedHandlerInfo(exceptionHandler(i));
        
        uint32_t start = unlinkedHandlerInfo.start;
        uint32_t end = unlinkedHandlerInfo.end;
        uint32_t target = unlinkedHandlerInfo.target;
        uint8_t typeBits = unlinkedHandlerInfo.typeBits;

        WRITEFIELD(start);
        WRITEFIELD(end);
        WRITEFIELD(target);
        WRITEFIELD(typeBits);
    }
   
}

void UnlinkedCodeBlock::loadRegexps(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_REGEXP);    

    size_t numbRegexps;
    READFIELD(numbRegexps);

    if (numbRegexps > 0) {
        createRareDataIfNecessary();
    }

    for(size_t i=0; i<numbRegexps; i++) {

        unsigned regexplength;
        READFIELD(regexplength);

        bool is8bit;
        READFIELD(is8bit);

        WTF::String pattern;
        if(is8bit){
            RefCountedArray<unsigned char> data(regexplength);
            vm.byteCodeProvider().readBytes(reinterpret_cast<char*>(data.begin()), regexplength);
            pattern = WTF::String(reinterpret_cast<const LChar*>(data.data()), regexplength);
        } else {
            RefCountedArray<unsigned char> data(2 * regexplength);
            vm.byteCodeProvider().readBytes(reinterpret_cast<char*>(data.begin()), 2*regexplength);
            pattern = WTF::String(reinterpret_cast<const UChar*>(data.data()), regexplength);
        }

        /*RegExpFlags*/ uint8_t flagsVal;
        /*RegExpState*/ uint8_t stateVal;
        
        READFIELD(flagsVal);
        READFIELD(stateVal);

        m_rareData->m_regexps.append(WriteBarrier<RegExp>(vm, this, 
            RegExp::create(vm, pattern, static_cast<RegExpFlags>(flagsVal))));
        
        // TODO :: SHould we reset the state 
    }
}

void UnlinkedCodeBlock::saveRegexps(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_REGEXP);

    size_t numRegexes = numberOfRegExps();
    WRITEFIELD(numRegexes);

    for(size_t i=0; i<numberOfRegExps(); i++) {
        RegExp* reg = regexp(i);
        reg->save(vm);
    }
}

void UnlinkedCodeBlock::loadConstantBuffers(VM& vm) {
        
    VERIFYMAGIC(MAGIC_CODEBLOCK_CONSTANTBUFFERS);    

    size_t numConstantBuffers;
    READFIELD(numConstantBuffers);

    if (numConstantBuffers > 0) {
        createRareDataIfNecessary();
    }

    for(size_t i=0; i<numConstantBuffers; i++) {
        size_t numConstants;
        READFIELD(numConstants);
        m_rareData->m_constantBuffers.append(Vector<JSValue>());
    
        for(size_t i=0; i<numConstants; i++) {
            uint8_t constTypeVal;
            READFIELD(constTypeVal);
            ConstantType constantType = static_cast<ConstantType>(constTypeVal);
    
            switch (constantType) {
    
                case ConstantType::NonCellValue:
                {
                    int64_t constant;
                    READFIELD(constant);
                    
                    m_rareData->m_constantBuffers.last().append(JSValue::decode(static_cast<EncodedJSValue>(constant)));  
                }
                    break;

                    case ConstantType::Empty:
                {
                    m_rareData->m_constantBuffers.last().append(JSValue()); 

                }
                    break;

                case ConstantType::String:
                {
                    int constRegisterIndex;
                    READFIELD(constRegisterIndex);

                    m_rareData->m_constantBuffers.last().append(m_constantRegisters[constRegisterIndex].get());
                }
                    break;

                default:
                    ASSERT(0);
                    //throw "Unknown constant type.";
            }
        }
    }
}

void UnlinkedCodeBlock::saveConstantBuffers(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_CONSTANTBUFFERS);

    size_t _constantBufferCount = hasRareData() ? constantBufferCount() : 0;
    WRITEFIELD(_constantBufferCount);

    for(size_t i=0; i < _constantBufferCount; i++) {
        ConstantBuffer& buffer = constantBuffer(i);
        size_t constBufferSize = buffer.size();

        WRITEFIELD(constBufferSize);
        
        for(JSValue value : buffer) {
            if(value.isCell()) {
                
                // Copied from constant regs.
                if(value.isEmpty()) {
                    uint8_t constantType = static_cast<uint8_t>(ConstantType::Empty);
                    WRITEFIELD(constantType);
                }
                else if(value.isString()) {

                    // Store index into constant regs.

                    JSString* bufferVal = JSC::asString(value);
                    const String& bufferStringVal = bufferVal->tryGetValue();

                    int constantIndex = 0;
                    for(auto constant : m_constantRegisters) {
                        JSValue jsValue = constant.get();
                    
                        if(!jsValue.isEmpty() && jsValue.isString()) {

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

                    uint8_t constantType = static_cast<uint8_t>(ConstantType::String);
                    WRITEFIELD(constantType);
                    WRITEFIELD(constantIndex);
                }
                else {
                    ASSERT(0);
                    //throw "We don't support this constant type";
                }
            }
            else {
                uint8_t constantType = static_cast<uint8_t>(ConstantType::NonCellValue);
                int64_t valueEnc = static_cast<int64_t>(JSValue::encode(value));

                        WRITEFIELD(constantType);
                        WRITEFIELD(valueEnc);
            }
        }
    }
}

void UnlinkedCodeBlock::loadPropertyAccessInstructions(VM& vm) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_PROPACCESSINSTRUCTIONS);    

    size_t numberOfPropertyAccessInstructions;
    READFIELD(numberOfPropertyAccessInstructions);

    for(size_t i=0; i<numberOfPropertyAccessInstructions; i++) {
        unsigned instr;
        READFIELD(instr);
        this->addPropertyAccessInstruction(instr);
    }
}

void UnlinkedCodeBlock::savePropertyAccessInstructions(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_PROPACCESSINSTRUCTIONS);

    size_t numberOfPropertyAccessInstructions = this->numberOfPropertyAccessInstructions();
    WRITEFIELD(numberOfPropertyAccessInstructions);

    for (unsigned instr : this->propertyAccessInstructions()) {
      WRITEFIELD(instr);
    }
}

void UnlinkedCodeBlock::loadJumpTargets(VM& vm) {
    VERIFYMAGIC(MAGIC_CODEBLOCK_JUMPTARGETS);    

    size_t numberOfJumpTargets;
    READFIELD(numberOfJumpTargets);

    for(size_t i=0; i<numberOfJumpTargets; i++) {
        unsigned jmpTarget;
        READFIELD(jmpTarget);
        this->addJumpTarget(jmpTarget);
    }
}

void UnlinkedCodeBlock::saveJumpTargets(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_JUMPTARGETS);

    size_t numJumpTargets = m_jumpTargets.size();
    WRITEFIELD(numJumpTargets);

    for (auto& jmptarget : m_jumpTargets) {
      WRITEFIELD(jmptarget);
    }
}

void UnlinkedCodeBlock::loadExpressionRangeInfos(VM& vm) {
    VERIFYMAGIC(MAGIC_CODEBLOCK_EXPRESSIONRANGEINFOS);    
    
    size_t expressionInfoSize;
    READFIELD(expressionInfoSize);
    
    for (size_t i = 0; i < expressionInfoSize; i++) {
        uint32_t instructionOffset, divotPoint, startOffset, endOffset;
        unsigned line, column;

        READFIELD(instructionOffset);
        READFIELD(divotPoint);
        READFIELD(startOffset);
        READFIELD(endOffset);
        READFIELD(line);
        READFIELD(column);

        this->addExpressionInfo(instructionOffset, divotPoint, startOffset,
            endOffset, line, column);
    }
}

void UnlinkedCodeBlock::saveExpressionRangeInfos(VM& vm) {
    WRITEMAGIC(MAGIC_CODEBLOCK_EXPRESSIONRANGEINFOS);

    Vector<ExpressionRangeInfo>& expressionInfo = m_expressionInfo;
    size_t expressionInfoSize = m_expressionInfo.size();
    WRITEFIELD(expressionInfoSize);
    
    for (size_t i = 0; i < expressionInfoSize; i++) {
        ExpressionRangeInfo& info = expressionInfo[i];
        unsigned line;
        unsigned column;
        getLineAndColumn(info, line, column);

        // TODO :: needs to compress these .. 
        uint32_t instructionOffset = info.instructionOffset;
        uint32_t startOffset = info.startOffset;
        uint32_t divotPoint = info.divotPoint;
        uint32_t endOffset = info.endOffset;
        
        WRITEFIELD(instructionOffset);
        WRITEFIELD(divotPoint);
        WRITEFIELD(startOffset);
        WRITEFIELD(endOffset);
        
        WRITEFIELD(line);
        WRITEFIELD(column);
    }
}

void UnlinkedStringJumpTable::save(VM& vm) {
    size_t offsetTableSize = offsetTable.size();
    WRITEFIELD(offsetTableSize);

    for( auto& entry : offsetTable) {
        RefPtr<StringImpl> key = entry.key;
        int32_t value = entry.value.branchOffset;
        
        unsigned labelLength = key->length();
        ASSERT(labelLength > 0);

        WRITEFIELD(labelLength);
        if(labelLength > 0)
            vm.byteCodeProvider().outStream.write(reinterpret_cast<const char* >(key->characters8()), labelLength);
        
        WRITEFIELD(value);
    }
}

void UnlinkedStringJumpTable::load(VM& vm) {
    int32_t offsetTableSize;
    READFIELD(offsetTableSize);

    for(int i=0; i < offsetTableSize; i++) {
        
        int32_t value;

        unsigned keyLength;
        READFIELD(keyLength);

        RefCountedArray<unsigned char> data(keyLength);
        vm.byteCodeProvider().readBytes(reinterpret_cast<char*>(data.begin()), keyLength);
        RefPtr<AtomicStringImpl> atomicStringKey = AtomicStringImpl::add(data.data(), keyLength);

        READFIELD(value);

        struct OffsetLocation location;
        location.branchOffset = value;

        offsetTable.add(atomicStringKey, location);
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
