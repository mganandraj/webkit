/*
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
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

#pragma once

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Ref.h>
#include <wtf/WriteStoreImplementation.h>

using namespace WTF;

namespace JSC {

class ProgramExecutable;
class VM;
        
class ByteCodeWriteStore : public RefCounted<ByteCodeWriteStore> {
public:
    static RefPtr<ByteCodeWriteStore> createForProgram(ProgramExecutable&);

    // void writeFunction(VM&v, FunctionExecutable*);
    
    size_t currentWritePosition();
    void writeBytes(const char*, size_t);
    
    void writeEpilogue(ProgramExecutable& program, size_t programOffset);

    template <typename T>
    void writePrimitive(T* buffer) {
        static_assert(std::is_fundamental<T>::value, "Not a primitive type!!");
        m_storeImplementation->writeBytes(reinterpret_cast<const char*>(buffer), sizeof(T));
    }

    WriteStoreImplementation& storeImplementation() { return m_storeImplementation.get(); }

private:
    WTF_MAKE_NONCOPYABLE(ByteCodeWriteStore);

    ByteCodeWriteStore(WriteStoreImplementation& storeImplementation);    
    Ref<WriteStoreImplementation> m_storeImplementation;
};

} // namespace JSC