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

#include "ByteCodeReadStore.h"
#include "ByteCodeWriteStore.h"

#include "ByteCodeStoreMacros.h"
#include "VariableEnvironmentStore.h"

namespace JSC {

Ref<VariableEnvironmentStore> VariableEnvironmentStore::create(VariableEnvironment& variableEnvironment) {
    return WTF::adoptRef(*new VariableEnvironmentStore(variableEnvironment));
}

void VariableEnvironmentStore::save(const Vector<Identifier>& identifiers, ByteCodeWriteStore& byteCodeCache) {
    unsigned mapSize = m_variableEnvironment.m_map.size();
    WRITEFIELD(mapSize);

    for_each (m_variableEnvironment.m_map.begin(), m_variableEnvironment.m_map.end(), [&identifiers, &byteCodeCache](auto keyvaluepair){
        
        WTF::RefPtr<WTF::UniquedStringImpl> key = keyvaluepair.key;
        JSC::VariableEnvironmentEntry entry = keyvaluepair.value;

        // Find the index in the identifier vector
        uint32_t keyindex=0;
        for(const Identifier& id : identifiers){
            if (WTF::equal(id.impl(), key.get()))
                break;
            keyindex++;
        }
        ASSERT(keyindex < identifiers.size());

        WRITEFIELD(keyindex);        
        WRITEFIELD(entry.m_bits);
    });
}

void VariableEnvironmentStore::load(const Vector<Identifier>& identifiers, ByteCodeReadStore& byteCodeCache) {
    unsigned mapSize;
    READFIELD(mapSize);

    for(size_t i=0; i<mapSize; i++) {
        uint32_t keyindex;
        uint16_t entryBits;   
    
        READFIELD(keyindex);
        READFIELD(entryBits);

        RefPtr<UniquedStringImpl> keystr(identifiers[keyindex].impl());
        
        VariableEnvironmentEntry entry;
        entry.m_bits = entryBits;

        VariableEnvironment::Map::AddResult addResult = m_variableEnvironment.m_map.add(keystr, entry);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

}