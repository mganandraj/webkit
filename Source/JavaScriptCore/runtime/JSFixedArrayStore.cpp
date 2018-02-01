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

#include "JSFixedArrayStore.h"

#include "ByteCodeReadStore.h"
#include "ByteCodeWriteStore.h"

#include <wtf/text/StringStore.h>

#include "ByteCodeStoreMacros.h"

namespace JSC {

enum class FixedConstantType {
Empty = 0,
String,
NonCellValue,
};


/*static*/ JSFixedArray* JSFixedArrayStore::load(VM& vm, ByteCodeReadStore& byteCodeCache) {
    unsigned fixedArraySize;
    READFIELD(fixedArraySize);

    ASSERT(fixedArraySize > 0);

    JSFixedArray* fixedArray = JSFixedArray::create(vm, fixedArraySize);

    for (unsigned i=0; i < fixedArraySize; i++) {
        uint8_t constTypeVal;
        READFIELD(constTypeVal);
        FixedConstantType constantType = static_cast<FixedConstantType>(constTypeVal);

        switch (constantType) {
            case FixedConstantType::NonCellValue:
            {
                int64_t constant;
                READFIELD(constant);
                
                fixedArray->set(vm, i, JSValue::decode(static_cast<EncodedJSValue>(constant)));
                //m_unlinkedCodeBlock.m_rareData->m_constantBuffers.last().append(JSValue::decode(static_cast<EncodedJSValue>(constant)));  
            }
            break;

            case FixedConstantType::Empty:
            {
                fixedArray->set(vm, i, JSValue()); 
            }
            break;

            case FixedConstantType::String:
            {
                Ref<StringImpl> constStringImpl = WTF::StringStore::load(byteCodeCache.storeImplementation(), vm.symbolRegistry());
                ASSERT(constStringImpl);

                fixedArray->set(vm, i, JSValue(jsString(&vm, String(constStringImpl.get()))));                  
            }
            break;

            default:
                ASSERT(0);
        }
    }
    
    return fixedArray;
}

/*static*/ void JSFixedArrayStore::save(VM& vm, JSFixedArray& fixedArray, ByteCodeWriteStore& byteCodeCache) {
    ASSERT(fixedArray.structure(vm) == vm.fixedArrayStructure.get());
    ASSERT(fixedArray.m_size > 0);

    unsigned fixedArraySize = fixedArray.m_size;
    WRITEFIELD(fixedArraySize);

    const JSValue* fixedArrayValues = fixedArray.values();
    for (unsigned i=0; i < fixedArraySize; i++) {
        JSValue value = fixedArrayValues[i];

        if(value.isCell()) {
            
            // Copied from constant regs.
            if(value.isEmpty()) {
                uint8_t constantType = static_cast<uint8_t>(FixedConstantType::Empty);
                WRITEFIELD(constantType);
            }
            else if(value.isString()) {
                uint8_t constantType = static_cast<uint8_t>(FixedConstantType::String);
                WRITEFIELD(constantType);

                const String& constStr = static_cast<const JSString*>(value.asCell())->tryGetValue();
                WTF::StringStore::save(*constStr.impl(), byteCodeCache.storeImplementation(), vm.symbolRegistry());
            }
            else {
                ASSERT(0);
                //throw "We don't support this constant type";
            }
        }
        else {
            uint8_t constantType = static_cast<uint8_t>(FixedConstantType::NonCellValue);
            int64_t valueEnc = static_cast<int64_t>(JSValue::encode(value));

            WRITEFIELD(constantType);
            WRITEFIELD(valueEnc);
        }

    }
}

}