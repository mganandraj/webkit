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

#define MAGIC_CODEBLOCK 100
#define MAGIC_CODEBLOCK_INSTRUCTIONS 1
#define MAGIC_CODEBLOCK_VIRTUALREGISTERS 2
#define MAGIC_CODEBLOCK_IDENTIFIERS 3
#define MAGIC_CODEBLOCK_BITVECTORS 4
#define MAGIC_CODEBLOCK_CONSTANTS 5
#define MAGIC_CODEBLOCK_CONSTANTIDENTIFIERSETS 6
#define MAGIC_CODEBLOCK_LINKTIMECONSTANTS 7
#define MAGIC_CODEBLOCK_PROFILECOUNTS 8
#define MAGIC_CODEBLOCK_MISC 9
#define MAGIC_CODEBLOCK_FUNCTIONDECLS 10
#define MAGIC_CODEBLOCK_FUNCTIONEXPRS 11
#define MAGIC_CODEBLOCK_SWITCHJUMPTABLES 12
#define MAGIC_CODEBLOCK_STRINGSWITCHJUMPTABLES 13
#define MAGIC_CODEBLOCK_HANDLERS 14
#define MAGIC_CODEBLOCK_REGEXP 15
#define MAGIC_CODEBLOCK_CONSTANTBUFFERS 16
#define MAGIC_CODEBLOCK_PROPACCESSINSTRUCTIONS 17
#define MAGIC_CODEBLOCK_JUMPTARGETS 18
#define MAGIC_CODEBLOCK_EXPRESSIONRANGEINFOS 19
#define MAGIC_CODEBLOCK_EXPRESSIONINFOFATPOSITIONS 19
#define MAGIC_CODEBLOCK_END 200

#define MAGIC_SCRIPT 150

#define MAGIC_VARIABLEENVIRONMENT 'V'

#define MAGIC_UNLINKEDFUNCTIONEXECUTABLE 'U'

#define WRITEMAGIC(magic)  do { uint8_t fieldHeaderIndex = magic; WRITEFIELD(fieldHeaderIndex); } while (0)
#define VERIFYMAGIC(magic) do { uint8_t index; READFIELD(index); ASSERT(index == magic); } while (0)

//#define WRITEFIELD(field_) byteCodeCache.writeBytes(reinterpret_cast<const char*>(&field_), sizeof(field_))
//#define READFIELD(field_) byteCodeCache.readBytes(reinterpret_cast<char*>(&field_), sizeof(field_))

#define WRITEFIELD(field_) byteCodeCache.writePrimitive(&field_)
#define READFIELD(field_) byteCodeCache.readPrimitive(&field_)

#define WRITEVECTOR8(buffer_, length_) byteCodeCache.writeBytes(reinterpret_cast<const char*>(buffer_), length_)
#define WRITEVECTOR16(buffer_, length_) byteCodeCache.writeBytes(reinterpret_cast<const char*>(buffer_), 2 * length_)

// Note .. There cannot be a block in this definition as the refcountedarray should stay alive until copied to the final buffer... 
#define READVECTOR8(buffer_, length_) \
    byteCodeCache.readVector(buffer_, length_);

#define READVECTOR8_NOALLOC(buffer_, length_) \
do { \
    byteCodeCache.readBytes(reinterpret_cast<char*>(buffer_), length_); \
} while (0)


#define READVECTOR16(buffer_, length_) \
    byteCodeCache.readVector(buffer_, 2 * length_);  
    
#define WRITEATOMICIDENTIFIER(id_) \
do { \
    unsigned _length = id_.string().length(); \
    WRITEFIELD(_length); \
    if(_length > 0) { \
        bool _is8bit = id_.string().is8Bit(); \
        WRITEFIELD(_is8bit); \
        \
        if(_is8bit) { \
            WRITEVECTOR8(id_.string().characters8(), _length); \
        } else {\
            WRITEVECTOR16(id_.string().characters16(), _length); \
        } \
    } \
} while (0)

#define READATOMICIDENTIFIER(id_) \
do { \
    unsigned _length; \
    READFIELD(_length); \
    if(_length > 0) { \
        bool _is8bit; \
        READFIELD(_is8bit); \
        if(_is8bit) { \
            char* _data; \
            READVECTOR8(&_data, _length); \
            id_=Identifier::fromUid(&vm, AtomicStringImpl::add(reinterpret_cast<const LChar *>(_data), _length).get()); \
        } else { \
            char* _data; \
            READVECTOR16(&_data, _length); \
            id_=Identifier::fromUid(&vm, AtomicStringImpl::add(reinterpret_cast<const UChar *>(_data), _length).get()); \
        } \
    } \
} while (0)

/*
// Call with RefPtr<StringImpl>
#define WRITESTRING(str_) \
do { \
    unsigned _length = str_->length(); \
    WRITEFIELD(_length); \
    if(_length > 0) { \
        bool _is8bit = str_->is8Bit(); \
        WRITEFIELD(_is8bit); \
        if(_is8bit) { \
            WRITEVECTOR8(str_->characters8(), _length); \
        } else { \
            WRITEVECTOR16(str_->characters16(), _length); \
        } \
    } \
} while (0)
*/
//#define READSTRING(str_) str_=WTF::StringStore::load(byteCodeCache.storeImplementation());

// creates and sets the provided RefPtr<StringImpl>
/*
#define READSTRING(str_) \
do { \
    unsigned _length; \
    READFIELD(_length); \
    if(_length > 0) { \
        bool _is8bit; \
        READFIELD(_is8bit); \
        if(_is8bit) { \
            char* _data; \
            READVECTOR8(&_data, _length); \
            str_=StringImpl::create(reinterpret_cast<const LChar *>(_data), _length); \
        } else { \
            char* _data; \
            READVECTOR16(&_data, _length); \
            str_=StringImpl::create(reinterpret_cast<const UChar *>(_data), _length); \
        } \
    } else { \
        str_ = nullptr; \
    } \
} while (0)
*/

/*
#define READATOMICSTRING(str_) \
do { \
    unsigned _length; \
    READFIELD(_length); \
    if(_length > 0) { \
        bool _is8bit; \
        READFIELD(_is8bit); \
        if(_is8bit) { \
            char* _data; \
            READVECTOR8(&_data, _length); \
            str_=AtomicStringImpl::add(reinterpret_cast<const LChar *>(_data), _length); \
        } else { \
            char* _data; \
            READVECTOR16(&_data, _length); \
            str_=AtomicStringImpl::add(reinterpret_cast<const UChar *>(_data), _length); \
        } \
    } else { \
        str_ = nullptr; \
    } \
} while (0)
*/