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

#include <fstream>
#include <string>
using namespace std;

#include <wtf/text/WTFString.h>
#include <wtf/MD5.h>
#include <wtf/text/Base64.h>

#include "parser/SourceCode.h"

namespace JSC {

struct ByteCodeStoreUtils {
public:
    static const uint32_t STORE_VERSION = 2;

    // Source code determines the byte code store path
    static std::string getByteCodeStorePathForSourceCode(const SourceCode& sourceCode);
    static std::string getByteCodeStoreFileNameForSourceCode(const SourceCode& sourceCode);

    static bool shouldCacheByteCodes();
private:
    static String stringToHash(const String& s) {
        WTF::MD5 md5;
        if (s.characters8())
            md5.addBytes(static_cast<const uint8_t*>(s.characters8()), s.length());
        else
            md5.addBytes(reinterpret_cast<const uint8_t*>(s.characters16()), 2 * s.length());

        WTF::MD5::Digest digest;
        md5.checksum(digest);

        return WTF::base64URLEncode(&digest[0], WTF::MD5::hashSize);
    }
};

} // namespace JSC