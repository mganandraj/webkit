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
#endif

#include <algorithm>
#include <string>

namespace JSC {

// /*static */bool ByteCodeStoreUtils::getNextStoreIndex(const char* programId, uint16_t& storeIndex) {
// 	std::string jscLocalStorePath = getJSCLocalStorePath();
// 	std::string byteCodeHeaderFilePath(jscLocalStorePath.c_str());
	
// 	#if OS(WINDOWS)
// 	byteCodeHeaderFilePath.append("\\");
// 	#else
// 	byteCodeHeaderFilePath.append("/");
// 	#endif	

// 	byteCodeHeaderFilePath.append(programId);
// 	byteCodeHeaderFilePath.append(".idx");

// 	std::ifstream indexStream(byteCodeHeaderFilePath, std::ios::in);
	
// 	if(!indexStream.fail()) {
// 		indexStream >> storeIndex; 
//         indexStream.close();

// 		storeIndex++;
		
// 		dataLogLn("getNextStoreIndex1 : ", storeIndex);
//         return true;
//     }
//     else {
// 		storeIndex=1;
// 		dataLogLn("getNextStoreIndex2 : ", storeIndex);
//         return true;
//     }
// }

// /*static */bool ByteCodeStoreUtils::getHeader(const char* programId, uint16_t& storeIndex, size_t& programOffset) {
// 	std::string jscLocalStorePath = getJSCLocalStorePath();
// 	std::string byteCodeHeaderFilePath(jscLocalStorePath.c_str());
	
// 	#if OS(WINDOWS)
// 	byteCodeHeaderFilePath.append("\\");
// 	#else
// 	byteCodeHeaderFilePath.append("/");
// 	#endif	

// 	byteCodeHeaderFilePath.append(programId);
// 	byteCodeHeaderFilePath.append(".idx");

// 	std::ifstream indexStream(byteCodeHeaderFilePath, std::ios::in);
	
// 	if(!indexStream.fail()) {
// 		indexStream >> storeIndex; 
// 		indexStream >> programOffset;
//         indexStream.close();
        
//         return true;
//     }
    
//     return false;
// }

/*static */std::string ByteCodeStoreUtils::getJSCLocalStorePath() {
	static std::string jscLocalStorePath;
	if(jscLocalStorePath.length() > 0)
		return jscLocalStorePath;

	const char* pstrJSCStorePath = Options::jscLocalStore();
	if(!pstrJSCStorePath) {
		dataLogLn("JSC Store path is not available.");
		return jscLocalStorePath;
	}

	jscLocalStorePath.assign(pstrJSCStorePath);
	return jscLocalStorePath;
}

/*static */std::string ByteCodeStoreUtils::getJSCByteCodeCachePath() {
	static std::string jscByteCodeCachePath;
	if(jscByteCodeCachePath.length() > 0)
		return jscByteCodeCachePath;

	jscByteCodeCachePath.assign(getJSCLocalStorePath());

#if OS(WINDOWS)
	jscByteCodeCachePath.append("\\");
#else
	jscByteCodeCachePath.append("/");
#endif	

	jscByteCodeCachePath.append("bytecodecache");

// Ensure that this directory is created.
#if OS(WINDOWS)
	std::wstring dirPath(jscByteCodeCachePath.begin(), jscByteCodeCachePath.end());

	if (!CreateDirectory(dirPath.c_str(), NULL) &&
		ERROR_ALREADY_EXISTS != GetLastError())
	{
		dataLogLn("ByteCode store creation failed.");
		ASSERT(0);
	}
#else
	if (mkdir(jscByteCodeCachePath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
		dataLogLn("ByteCode store creation failed.");
		ASSERT(0);
	}
#endif

	return jscByteCodeCachePath;
}


// /*static */bool ByteCodeStoreUtils::updateHeader(const char* programId, uint16_t storeIndex, size_t programOffset) {
// 	std::string jscLocalStorePath = getJSCLocalStorePath();
// 	std::string byteCodeHeaderFilePath(jscLocalStorePath.c_str());
	
// 	#if OS(WINDOWS)
// 	byteCodeHeaderFilePath.append("\\");
// 	#else
// 	byteCodeHeaderFilePath.append("/");
// 	#endif	

// 	byteCodeHeaderFilePath.append(programId);
// 	byteCodeHeaderFilePath.append(".idx");

//     std::ofstream headerStream;
//     headerStream.open(byteCodeHeaderFilePath, std::ios::out); // overwrite
//     ASSERT(!headerStream.fail());
//     headerStream << storeIndex << " " << programOffset;
//     headerStream.close();

//     return true;
// }

/*static */std::string ByteCodeStoreUtils::getByteCodeStorePathForSourceCode(const SourceCode& sourceCode) {
    std::string jscLocalStorePath = getJSCByteCodeCachePath();
	std::string byteCodeStoreFilePath(jscLocalStorePath.c_str());
	
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
	
	if (programSourceUrl.length() <= 0) {
		return std::string();
	}

	// TODO :: Use a better mechanism (for e.g hash the url) at least in ship build.
	std::string urlStr(reinterpret_cast<const char*>(programSourceUrl.characters8()), programSourceUrl.length());

	// replace all filename separators.
	std::replace( urlStr.begin(), urlStr.end(), '/', '_'); 
	std::replace( urlStr.begin(), urlStr.end(), '\\', '_'); 
	std::replace(urlStr.begin(), urlStr.end(), ':', '_');

	ASSERT(urlStr.length() > 0);
	
	urlStr.append("_");
	stringstream ss;
	ss << sourceCode.firstLine().oneBasedInt();
	urlStr.append(ss.str());

	urlStr.append("_");
	stringstream ss2;
	ss2 << sourceCode.startColumn().oneBasedInt();
	urlStr.append(ss2.str());

	urlStr.append(".jsb");
	
	return urlStr;
}


} // end namespace JSC