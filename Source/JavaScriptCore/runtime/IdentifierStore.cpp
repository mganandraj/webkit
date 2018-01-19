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

#include "IdentifierStore.h"
#include "BuiltinNames.h"

#include "ByteCodeReadStore.h"
#include "ByteCodeWriteStore.h"

#include "ByteCodeStoreMacros.h"

#include <wtf/text/StringStore.h>

namespace JSC {

enum class IdentifierType {
    CommonIdentifier = 0, // Look at CommonIdentifier.h
    WellKnownSymbol,
    Symbol,
    BuiltinPrivateName,
    PrivateName,
    Normal
};

/*static */ void IdentifierStore::save(VM& vm, Identifier& identifier, ByteCodeWriteStore& byteCodeCache) {
    if (identifier.isPrivateName()) {
        size_t privateNameIndex;
        if(vm.propertyNames->builtinNames().findPrivateNameIndex(identifier, privateNameIndex)) {
            uint8_t identifierType = static_cast<uint8_t>(IdentifierType::BuiltinPrivateName);
            WRITEFIELD(identifierType);
            WRITEFIELD(privateNameIndex);
        } else {
            uint8_t identifierType = static_cast<uint8_t>(IdentifierType::PrivateName);
            WRITEFIELD(identifierType);

            // ASSERT(identifier.string().length() == 0); // others are not yet implemented..
            WTF::StringStore::saveSymbolImpl(reinterpret_cast<SymbolImpl&>(*identifier.impl()), byteCodeCache.storeImplementation(), vm.symbolRegistry());
        }
    }
    else if(identifier.isSymbol()) {
        size_t symbolIndex;
        if (vm.propertyNames->findCommonSymbol(identifier, symbolIndex)) {
            uint8_t identifierType = static_cast<uint8_t>(IdentifierType::WellKnownSymbol);
            WRITEFIELD(identifierType);
            WRITEFIELD(symbolIndex);
        }
        else {
            uint8_t identifierType = static_cast<uint8_t>(IdentifierType::Symbol);
            WRITEFIELD(identifierType);

            // ASSERT(identifier.string().length() == 0); // other symbols are not yet implemented..
            WTF::StringStore::saveSymbolImpl(reinterpret_cast<SymbolImpl&>(*identifier.impl()), byteCodeCache.storeImplementation(), vm.symbolRegistry());
        }
    } else {
        size_t commonIdIndex;
        if(vm.propertyNames->findCommonPropName(identifier, commonIdIndex)) {
            uint8_t identifierType = static_cast<uint8_t>(IdentifierType::CommonIdentifier);
            WRITEFIELD(identifierType);
            WRITEFIELD(commonIdIndex);
        } else {
            uint8_t identifierType = static_cast<uint8_t>(IdentifierType::Normal);
            WRITEFIELD(identifierType);      

            
            //int identifierLength = identifier.length();
            //WRITEFIELD(identifierLength);

            // WRITEATOMICIDENTIFIER(identifier);
            WTF::StringStore::save(*identifier.string().impl(), byteCodeCache.storeImplementation(), vm.symbolRegistry());
        }
    }
}

/*static */ Identifier IdentifierStore::load(VM& vm, ByteCodeReadStore& byteCodeCache) {

    uint8_t identifierType;
    
    READFIELD(identifierType);
    switch(static_cast<IdentifierType>(identifierType)) {
        case IdentifierType::CommonIdentifier:{
            size_t identifierIndex;
            READFIELD(identifierIndex);
            return vm.propertyNames->lookupCommonPropNameIdenfier(identifierIndex);
        }
        break;

        case IdentifierType::WellKnownSymbol: {
            size_t identifierIndex;
            READFIELD(identifierIndex);
            return vm.propertyNames->lookupCommonSymbolIdenfier(identifierIndex);
        }
        break;

        case IdentifierType::Symbol: {
            //ASSERT(0);
            //int idlength;
            //READFIELD(idlength);
            //ASSERT(idlength == 0);

            // return Identifier::fromUid(PrivateName());
            Ref<SymbolImpl> symbolImpl = WTF::StringStore::loadSymbolImpl(byteCodeCache.storeImplementation(), vm.symbolRegistry());
            return Identifier::fromUid(&vm, symbolImpl.ptr());
        }
        break; 

        case IdentifierType::PrivateName: {
            //int idlength;
            ///READFIELD(idlength);
            //ASSERT(idlength == 0 );

            // return Identifier::fromUid(PrivateName());
            Ref<SymbolImpl> symbolImpl = WTF::StringStore::loadSymbolImpl(byteCodeCache.storeImplementation(), vm.symbolRegistry());
            return Identifier::fromUid(&vm, symbolImpl.ptr());
        }
        break;

        case IdentifierType::BuiltinPrivateName: {
            size_t identifierIndex;
            READFIELD(identifierIndex);
            ASSERT(identifierIndex >= 0);

            return vm.propertyNames->builtinNames().lookupPrivateNameIdentifier(identifierIndex);
        }
        break;

        case IdentifierType::Normal: {
            //int idlength;
            //READFIELD(idlength);
                    
            Ref<StringImpl> stringImpl = WTF::StringStore::load(byteCodeCache.storeImplementation(), vm.symbolRegistry());
            String idString(stringImpl.get());
            return Identifier::fromString(&vm, idString);

            // Identifier id;
            // READATOMICIDENTIFIER(id);
            // return id;
        }
        break;

        default:
            ASSERT(0);
    }
}

}