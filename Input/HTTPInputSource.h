//
//  HTTPInputSource.h
//  SFBAudioEngine-iOS
//
//  Created by Jason Swain on 14/07/2011.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#pragma once

#include <stdio.h>
#include <sys/stat.h>

#include "InputSource.h"

class HTTPInputSource : public InputSource
{
    
public:
    
	// ========================================
	// Creation
	HTTPInputSource(CFURLRef url);
    
	// ========================================
	// Destruction
	virtual ~HTTPInputSource();
    
	// ========================================
	// Bytestream access
	virtual bool Open(CFErrorRef *error = NULL);
	virtual bool Close(CFErrorRef *error = NULL);
    
	// ========================================
	//
	virtual SInt64 Read(void *buffer, SInt64 byteCount);
	virtual inline bool AtEOF() const						{ return mOffset == mLength; }
	
	virtual inline SInt64 GetOffset() const					{ return mOffset; }
	virtual inline SInt64 GetLength() const					{ return mLength; }
	
	// ========================================
	// Seeking support
	virtual inline bool SupportsSeeking() const				{ return false; }
	virtual bool SeekToOffset(SInt64 offset);
	
private:
	int mSocket;
    SInt64 mOffset;
    SInt64 mLength;
    char* mHeaderBuffer;
    char* mHeaderEnd;
};
