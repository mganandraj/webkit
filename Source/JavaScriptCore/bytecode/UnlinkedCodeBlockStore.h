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

#include "UnlinkedCodeBlock.h"

namespace JSC {

enum class ConstantType {
    Empty = 0,
    String,
    SymbolTable,
    NonCellValue
};

enum class IdentifierType {
    CommonIdentifier = 0, // Look at CommonIdentifier.h
    WellKnownSymbol,
    Symbol,
    BuiltinPrivateName,
    PrivateName,
    Normal
};

class UnlinkedCodeBlockStore  : public RefCounted<UnlinkedCodeBlockStore> {
public:
    static Ref<UnlinkedCodeBlockStore> create(UnlinkedCodeBlock&);

    void save(ByteCodeWriteStore&);
    void load(ByteCodeReadStore&);

protected:
    UnlinkedCodeBlockStore(UnlinkedCodeBlock& unlinkedCodeBlock)
        : m_unlinkedCodeBlock(unlinkedCodeBlock)
    {}

private:
    void saveInstructions(ByteCodeWriteStore&);
    void loadInstructions(ByteCodeReadStore&);

    void saveVirtualRegisters(ByteCodeWriteStore&);
    void loadVirtualRegisters(ByteCodeReadStore&);

    void saveIdentifiers(ByteCodeWriteStore&);
    void loadIdentifiers(ByteCodeReadStore&);

    void saveBitVectors(ByteCodeWriteStore&);
    void loadBitVectors(ByteCodeReadStore&);

    void saveConstants(ByteCodeWriteStore&);
    void loadConstants(ByteCodeReadStore&);

    void saveConstantIdentifierSets(ByteCodeWriteStore&);
    void loadConstantIdentifierSets(ByteCodeReadStore&);

    void saveLinkTimeConstants(ByteCodeWriteStore&);
    void loadLinkTimeConstants(ByteCodeReadStore&);

    void saveProfileCounts(ByteCodeWriteStore&);
    void loadProfileCounts(ByteCodeReadStore&);

    void loadMisc(ByteCodeReadStore&);
    void saveMisc(ByteCodeWriteStore&);

    void loadFunctionDecls(ByteCodeReadStore&);
    void saveFunctionDecls(ByteCodeWriteStore&);

    void loadFunctionExprs(ByteCodeReadStore&);
    void saveFunctionExprs(ByteCodeWriteStore&);

    void loadSwitchJumpTables(ByteCodeReadStore&);
    void saveSwitchJumpTables(ByteCodeWriteStore&);

    void loadStringSwitchJumpTables(ByteCodeReadStore&);
    void saveStringSwitchJumpTables(ByteCodeWriteStore&);

    void loadExceptionHandlers(ByteCodeReadStore&);
    void saveExceptionHandlers(ByteCodeWriteStore&);

    void loadRegexps(ByteCodeReadStore&);
    void saveRegexps(ByteCodeWriteStore&);

    void loadConstantBuffers(ByteCodeReadStore&);
    void saveConstantBuffers(ByteCodeWriteStore&);

    void loadPropertyAccessInstructions(ByteCodeReadStore&);
    void savePropertyAccessInstructions(ByteCodeWriteStore&);

    void loadJumpTargets(ByteCodeReadStore&);
    void saveJumpTargets(ByteCodeWriteStore&);

    void loadExpressionRangeInfos(ByteCodeReadStore&);
    void saveExpressionRangeInfos(ByteCodeWriteStore&);

    void loadExpressionInfoFatPositions(ByteCodeReadStore&);
    void saveExpressionInfoFatPositions(ByteCodeWriteStore&);

    UnlinkedCodeBlock& m_unlinkedCodeBlock;
};

}