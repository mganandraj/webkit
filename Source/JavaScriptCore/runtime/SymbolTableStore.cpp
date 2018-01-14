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

#include "SymbolTableStore.h"

namespace JSC {

Ref<SymbolTableStore> SymbolTableStore::create(SymbolTable& symbolTable) {
    return WTF::adoptRef(*new SymbolTableStore(symbolTable));
}

void SymbolTableStore::load(VM& vm, const Vector<Identifier>& codeblockIdentifiers, ByteCodeReadStore& byteCodeCache) {
    uint8_t flags;
    READFIELD(flags);
    
    m_symbolTable.m_usesNonStrictEval = flags & 0x0001;
    m_symbolTable.m_nestedLexicalScope = (flags >> 1) & 0x0001;
    m_symbolTable.m_scopeType =  (flags >> 2) & 0x0003;
    
    size_t numSymbols;
    READFIELD(numSymbols);

    for(size_t i=0; i< numSymbols; i++) {
        uint8_t symbolTypeVal;
        READFIELD(symbolTypeVal);

        RefPtr<UniquedStringImpl> keyStringImpl;

        switch(static_cast<SymbolSaveType>(symbolTypeVal)) {
            case SymbolSaveType::Identifier: {
                size_t identifierIndex;
                READFIELD(identifierIndex);

                const Identifier& id = codeblockIdentifiers[identifierIndex];
                keyStringImpl = id.impl();
            }
            break;
            
            case SymbolSaveType::BuiltinPrivateName: {
                size_t index;
                READFIELD(index);
                keyStringImpl = vm.propertyNames->builtinNames().lookupPrivateNameIdentifier2(index);
            }
            break;

            case SymbolSaveType::Raw: {
                READATOMICSTRING(keyStringImpl);
            }
            break;

            case SymbolSaveType::PrivateName:
            case SymbolSaveType::WellKnownSymbol:
            case SymbolSaveType::Symbol:
            default:
                ASSERT(0);
        }
        
        intptr_t bits;
        READFIELD(bits);

        SymbolTableEntry entry;
        entry.bits() = bits;

        m_symbolTable.add(keyStringImpl.get(), WTFMove(entry));
    }

    // Read ScopedArguments table

    // Now write ScopedArguments table.
    uint32_t argumentsLength;
    READFIELD(argumentsLength);
    m_symbolTable.setArgumentsLength(vm, argumentsLength);

    for(uint32_t i=0; i<argumentsLength; i++) {
        unsigned offset;
        READFIELD(offset);

        m_symbolTable.setArgumentOffset(vm, i, ScopeOffset(offset));
    }
}

void SymbolTableStore::save(VM& vm, const Vector<Identifier>& codeblockIdentifiers, ByteCodeWriteStore& byteCodeCache) {
    uint8_t flags = 0;
    flags |= m_symbolTable.m_usesNonStrictEval;
    flags |= m_symbolTable.m_nestedLexicalScope << 1;
    flags |= m_symbolTable.m_scopeType << 2;
    
    WRITEFIELD(flags);

    size_t symbolTableSize = m_symbolTable.size();
    WRITEFIELD(symbolTableSize);
    
    size_t index;
    for( auto& entry : m_symbolTable.m_map) {
        RefPtr<UniquedStringImpl> key = entry.key;
        
        size_t identifierIndex = 0;
        // Find the symbol in the code block identifiers.
        for(const Identifier& identifier : codeblockIdentifiers) {
            // This is required so that the symbols are matched correctly.
            if(identifier.impl()->existingSymbolAwareHash() == key.get()->existingSymbolAwareHash()) {
                break; 
            }

            identifierIndex ++;
        }
        
        if(identifierIndex < codeblockIdentifiers.size()) {
            uint8_t symbolType = static_cast<uint8_t>(SymbolSaveType::Identifier);
            WRITEFIELD(symbolType);
            WRITEFIELD(identifierIndex);
        }
        else if( vm.propertyNames->builtinNames().findPrivateNameIndex2(key.get(), index)) {
            uint8_t symbolType = static_cast<uint8_t>(SymbolSaveType::BuiltinPrivateName);
            WRITEFIELD(symbolType);
            WRITEFIELD(index);    
        } else if(!key->isSymbol()){
            uint8_t symbolType = static_cast<uint8_t>(SymbolSaveType::Raw);
            WRITEFIELD(symbolType);

            ASSERT(key->length() > 0);
            WRITESTRING(key);

        } else {
            ASSERT(0);
        }

        intptr_t value = entry.value.bits();
        WRITEFIELD(value);
    }

    // Now write ScopedArguments table.
    uint32_t argsLength = m_symbolTable.argumentsLength();
    WRITEFIELD(argsLength);

    for(uint32_t i=0; i<argsLength; i++) {
        unsigned offset = m_symbolTable.argumentOffset(i).offsetUnchecked();
        WRITEFIELD(offset);
    }
}

}