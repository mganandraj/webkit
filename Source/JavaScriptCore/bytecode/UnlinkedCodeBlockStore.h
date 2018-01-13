/*
 * Copyright (C) 2012-2016 Apple Inc. All Rights Reserved.
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

#include "BytecodeConventions.h"
#include "CodeType.h"
#include "ExpressionRangeInfo.h"
#include "HandlerInfo.h"
#include "Identifier.h"
#include "JSCell.h"
#include "LockDuringMarking.h"
#include "ParserModes.h"
#include "RegExp.h"
#include "SpecialPointer.h"
#include "UnlinkedFunctionExecutable.h"
#include "VirtualRegister.h"
#include <algorithm>
#include <wtf/BitVector.h>
#include <wtf/HashSet.h>
#include <wtf/TriState.h>
#include <wtf/Vector.h>
#include <wtf/text/UniquedStringImpl.h>

namespace JSC {

class UnlinkedCodeBlockStore  : public RefCounted<UnlinkedCodeBlockStore> {
public:
    static Ref<UnlinkedCodeBlockStore> create(UnlinkedCodeBlock&);

    void save(VM&, ByteCodeWriteStore&);
    void load(VM&, ByteCodeReadStore&);

private:
    UnlinkedCodeBlockStore(UnlinkedCodeBlock& unlinkedCodeBlock)
        : m_unlinkedCodeBlock(unlinkedCodeBlock)
    {}

    void saveInstructions(VM&, ByteCodeWriteStore&);
    void loadInstructions(VM&, ByteCodeReadStore&);

    void saveVirtualRegisters(VM&, ByteCodeWriteStore&);
    void loadVirtualRegisters(VM&, ByteCodeReadStore&);

    void saveIdentifiers(VM&, ByteCodeWriteStore&);
    void loadIdentifiers(VM&, ByteCodeReadStore&);

    void saveBitVectors(VM&, ByteCodeWriteStore&);
    void loadBitVectors(VM&, ByteCodeReadStore&);

    void saveConstants(VM&, ByteCodeWriteStore&);
    void loadConstants(VM&, ByteCodeReadStore&);

    void saveConstantIdentifierSets(VM&, ByteCodeWriteStore&);
    void loadConstantIdentifierSets(VM&, ByteCodeReadStore&);

    void saveLinkTimeConstants(VM&, ByteCodeWriteStore&);
    void loadLinkTimeConstants(VM&, ByteCodeReadStore&);

    void saveProfileCounts(VM&, ByteCodeWriteStore&);
    void loadProfileCounts(VM&, ByteCodeReadStore&);

    void loadMisc(VM&, ByteCodeReadStore&);
    void saveMisc(VM&, ByteCodeWriteStore&);

    void loadFunctionDecls(VM&, ByteCodeReadStore&);
    void saveFunctionDecls(VM&, ByteCodeWriteStore&);

    void loadFunctionExprs(VM&, ByteCodeReadStore&);
    void saveFunctionExprs(VM&, ByteCodeWriteStore&);

    void loadSwitchJumpTables(VM&, ByteCodeReadStore&);
    void saveSwitchJumpTables(VM&, ByteCodeWriteStore&);

    void loadStringSwitchJumpTables(VM&, ByteCodeReadStore&);
    void saveStringSwitchJumpTables(VM&, ByteCodeWriteStore&);

    void loadExceptionHandlers(VM&, ByteCodeReadStore&);
    void saveExceptionHandlers(VM&, ByteCodeWriteStore&);

    void loadRegexps(VM&, ByteCodeReadStore&);
    void saveRegexps(VM&, ByteCodeWriteStore&);

    void loadConstantBuffers(VM&, ByteCodeReadStore&);
    void saveConstantBuffers(VM&, ByteCodeWriteStore&);

    void loadPropertyAccessInstructions(VM&, ByteCodeReadStore&);
    void savePropertyAccessInstructions(VM&, ByteCodeWriteStore&);

    void loadJumpTargets(VM&, ByteCodeReadStore&);
    void saveJumpTargets(VM&, ByteCodeWriteStore&);

    void loadExpressionRangeInfos(VM&, ByteCodeReadStore&);
    void saveExpressionRangeInfos(VM&, ByteCodeWriteStore&);

    void loadExpressionInfoFatPositions(VM&, ByteCodeReadStore&);
    void saveExpressionInfoFatPositions(VM&, ByteCodeWriteStore&);

    UnlinkedCodeBlock& m_unlinkedCodeBlock;
};

}