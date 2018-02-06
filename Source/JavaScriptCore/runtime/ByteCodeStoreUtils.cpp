/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#include "SourceProvider.h"
#include <sstream>

#if OS(WINDOWS)
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#endif

#include <algorithm>
#include <string>

namespace JSC {

/*static */bool ByteCodeStoreUtils::shouldCacheByteCodes() {
	if(!JSC::Options::enableBytecodeCaching()) {
		dataLogLn("ByteCode caching not enabled.");
		return false;
	}

	const char* localStorePath = JSC::Options::localStore();
	if (!localStorePath || !strlen(localStorePath)) {
		dataLogLn("Local store not configured.");
		return false;
	}

	return true;
}

// Note :: Returning std::string instead of WTF::String as they are more convenient when dealing with null terminated strings ..
/*static */std::string ByteCodeStoreUtils::getByteCodeStorePathForSourceCode(const SourceCode& sourceCode) {
    std::string byteCodeStoreFilePath(JSC::Options::localStore());
	ASSERT(byteCodeStoreFilePath.size() > 0);

	#if OS(WINDOWS)
	byteCodeStoreFilePath.append("\\");
	#else
	byteCodeStoreFilePath.append("/");
	#endif	

	std::string storeFileName = getByteCodeStoreFileNameForSourceCode(sourceCode);
	if (storeFileName.length() == 0) {
		return std::string();
	}

	byteCodeStoreFilePath.append(storeFileName);
   	return byteCodeStoreFilePath;
}

/*static */std::string ByteCodeStoreUtils::getByteCodeStoreFileNameForSourceCode(const SourceCode& sourceCode) {
	SourceProvider* programSourceProvider = sourceCode.provider();
	String programSourceUrl = programSourceProvider->url();

	// Some very basic checks to make sure that a unique source url is supplied.
	// Check whether there is at least 10 characters in the url.
	if (programSourceUrl.length() <= 10) {
		return std::string();
	}

	// Check whether at least one path separator
	if (programSourceUrl.find('\\') == notFound && programSourceUrl.find('/') == notFound) {
		return std::string();
	}
	
	String sourceUrlHash = stringToHash(programSourceUrl);
	ASSERT(sourceUrlHash.is8Bit());
	ASSERT(sourceUrlHash.length() > 0);

	std::string urlStr(reinterpret_cast<const char*>(sourceUrlHash.characters8()), sourceUrlHash.length());
	urlStr.append(".jsb");
	
	return urlStr;
}


} // end namespace JSC