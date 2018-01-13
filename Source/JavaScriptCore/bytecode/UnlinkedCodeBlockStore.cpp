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

#include "UnlinkedCodeBlockStore.h"
#include "UnlinkedFunctionExecutableStore.h"

namespace JSC {

Ref<UnlinkedCodeBlockStore> UnlinkedCodeBlockStore::create(UnlinkedCodeBlock& UnlinkedCodeBlock) {
    return WTF::adoptRef(*new UnlinkedCodeBlockStore(UnlinkedCodeBlock));
}

void UnlinkedCodeBlockStore::load(VM& vm, ByteCodeReadStore& byteCodeCache) {

    VERIFYMAGIC(MAGIC_CODEBLOCK);
    READFIELD(m_unlinkedCodeBlock.m_numParameters);
    
    loadInstructions(vm, byteCodeCache);
    loadVirtualRegisters(vm, byteCodeCache);
    loadIdentifiers(vm, byteCodeCache);
    loadBitVectors(vm, byteCodeCache);
    loadConstants(vm, byteCodeCache);
    loadConstantIdentifierSets(vm, byteCodeCache);
    loadLinkTimeConstants(vm, byteCodeCache);
    loadProfileCounts(vm, byteCodeCache);
    loadMisc(vm, byteCodeCache);
    loadFunctionDecls(vm, byteCodeCache);
    loadFunctionExprs(vm, byteCodeCache);
    loadSwitchJumpTables(vm, byteCodeCache);
    loadStringSwitchJumpTables(vm, byteCodeCache);
    loadExceptionHandlers(vm, byteCodeCache);
    loadRegexps(vm, byteCodeCache);
    loadConstantBuffers(vm, byteCodeCache);
    loadPropertyAccessInstructions(vm, byteCodeCache);
    loadJumpTargets(vm, byteCodeCache);
    loadExpressionRangeInfos(vm, byteCodeCache);   
    loadExpressionInfoFatPositions(vm, byteCodeCache);

    VERIFYMAGIC(MAGIC_CODEBLOCK_END);
}

void UnlinkedCodeBlockStore::save(VM& vm, ByteCodeWriteStore& byteCodeCache) {

    WRITEMAGIC(MAGIC_CODEBLOCK);
    WRITEFIELD(m_unlinkedCodeBlock.m_numParameters);
    
    // instructions

    saveInstructions(vm, byteCodeCache);
    saveVirtualRegisters(vm, byteCodeCache);
    saveIdentifiers(vm, byteCodeCache);
    saveBitVectors(vm, byteCodeCache);
    saveConstants(vm, byteCodeCache);
    saveConstantIdentifierSets(vm, byteCodeCache);
    saveLinkTimeConstants(vm, byteCodeCache);
    saveProfileCounts(vm, byteCodeCache);
    saveMisc(vm, byteCodeCache);
    saveFunctionDecls(vm, byteCodeCache);
    saveFunctionExprs(vm, byteCodeCache);
    saveSwitchJumpTables(vm, byteCodeCache);
    saveStringSwitchJumpTables(vm, byteCodeCache);
    saveExceptionHandlers(vm, byteCodeCache);
    saveRegexps(vm, byteCodeCache);
    saveConstantBuffers(vm, byteCodeCache);
    savePropertyAccessInstructions(vm, byteCodeCache);
    saveJumpTargets(vm, byteCodeCache);
    saveExpressionRangeInfos(vm, byteCodeCache);
    saveExpressionInfoFatPositions(vm, byteCodeCache);

    WRITEMAGIC(MAGIC_CODEBLOCK_END);
}    

void UnlinkedCodeBlockStore::saveInstructions(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_INSTRUCTIONS);
    
    unsigned instructionCount = m_unlinkedCodeBlock.m_unlinkedInstructions->m_instructionCount;
    WRITEFIELD(instructionCount);
    
    size_t instructionStreamSize = m_unlinkedCodeBlock.m_unlinkedInstructions->m_data.size();
    WRITEFIELD(instructionStreamSize);

    WRITEVECTOR8(m_unlinkedCodeBlock.m_unlinkedInstructions->m_data.data(), instructionStreamSize);
}

void UnlinkedCodeBlockStore::loadInstructions(VM&, ByteCodeReadStore& byteCodeCache) {
    VERIFYMAGIC(MAGIC_CODEBLOCK_INSTRUCTIONS);

    unsigned instructioncount;
    READFIELD(instructioncount);
    
    size_t instructionStreamSize;
    READFIELD(instructionStreamSize);

    RefCountedArray<unsigned char> instrArray(instructionStreamSize);
    READVECTOR8_NOALLOC(instrArray.data(), instructionStreamSize);

    m_unlinkedCodeBlock.m_unlinkedInstructions = std::make_unique<UnlinkedInstructionStream>(instrArray, instructioncount);
}

void UnlinkedCodeBlockStore::saveVirtualRegisters(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_VIRTUALREGISTERS);
    
    int registerOffset = m_unlinkedCodeBlock.m_thisRegister.offset();
    WRITEFIELD(registerOffset);

    registerOffset = m_unlinkedCodeBlock.m_scopeRegister.offset();
    WRITEFIELD(registerOffset);

    registerOffset = m_unlinkedCodeBlock.m_globalObjectRegister.offset();
    WRITEFIELD(registerOffset);

}

void UnlinkedCodeBlockStore::loadVirtualRegisters(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_VIRTUALREGISTERS);
    
    int regoffset;
    READFIELD(regoffset);
    m_unlinkedCodeBlock.m_thisRegister=VirtualRegister(regoffset);
    READFIELD(regoffset);
    m_unlinkedCodeBlock.m_scopeRegister=VirtualRegister(regoffset);
    READFIELD(regoffset);
    m_unlinkedCodeBlock.m_globalObjectRegister=VirtualRegister(regoffset);
}

void UnlinkedCodeBlockStore::saveIdentifiers(VM& vm, ByteCodeWriteStore& byteCodeCache) {

    WRITEMAGIC(MAGIC_CODEBLOCK_IDENTIFIERS);
    
    size_t numIdentifiers = m_unlinkedCodeBlock.m_identifiers.size();
    WRITEFIELD(numIdentifiers);
    
    uint8_t identifierType;
    for (auto &identifier : m_unlinkedCodeBlock.m_identifiers) {
        if (identifier.isPrivateName()) {
            size_t privateNameIndex;
            if(vm.propertyNames->builtinNames().findPrivateNameIndex(identifier, privateNameIndex)) {
                identifierType = static_cast<uint8_t>(IdentifierType::BuiltinPrivateName);
                WRITEFIELD(identifierType);
                WRITEFIELD(privateNameIndex);
            } else {
                identifierType = static_cast<uint8_t>(IdentifierType::PrivateName);
                WRITEFIELD(identifierType);

                ASSERT(identifier.string().length() == 0); // others are not yet implemented..
            }
        }
        else if(identifier.isSymbol()) {
            size_t symbolIndex;
            if (vm.propertyNames->findCommonSymbol(identifier, symbolIndex)) {
                identifierType = static_cast<uint8_t>(IdentifierType::WellKnownSymbol);
                WRITEFIELD(identifierType);
                WRITEFIELD(symbolIndex);
            }
            else {
                identifierType = static_cast<uint8_t>(IdentifierType::Symbol);
                WRITEFIELD(identifierType);

                ASSERT(identifier.string().length() == 0); // other symbols are not yet implemented..
            }
        } else {
            size_t commonIdIndex;
            if(vm.propertyNames->findCommonPropName(identifier, commonIdIndex)) {
                identifierType = static_cast<uint8_t>(IdentifierType::CommonIdentifier);
                WRITEFIELD(identifierType);
                WRITEFIELD(commonIdIndex);
            } else {
                identifierType = static_cast<uint8_t>(IdentifierType::Normal);
                WRITEFIELD(identifierType);      

                
                //int identifierLength = identifier.length();
                //WRITEFIELD(identifierLength);

                WRITEATOMICIDENTIFIER(identifier);
            }
        }
    }   
}

void UnlinkedCodeBlockStore::loadIdentifiers(VM& vm, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_IDENTIFIERS);

    size_t numIdentifiers;
    uint8_t identifierType;
    size_t identifierIndex;
    READFIELD(numIdentifiers);
    for(size_t i=0; i<numIdentifiers; i++) {
        READFIELD(identifierType);
        switch(static_cast<IdentifierType>(identifierType)) {
            case IdentifierType::CommonIdentifier:{
                READFIELD(identifierIndex);
                m_unlinkedCodeBlock.m_identifiers.append(vm.propertyNames->lookupCommonPropNameIdenfier(identifierIndex));
            }
            break;

            case IdentifierType::WellKnownSymbol: {
                READFIELD(identifierIndex);
                m_unlinkedCodeBlock.m_identifiers.append(vm.propertyNames->lookupCommonSymbolIdenfier(identifierIndex));
            }
            break;

            case IdentifierType::Symbol: {
                //ASSERT(0);
                //int idlength;
                //READFIELD(idlength);
                //ASSERT(idlength == 0);

                m_unlinkedCodeBlock.m_identifiers.append(Identifier::fromUid(PrivateName()));
            }
            break; 

            case IdentifierType::PrivateName: {
                //int idlength;
                ///READFIELD(idlength);
                //ASSERT(idlength == 0 );

                m_unlinkedCodeBlock.m_identifiers.append(Identifier::fromUid(PrivateName()));
            }
            break;

            case IdentifierType::BuiltinPrivateName: {
                READFIELD(identifierIndex);
                ASSERT(identifierIndex >= 0);

                m_unlinkedCodeBlock.m_identifiers.append(vm.propertyNames->builtinNames().lookupPrivateNameIdentifier(identifierIndex));
            }
            break;

            case IdentifierType::Normal: {
                //int idlength;
                //READFIELD(idlength);
                        
                Identifier id;
                READATOMICIDENTIFIER(id);
                m_unlinkedCodeBlock.m_identifiers.append(id);
            }
            break;

            default:
                ASSERT(0);
        }
    }
}

void UnlinkedCodeBlockStore::saveBitVectors(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_BITVECTORS);
    
    size_t bitVectorSize = m_unlinkedCodeBlock.m_bitVectors.size();
    WRITEFIELD(bitVectorSize);
    for (auto &bitVector : m_unlinkedCodeBlock.m_bitVectors){
        uintptr_t bits = *bitVector.bits();
        WRITEFIELD(bits);
    }
}

void UnlinkedCodeBlockStore::loadBitVectors(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_BITVECTORS);

    size_t numBitVectors;
    READFIELD(numBitVectors);
    for(size_t i=0; i<numBitVectors; i++) {
        uintptr_t bits;
        READFIELD(bits);

        BitVector bitVector;
        *(bitVector.bits()) = bits;
        m_unlinkedCodeBlock.m_bitVectors.append(bitVector);
    }
}

void UnlinkedCodeBlockStore::saveConstants(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_CONSTANTS);
    
    size_t constantRegistersSize = m_unlinkedCodeBlock.constantRegisters().size();
    WRITEFIELD(constantRegistersSize);
    
    if (!m_unlinkedCodeBlock.constantRegisters().isEmpty()) {
        size_t i = 0;
        for (const auto& constant : m_unlinkedCodeBlock.constantRegisters()) {
            
            JSValue value = constant.get();
            if(value.isCell()) {
                if(value.isEmpty()) {
                    uint8_t constantType = static_cast<uint8_t>(ConstantType::Empty);
                    WRITEFIELD(constantType);

                    int32_t sourceCodeRepresentation = static_cast<int32_t>(m_unlinkedCodeBlock.constantsSourceCodeRepresentation()[i]);
                    WRITEFIELD(sourceCodeRepresentation);
                }
                else if(value.isString()) {
                    uint8_t constantType = static_cast<uint8_t>(ConstantType::String);
                    WRITEFIELD(constantType);

                    const String& constStr = static_cast<const JSString*>(value.asCell())->tryGetValue();
                    WRITESTRING(constStr.impl());

                    int32_t sourceCodeRepresentation = static_cast<int32_t>(m_unlinkedCodeBlock.constantsSourceCodeRepresentation()[i]);
                    WRITEFIELD(sourceCodeRepresentation);
                }
                else if (value.asCell()->classInfo(vm) == SymbolTable::info()) {
                    uint8_t constantType = static_cast<uint8_t>(ConstantType::SymbolTable);
                    WRITEFIELD(constantType);

                    SymbolTable* symbolTable = jsCast<SymbolTable*>(value);
                    symbolTable->save(vm, m_unlinkedCodeBlock.identifiers(), byteCodeCache);

                    int32_t sourceCodeRepresentation = static_cast<int32_t>(m_unlinkedCodeBlock.constantsSourceCodeRepresentation()[i]);
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

                int32_t sourceCodeRepresentation = static_cast<int32_t>(m_unlinkedCodeBlock.constantsSourceCodeRepresentation()[i]);
                WRITEFIELD(sourceCodeRepresentation);
            }

            ++i;
        }
    }
}

void UnlinkedCodeBlockStore::loadConstants(VM& vm, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_CONSTANTS);

    size_t numConstantRegisters;
    READFIELD(numConstantRegisters);
    
    for(size_t i=0; i<numConstantRegisters; i++) {
        uint8_t constTypeVal;
        READFIELD(constTypeVal);
        
        m_unlinkedCodeBlock.m_constantRegisters.append(WriteBarrier<Unknown>());
        
        switch (static_cast<ConstantType>(constTypeVal)) {

            case ConstantType::NonCellValue:
            {
                uint64_t constant;
                uint32_t constRepresentation;

                READFIELD(constant);
                READFIELD(constRepresentation);

                m_unlinkedCodeBlock.m_constantRegisters.last().set(vm, &m_unlinkedCodeBlock, JSValue::decode(static_cast<EncodedJSValue>(constant)));  
                m_unlinkedCodeBlock.m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));
            }
            break;

            case ConstantType::Empty:
            {
                uint32_t constRepresentation;
                READFIELD(constRepresentation);

                m_unlinkedCodeBlock.m_constantRegisters.last().set(vm, &m_unlinkedCodeBlock, JSValue());  
                m_unlinkedCodeBlock.m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));

            }
            break;

            case ConstantType::String:
            {
                RefPtr<StringImpl> constStringImpl = nullptr;
                READSTRING(constStringImpl);
    
                if(constStringImpl) {
                    m_unlinkedCodeBlock.m_constantRegisters.last().set(vm, &m_unlinkedCodeBlock, JSValue(jsString(&vm, String(constStringImpl.get()))));  
                } else {
                    m_unlinkedCodeBlock.m_constantRegisters.last().set(vm, &m_unlinkedCodeBlock, JSValue(jsString(&vm, String())));  
                }

                uint32_t constRepresentation;
                READFIELD(constRepresentation);

                m_unlinkedCodeBlock.m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));
            }
            break;
            
            case ConstantType::SymbolTable:
            {
                SymbolTable* functionSymbolTable = SymbolTable::create(vm);
                functionSymbolTable->load(vm, m_unlinkedCodeBlock.identifiers(), byteCodeCache);

                JSValue jsValue(functionSymbolTable);
                m_unlinkedCodeBlock.m_constantRegisters.last().set(vm, &m_unlinkedCodeBlock, jsValue);

                uint32_t constRepresentation;
                READFIELD(constRepresentation);
                m_unlinkedCodeBlock.m_constantsSourceCodeRepresentation.append(static_cast<SourceCodeRepresentation>(constRepresentation));
            }
            break;

            default:
                ASSERT(0);
        }
    }
}

void UnlinkedCodeBlockStore::saveConstantIdentifierSets(VM&, ByteCodeWriteStore& byteCodeCache) {
    
    WRITEMAGIC(MAGIC_CODEBLOCK_CONSTANTIDENTIFIERSETS);

    size_t numConstantIdentiferSets = m_unlinkedCodeBlock.m_constantIdentifierSets.size();
    WRITEFIELD(numConstantIdentiferSets);
}

void UnlinkedCodeBlockStore::loadConstantIdentifierSets(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_CONSTANTIDENTIFIERSETS);

    size_t numConstantIdentiferSets;
    READFIELD(numConstantIdentiferSets);

    // We haven't seen a use case yet.
    ASSERT(numConstantIdentiferSets == 0);
}

void UnlinkedCodeBlockStore::saveLinkTimeConstants(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_LINKTIMECONSTANTS);

    size_t numLinktimeConstants = m_unlinkedCodeBlock.m_linkTimeConstants.size();
    WRITEFIELD(numLinktimeConstants);

    for (unsigned linkTimeConstant : m_unlinkedCodeBlock.m_linkTimeConstants) {
        WRITEFIELD(linkTimeConstant);
    }
}

void UnlinkedCodeBlockStore::loadLinkTimeConstants(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_LINKTIMECONSTANTS);

    size_t numLinktimeConstants;
    READFIELD(numLinktimeConstants);
    for(size_t i=0; i<numLinktimeConstants; i++) {
        unsigned linkTimeConstant;
        READFIELD(linkTimeConstant);
        m_unlinkedCodeBlock.m_linkTimeConstants[i] = linkTimeConstant;
    }
}

void UnlinkedCodeBlockStore::saveProfileCounts(VM&, ByteCodeWriteStore& byteCodeCache) {

    WRITEMAGIC(MAGIC_CODEBLOCK_PROFILECOUNTS);

    WRITEFIELD(m_unlinkedCodeBlock.m_arrayProfileCount);
    WRITEFIELD(m_unlinkedCodeBlock.m_arrayAllocationProfileCount);
    WRITEFIELD(m_unlinkedCodeBlock.m_objectAllocationProfileCount);
    WRITEFIELD(m_unlinkedCodeBlock.m_valueProfileCount);
    WRITEFIELD(m_unlinkedCodeBlock.m_llintCallLinkInfoCount);

}

void UnlinkedCodeBlockStore::loadProfileCounts(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_PROFILECOUNTS);

    READFIELD(m_unlinkedCodeBlock.m_arrayProfileCount);
    READFIELD(m_unlinkedCodeBlock.m_arrayAllocationProfileCount);
    READFIELD(m_unlinkedCodeBlock.m_objectAllocationProfileCount);
    READFIELD(m_unlinkedCodeBlock.m_valueProfileCount);
    READFIELD(m_unlinkedCodeBlock.m_llintCallLinkInfoCount);

}

void UnlinkedCodeBlockStore::loadMisc(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_MISC);

    READFIELD(m_unlinkedCodeBlock.m_numVars);
    READFIELD(m_unlinkedCodeBlock.m_numCapturedVars);
    READFIELD(m_unlinkedCodeBlock.m_numCalleeLocals);

}

void UnlinkedCodeBlockStore::saveMisc(VM&, ByteCodeWriteStore& byteCodeCache) {
    
    WRITEMAGIC(MAGIC_CODEBLOCK_MISC);

    WRITEFIELD(m_unlinkedCodeBlock.m_numVars);
    WRITEFIELD(m_unlinkedCodeBlock.m_numCapturedVars);
    WRITEFIELD(m_unlinkedCodeBlock.m_numCalleeLocals);
}


void UnlinkedCodeBlockStore::loadFunctionDecls(VM& vm, ByteCodeReadStore& byteCodeCache) {

    VERIFYMAGIC(MAGIC_CODEBLOCK_FUNCTIONDECLS);

    size_t numFuncDecls;
    READFIELD(numFuncDecls);

    for(size_t i=0; i<numFuncDecls; i++) {
        m_unlinkedCodeBlock.m_functionDecls.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_unlinkedCodeBlock.m_functionDecls.last().set(vm, &m_unlinkedCodeBlock, UnlinkedFunctionExecutable::create(&vm, byteCodeCache));
    }
    
}

void UnlinkedCodeBlockStore::saveFunctionDecls(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_FUNCTIONDECLS);

    size_t numFunctionDecls = m_unlinkedCodeBlock.m_functionDecls.size();
    WRITEFIELD(numFunctionDecls);

    for(WriteBarrier<UnlinkedFunctionExecutable> f : m_unlinkedCodeBlock.m_functionDecls) {
        UnlinkedFunctionExecutable* ufexe = f.get();
        
        Ref<UnlinkedFunctionExecutableStore> unlinkedFunctionExecutableStore = UnlinkedFunctionExecutableStore::create(*ufexe);
        unlinkedFunctionExecutableStore->saveHeader(vm, byteCodeCache);
    }
}

void UnlinkedCodeBlockStore::loadFunctionExprs(VM& vm, ByteCodeReadStore& byteCodeCache) {

    VERIFYMAGIC(MAGIC_CODEBLOCK_FUNCTIONEXPRS);

    size_t numFuncExprs;
    READFIELD(numFuncExprs);

    for(size_t i=0; i<numFuncExprs; i++) {
        m_unlinkedCodeBlock.m_functionExprs.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_unlinkedCodeBlock.m_functionExprs.last().set(vm, &m_unlinkedCodeBlock, UnlinkedFunctionExecutable::create(&vm, byteCodeCache));
    }    
}

void UnlinkedCodeBlockStore::saveFunctionExprs(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_FUNCTIONEXPRS);

    size_t numFunctionExprs = m_unlinkedCodeBlock.m_functionExprs.size();
    WRITEFIELD(numFunctionExprs);

    for(WriteBarrier<UnlinkedFunctionExecutable> f : m_unlinkedCodeBlock.m_functionExprs) {
        UnlinkedFunctionExecutable* ufexe = f.get();
        //ufexe->saveNonCode(vm, byteCodeCache);

        Ref<UnlinkedFunctionExecutableStore> unlinkedFunctionExecutableStore = UnlinkedFunctionExecutableStore::create(*ufexe);
        unlinkedFunctionExecutableStore->saveHeader(vm, byteCodeCache);
    }
}

void UnlinkedCodeBlockStore::loadSwitchJumpTables(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_SWITCHJUMPTABLES);

    size_t numSwitchTables;
    READFIELD(numSwitchTables);

    if(numSwitchTables > 0) {
        m_unlinkedCodeBlock.createRareDataIfNecessary();
    }

    for(size_t i=0; i<numSwitchTables; i++) {
        m_unlinkedCodeBlock.m_rareData->m_switchJumpTables.append(UnlinkedSimpleJumpTable());
        
        size_t numBranchOffsets;
        READFIELD(numBranchOffsets);
        
        for(size_t i=0; i< numBranchOffsets; i++) {
            int32_t branchOffset;
            READFIELD(branchOffset);
            m_unlinkedCodeBlock.m_rareData->m_switchJumpTables.last().branchOffsets.append(branchOffset);
        }

        int32_t switchTableMin;
        READFIELD(switchTableMin);
        m_unlinkedCodeBlock.m_rareData->m_switchJumpTables.last().min = switchTableMin;
    }

}

void UnlinkedCodeBlockStore::saveSwitchJumpTables(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_SWITCHJUMPTABLES);

    size_t numSwitchJumpTables = m_unlinkedCodeBlock.numberOfSwitchJumpTables();
    WRITEFIELD(numSwitchJumpTables);

    for(size_t i=0; i<m_unlinkedCodeBlock.numberOfSwitchJumpTables(); i++) {
        size_t numbranchOffsets = m_unlinkedCodeBlock.switchJumpTable(i).branchOffsets.size();
        WRITEFIELD(numbranchOffsets);

        for (int32_t branchOffset : m_unlinkedCodeBlock.switchJumpTable(i).branchOffsets) {
            WRITEFIELD(branchOffset);
        }

        int32_t switchTableMin = m_unlinkedCodeBlock.switchJumpTable(i).min;
        WRITEFIELD(switchTableMin);
    }
    
}

void UnlinkedCodeBlockStore::loadStringSwitchJumpTables(VM& vm, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_STRINGSWITCHJUMPTABLES);

    size_t numStringSwitchTables;
    READFIELD(numStringSwitchTables);

	 if (numStringSwitchTables > 0) {
		 m_unlinkedCodeBlock.createRareDataIfNecessary();
	 }

    for(size_t i=0; i<numStringSwitchTables; i++) {
        m_unlinkedCodeBlock.m_rareData->m_stringSwitchJumpTables.append(UnlinkedStringJumpTable());
        m_unlinkedCodeBlock.m_rareData->m_stringSwitchJumpTables.last().load(vm, byteCodeCache);
    }
}

void UnlinkedCodeBlockStore::saveStringSwitchJumpTables(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_STRINGSWITCHJUMPTABLES);

    size_t numStringSwitchTables = m_unlinkedCodeBlock.numberOfStringSwitchJumpTables();
    WRITEFIELD(numStringSwitchTables);
    
    for(size_t i=0; i<m_unlinkedCodeBlock.numberOfStringSwitchJumpTables(); i++) {
        m_unlinkedCodeBlock.stringSwitchJumpTable(i).save(vm, byteCodeCache);
    }
}

void UnlinkedCodeBlockStore::loadExceptionHandlers(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_HANDLERS);    

    size_t numberOfExceptionHandlers;
    READFIELD(numberOfExceptionHandlers);

	 if (numberOfExceptionHandlers > 0) {
		 m_unlinkedCodeBlock.createRareDataIfNecessary();
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

        m_unlinkedCodeBlock.m_rareData->m_exceptionHandlers.append(UnlinkedHandlerInfo(start, end, target, static_cast<HandlerType>(handlerTypeVal)));
    }
}

void UnlinkedCodeBlockStore::saveExceptionHandlers(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_HANDLERS);

    size_t numHandlers = m_unlinkedCodeBlock.numberOfExceptionHandlers();
    WRITEFIELD(numHandlers);
    
    for(size_t i=0; i<m_unlinkedCodeBlock.numberOfExceptionHandlers(); i++) {
        UnlinkedHandlerInfo& unlinkedHandlerInfo(m_unlinkedCodeBlock.exceptionHandler(i));
        
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

void UnlinkedCodeBlockStore::loadRegexps(VM& vm, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_REGEXP);    

    size_t numbRegexps;
    READFIELD(numbRegexps);

    if (numbRegexps > 0) {
        m_unlinkedCodeBlock.createRareDataIfNecessary();
    }

    for(size_t i=0; i<numbRegexps; i++) {

        RefPtr<StringImpl> patternStringImpl;
        READSTRING(patternStringImpl);
        ASSERT(patternStringImpl);

        WTF::String pattern(patternStringImpl.get());

        /*RegExpFlags*/ uint8_t flagsVal;
        /*RegExpState*/ uint8_t stateVal;
        
        READFIELD(flagsVal);
        READFIELD(stateVal);

        m_unlinkedCodeBlock.m_rareData->m_regexps.append(WriteBarrier<RegExp>(vm, &m_unlinkedCodeBlock, 
            RegExp::create(vm, pattern, static_cast<RegExpFlags>(flagsVal))));
        
        // TODO :: SHould we reset the state 
    }
}

void UnlinkedCodeBlockStore::saveRegexps(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_REGEXP);

    size_t numRegexes = m_unlinkedCodeBlock.numberOfRegExps();
    WRITEFIELD(numRegexes);

    for(size_t i=0; i<m_unlinkedCodeBlock.numberOfRegExps(); i++) {
        RegExp* reg = m_unlinkedCodeBlock.regexp(i);
        reg->save(vm, byteCodeCache);
    }
}

void UnlinkedCodeBlockStore::loadConstantBuffers(VM&, ByteCodeReadStore& byteCodeCache) {
        
    VERIFYMAGIC(MAGIC_CODEBLOCK_CONSTANTBUFFERS);    

    size_t numConstantBuffers;
    READFIELD(numConstantBuffers);

    if (numConstantBuffers > 0) {
        m_unlinkedCodeBlock.createRareDataIfNecessary();
    }

    for(size_t i=0; i<numConstantBuffers; i++) {
        size_t numConstants;
        READFIELD(numConstants);
        m_unlinkedCodeBlock.m_rareData->m_constantBuffers.append(Vector<JSValue>());
    
        for(size_t i=0; i<numConstants; i++) {
            uint8_t constTypeVal;
            READFIELD(constTypeVal);
            ConstantType constantType = static_cast<ConstantType>(constTypeVal);
    
            switch (constantType) {
    
                case ConstantType::NonCellValue:
                {
                    int64_t constant;
                    READFIELD(constant);
                    
                    m_unlinkedCodeBlock.m_rareData->m_constantBuffers.last().append(JSValue::decode(static_cast<EncodedJSValue>(constant)));  
                }
                break;

                case ConstantType::Empty:
                {
                    m_unlinkedCodeBlock.m_rareData->m_constantBuffers.last().append(JSValue()); 

                }
                break;

                case ConstantType::String:
                {
                    size_t constRegisterIndex;
                    READFIELD(constRegisterIndex);
                    ASSERT(constRegisterIndex >= 0 && constRegisterIndex < m_unlinkedCodeBlock.m_constantRegisters.size());

                    m_unlinkedCodeBlock.m_rareData->m_constantBuffers.last().append(m_unlinkedCodeBlock.m_constantRegisters[constRegisterIndex].get());
                }
                break;

                default:
                    ASSERT(0);
                    //throw "Unknown constant type.";
            }
        }
    }
}

typedef Vector<JSValue> ConstantBuffer;

void UnlinkedCodeBlockStore::saveConstantBuffers(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_CONSTANTBUFFERS);

    size_t _constantBufferCount = m_unlinkedCodeBlock.hasRareData() ? m_unlinkedCodeBlock.constantBufferCount() : 0;
    WRITEFIELD(_constantBufferCount);

    for(size_t i=0; i < _constantBufferCount; i++) {
        ConstantBuffer& buffer = m_unlinkedCodeBlock.constantBuffer(i);
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

                    size_t constantIndex = 0;
                    for(auto constant : m_unlinkedCodeBlock.m_constantRegisters) {
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

                    if(constantIndex >= m_unlinkedCodeBlock.m_constantRegisters.size()) {
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

void UnlinkedCodeBlockStore::loadPropertyAccessInstructions(VM&, ByteCodeReadStore& byteCodeCache) {
    
    VERIFYMAGIC(MAGIC_CODEBLOCK_PROPACCESSINSTRUCTIONS);    

    size_t numberOfPropertyAccessInstructions;
    READFIELD(numberOfPropertyAccessInstructions);

    for(size_t i=0; i<numberOfPropertyAccessInstructions; i++) {
        unsigned instr;
        READFIELD(instr);
        m_unlinkedCodeBlock.addPropertyAccessInstruction(instr);
    }
}

void UnlinkedCodeBlockStore::savePropertyAccessInstructions(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_PROPACCESSINSTRUCTIONS);

    size_t numberOfPropertyAccessInstructions = m_unlinkedCodeBlock.numberOfPropertyAccessInstructions();
    WRITEFIELD(numberOfPropertyAccessInstructions);

    for (unsigned instr : m_unlinkedCodeBlock.propertyAccessInstructions()) {
      WRITEFIELD(instr);
    }
}

void UnlinkedCodeBlockStore::loadJumpTargets(VM&, ByteCodeReadStore& byteCodeCache) {
    VERIFYMAGIC(MAGIC_CODEBLOCK_JUMPTARGETS);    

    size_t numberOfJumpTargets;
    READFIELD(numberOfJumpTargets);

    for(size_t i=0; i<numberOfJumpTargets; i++) {
        unsigned jmpTarget;
        READFIELD(jmpTarget);
        m_unlinkedCodeBlock.addJumpTarget(jmpTarget);
    }
}

void UnlinkedCodeBlockStore::saveJumpTargets(VM&, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_JUMPTARGETS);

    size_t numJumpTargets = m_unlinkedCodeBlock.m_jumpTargets.size();
    WRITEFIELD(numJumpTargets);

    for (auto& jmptarget : m_unlinkedCodeBlock.m_jumpTargets) {
      WRITEFIELD(jmptarget);
    }
}

void UnlinkedCodeBlockStore::loadExpressionRangeInfos(VM& vm, ByteCodeReadStore& byteCodeCache) {
    VERIFYMAGIC(MAGIC_CODEBLOCK_EXPRESSIONRANGEINFOS);    
    
    size_t expressionInfoSize;
    READFIELD(expressionInfoSize);
    
    for (size_t i = 0; i < expressionInfoSize; i++) {

        ExpressionRangeInfo info;
        info.load(vm, byteCodeCache);
        m_unlinkedCodeBlock.m_expressionInfo.append(info);
    }
}

void UnlinkedCodeBlockStore::saveExpressionRangeInfos(VM&vm, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_EXPRESSIONRANGEINFOS);

    Vector<ExpressionRangeInfo>& expressionInfo = m_unlinkedCodeBlock.m_expressionInfo;
    size_t expressionInfoSize = m_unlinkedCodeBlock.m_expressionInfo.size();
    WRITEFIELD(expressionInfoSize);
    
    for (size_t i = 0; i < expressionInfoSize; i++) {
        ExpressionRangeInfo& info = expressionInfo[i];
        info.save(vm, byteCodeCache);
    }
}

void UnlinkedCodeBlockStore::loadExpressionInfoFatPositions(VM&, ByteCodeReadStore& byteCodeCache) {
    VERIFYMAGIC(MAGIC_CODEBLOCK_EXPRESSIONINFOFATPOSITIONS);

    size_t numFatPositions;
    READFIELD(numFatPositions);

    if(numFatPositions > 0) {
        m_unlinkedCodeBlock.createRareDataIfNecessary();
    }

    for(size_t i=0; i<numFatPositions; i++) {

        uint32_t line;
        uint32_t column;
        
        READFIELD(line);
        READFIELD(column);
        
        ExpressionRangeInfo::FatPosition position;
        position.line = line;
        position.column = column;

        m_unlinkedCodeBlock.m_rareData->m_expressionInfoFatPositions.append(position);
    }
}

void UnlinkedCodeBlockStore::saveExpressionInfoFatPositions(VM& vm, ByteCodeWriteStore& byteCodeCache) {
    WRITEMAGIC(MAGIC_CODEBLOCK_EXPRESSIONINFOFATPOSITIONS);

    size_t numFatPositions = m_unlinkedCodeBlock.m_rareData ? m_unlinkedCodeBlock.m_rareData->m_expressionInfoFatPositions.size() : 0;
    WRITEFIELD(numFatPositions);

    if(numFatPositions > 0) {
        for(size_t i=0; i<numFatPositions; i++) {
            ExpressionRangeInfo::FatPosition fatPosition = m_unlinkedCodeBlock.m_rareData->m_expressionInfoFatPositions[i];
            WRITEFIELD(fatPosition.line);
            WRITEFIELD(fatPosition.column);
        }
    }
}

}