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

#include <wtf/WriteStoreImplementation.h>
#include <wtf/ReadStoreImplementation.h>
#include <wtf/Ref.h>

#include "StringStore.h"

#include <wtf/DataLog.h>

namespace WTF {

/*static */ Ref<SymbolImpl> StringStore::loadSymbolImpl(ReadStoreImplementation& readStore, SymbolRegistry& symbolRegistry) {
    bool isNullSymbol;
    readStore.readBytes(reinterpret_cast<char*>(&isNullSymbol), sizeof(bool));

    if(isNullSymbol) {
        return SymbolImpl::createNullSymbol();
    }

    bool isRegistered;
    bool isPrivate;

    readStore.readBytes(reinterpret_cast<char*>(&isRegistered), sizeof(bool));
    readStore.readBytes(reinterpret_cast<char*>(&isPrivate), sizeof(bool));

    RefPtr<StringImpl> ownerStringImpl;

    StringImpl::StringKind ownerStringKind;
    readStore.readBytes(reinterpret_cast<char*>(&ownerStringKind), sizeof(StringImpl::StringKind));

    unsigned length;
    readStore.readBytes(reinterpret_cast<char*>(&length), sizeof(unsigned));

    if(length > 0) {

        bool is8bit;
        readStore.readBytes(reinterpret_cast<char*>(&is8bit), sizeof(bool));

        char* data;
        readStore.readVector(&data, is8bit ? length : 2 * length);

        switch (ownerStringKind)
        {
            case StringImpl::StringKind::StringNormal :
                if(is8bit)
                    ownerStringImpl = adoptRef(&StringImpl::create(reinterpret_cast<const LChar *>(data) , length).leakRef());
                else
                    ownerStringImpl = adoptRef(&StringImpl::create(reinterpret_cast<const UChar *>(data) , length).leakRef());
                
                break;
            case StringImpl::StringKind::StringAtomic :
                if(is8bit)
                    ownerStringImpl = AtomicStringImpl::add(reinterpret_cast<const LChar *>(data) , length);
                else
                    ownerStringImpl = AtomicStringImpl::add(reinterpret_cast<const UChar *>(data) , length);
            
                break;                
            default:
                dataLogLn("Trying to load unsupported string type.");
                RELEASE_ASSERT(0); // Not implemented.
        }
    }

    if(ownerStringImpl) {
        if(isPrivate)
            return adoptRef(PrivateSymbolImpl::create(*ownerStringImpl).leakRef());
        else if (isRegistered)
            //return RegisteredSymbolImpl::create(*ownerStringImpl, symbolRegistry);
            return adoptRef(symbolRegistry.symbolForKey(*ownerStringImpl).leakRef());
        else
            return SymbolImpl::create(*ownerStringImpl);
    } else {
        RELEASE_ASSERT(!isRegistered);

        if(isPrivate)
            return adoptRef(PrivateSymbolImpl::createNullSymbol().leakRef());
        else
            return SymbolImpl::createNullSymbol();
    }
}

/*static*/ void StringStore::saveSymbolImpl(SymbolImpl& symbolImpl, WriteStoreImplementation& writeStore, SymbolRegistry& currentVMSymbolRegistry) {
    
    bool isNullSymbol = symbolImpl.isNullSymbol();
    writeStore.writeBytes(reinterpret_cast<const char*>(&isNullSymbol), sizeof(bool));

    if(isNullSymbol)
        return;

    bool isRegistered = symbolImpl.isRegistered();
    bool isPrivate = symbolImpl.isPrivate();

    if(isRegistered) {
        // We don't support loading alternate symbol registries.
        RELEASE_ASSERT(&currentVMSymbolRegistry == symbolImpl.symbolRegistry());
    }
    
    writeStore.writeBytes(reinterpret_cast<const char*>(&isRegistered), sizeof(bool));
    writeStore.writeBytes(reinterpret_cast<const char*>(&isPrivate), sizeof(bool));
    
    StringImpl* ownerStringImpl = symbolImpl.m_owner;
    
    // AFAIK, this can be null only for static symbols which we don't support anyways.
    RELEASE_ASSERT(ownerStringImpl);

    StringImpl::StringKind stringKind = ownerStringImpl->stringKind();
    RELEASE_ASSERT(stringKind != StringImpl::StringKind::StringSymbol);

    // Note that we ideally want to assert that the owner is an atomic string..
    // using normal string as description will result in duplicated string when loading byte codes.

    writeStore.writeBytes(reinterpret_cast<const char*>(&stringKind), sizeof(StringImpl::StringKind));
    
    unsigned length = ownerStringImpl->length();
    writeStore.writeBytes(reinterpret_cast<const char*>(&length), sizeof(unsigned));

    if(length > 0) {
        bool is8bit = ownerStringImpl->is8Bit();
        writeStore.writeBytes(reinterpret_cast<const char*>(&is8bit), sizeof(bool));

        if(is8bit) {
            writeStore.writeBytes(reinterpret_cast<const char*>(ownerStringImpl->characters8()), length);
        } else {
            writeStore.writeBytes(reinterpret_cast<const char*>(ownerStringImpl->characters16()), 2*length);
        }
    }
}

/*static*/ void StringStore::save(StringImpl& stringImpl, WriteStoreImplementation& writeStore, SymbolRegistry& currentVMSymbolRegistry) {
    
    /*
    Notes :
    1. We are not persisting the flags as such, as we won't be able to restore the strings with full-fidility 
    for a few cases (below). Hence, we are explicitly handling the various cases such as buffer ownership, bitness, string kind etc.
    2. When roundtripping through the store, BufferOwned will be converted to BufferInternal. I believe, it won't affet the semantics in any csae.
    3. When roundtripping through the store, non-symbol StringImpls with BufferSubString ownership will be collapsed to BufferInternal. And we assume
    that intented string is equal to the owner string. With some jugglery, we can implement this scenario a little better, but we can't 
    get complete fidility as there is no way to persist the connection to the actual owner string. We are release asserting ..

    */

    StringImpl::StringKind stringKind = stringImpl.stringKind();
    writeStore.writeBytes(reinterpret_cast<const char*>(&stringKind), sizeof(StringImpl::StringKind));

    if(stringKind == StringImpl::StringKind::StringSymbol) {
        // We don't support persisting static symbols.
        RELEASE_ASSERT(stringImpl.bufferOwnership() == StringImpl::BufferOwnership::BufferSubstring);

        SymbolImpl& symbolImpl = reinterpret_cast<SymbolImpl&>(stringImpl);
        saveSymbolImpl(symbolImpl, writeStore, currentVMSymbolRegistry);
        return;
    }

    unsigned length = stringImpl.length();
    writeStore.writeBytes(reinterpret_cast<const char*>(&length), sizeof(unsigned));

    if(length > 0) {
        bool is8bit = stringImpl.is8Bit();
        writeStore.writeBytes(reinterpret_cast<const char*>(&is8bit), sizeof(bool));

        if(is8bit) {
            writeStore.writeBytes(reinterpret_cast<const char*>(stringImpl.characters8()), length);
        } else {
            writeStore.writeBytes(reinterpret_cast<const char*>(stringImpl.characters16()), 2*length);
        }
    }
}

/*static*/ Ref<StringImpl> StringStore::load(ReadStoreImplementation& readStore, SymbolRegistry& registry) {
    StringImpl::StringKind stringKind;
    readStore.readBytes(reinterpret_cast<char*>(&stringKind), sizeof(StringImpl::StringKind));

    if(stringKind == StringImpl::StringKind::StringSymbol) {
        return adoptRef(loadSymbolImpl(readStore, registry).leakRef());
    }

    unsigned length;
    readStore.readBytes(reinterpret_cast<char*>(&length), sizeof(unsigned));

    if(length > 0) {

        bool is8bit;
        readStore.readBytes(reinterpret_cast<char*>(&is8bit), sizeof(bool));

        char* data;
        readStore.readVector(&data, is8bit ? length : 2 * length);

        switch (stringKind)
        {
            case StringImpl::StringKind::StringNormal :
                if(is8bit)
                    return StringImpl::create(reinterpret_cast<const LChar *>(data) , length);
                else
                    return StringImpl::create(reinterpret_cast<const UChar *>(data) , length);
                
                break;
            case StringImpl::StringKind::StringAtomic :
                if(is8bit)
                    return adoptRef(*AtomicStringImpl::add(reinterpret_cast<const LChar *>(data) , length).leakRef());
                else
                    return adoptRef(*AtomicStringImpl::add(reinterpret_cast<const UChar *>(data) , length).leakRef());
            
                break;                
            default:
                dataLogLn("Trying to load unsupported string type.");
                RELEASE_ASSERT(0); // Not implemented.
        }

    } else { 
        switch (stringKind)
        {
            case StringImpl::StringKind::StringNormal :
                return adoptRef(*StringImpl::empty());
	        case StringImpl::StringKind::StringAtomic:
                return adoptRef(*StringImpl::empty());
            default:
                dataLogLn("Trying to load empty unsupported string type.");
                RELEASE_ASSERT(0); // Not implemented.
        }
    }

}


/*static*/ Ref<UniquedStringImpl> StringStore::loadUniqued(ReadStoreImplementation&readStore, SymbolRegistry& registry) {
    StringImpl::StringKind stringKind;
    readStore.readBytes(reinterpret_cast<char*>(&stringKind), sizeof(StringImpl::StringKind));

	 unsigned length;
	 readStore.readBytes(reinterpret_cast<char*>(&length), sizeof(unsigned));

    if(length > 0) {

        bool is8bit;
        readStore.readBytes(reinterpret_cast<char*>(&is8bit), sizeof(bool));

        char* data;
        readStore.readVector(&data, is8bit ? length : 2 * length);

        switch (stringKind)
        {
            case StringImpl::StringKind::StringNormal :
                dataLogLn("Normal strings are not uniqued.");
                RELEASE_ASSERT(0); // Normal strings are not uniqued.
                break;
            case StringImpl::StringKind::StringAtomic :
                if(is8bit)
                    return adoptRef(*AtomicStringImpl::add(reinterpret_cast<const LChar *>(data) , length).leakRef());
                else
                    return adoptRef(*AtomicStringImpl::add(reinterpret_cast<const UChar *>(data) , length).leakRef());
            
                break;
            case StringImpl::StringKind::StringSymbol :
                return adoptRef(loadSymbolImpl(readStore, registry).leakRef());
            
            default:
                RELEASE_ASSERT(0); // Not implemented.
        }

    } else { 
        switch (stringKind)
        {
            case StringImpl::StringKind::StringNormal :
                RELEASE_ASSERT(0);
	        case StringImpl::StringKind::StringAtomic:
                return adoptRef(*AtomicStringImpl::add(StringImpl::empty()).leakRef());
            case StringImpl::StringKind::StringSymbol:
                dataLogLn("Trying to load empty symbol string type.");
                RELEASE_ASSERT(0); // Not implemented.
            default:
                dataLogLn("Trying to load empty unsupported string type.");
                RELEASE_ASSERT(0); // Not implemented.
        }
    }
}


}