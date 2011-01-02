/*
 *  Copyright (C) 2010, 2011 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <CoreServices/CoreServices.h>
#include <iomanip>

#include "CFOperatorOverloads.h"

#define BUFFER_LENGTH 512

std::ostream& operator<<(std::ostream& out, CFStringRef s)
{
	if(NULL == s) {
		out << "(null)";
		return out;
	}

	char buf [BUFFER_LENGTH];

	CFIndex totalCharacters = CFStringGetLength(s);
	CFIndex currentCharacter = 0;
	CFIndex charactersConverted = 0;
	CFIndex bytesWritten;

	while(currentCharacter < totalCharacters) {
		charactersConverted = CFStringGetBytes(s, CFRangeMake(currentCharacter, totalCharacters), kCFStringEncodingUTF8, 0, false, 
												reinterpret_cast<UInt8 *>(buf), BUFFER_LENGTH, &bytesWritten);
		currentCharacter += charactersConverted;
		out.write(buf, bytesWritten);
	};

	return out;
}

std::ostream& operator<<(std::ostream& out, CFURLRef u)
{
	if(NULL == u) {
		out << "(null)";
		return out;
	}

	CFStringRef s = CFURLGetString(u);
	if(CFStringHasPrefix(s, CFSTR("file:"))) {
		CFStringRef displayName = NULL;
		OSStatus result = LSCopyDisplayNameForURL(u, &displayName);

		if(noErr == result && NULL != displayName) {
			out << displayName;
			CFRelease(displayName), displayName = NULL;
		}
	}
	else
		out << s;
	
	return out;
}

// Most of this is stolen from Apple's CAStreamBasicDescription::Print()
std::ostream& operator<<(std::ostream& out, const AudioStreamBasicDescription& format)
{
	unsigned char formatID [5];
	*(UInt32 *)formatID = OSSwapHostToBigInt32(format.mFormatID);
	formatID[4] = '\0';
	
	// General description
	out << format.mChannelsPerFrame << " ch, " << format.mSampleRate << " Hz, '" << formatID << "' (0x" << std::hex << std::setw(8) << std::setfill('0') << format.mFormatFlags << std::dec << ") ";
	
	if(kAudioFormatLinearPCM == format.mFormatID) {
		// Bit depth
		UInt32 fractionalBits = ((0x3f << 7)/*kLinearPCMFormatFlagsSampleFractionMask*/ & format.mFormatFlags) >> 7/*kLinearPCMFormatFlagsSampleFractionShift*/;
		if(0 < fractionalBits)
			out << (format.mBitsPerChannel - fractionalBits) << "." << fractionalBits;
		else
			out << format.mBitsPerChannel;
		
		out << "-bit";
		
		// Endianness
		bool isInterleaved = !(kAudioFormatFlagIsNonInterleaved & format.mFormatFlags);
		UInt32 interleavedChannelCount = (isInterleaved ? format.mChannelsPerFrame : 1);
		UInt32 sampleSize = (0 < format.mBytesPerFrame && 0 < interleavedChannelCount ? format.mBytesPerFrame / interleavedChannelCount : 0);
		if(1 < sampleSize)
			out << ((kLinearPCMFormatFlagIsBigEndian & format.mFormatFlags) ? " big-endian" : " little-endian");
		
		// Sign
		bool isInteger = !(kLinearPCMFormatFlagIsFloat & format.mFormatFlags);
		if(isInteger)
			out << ((kLinearPCMFormatFlagIsSignedInteger & format.mFormatFlags) ? " signed" : " unsigned");
		
		// Integer or floating
		out << (isInteger ? " integer" : " float");
		
		// Packedness
		if(0 < sampleSize && ((sampleSize << 3) != format.mBitsPerChannel))
			out << ((kLinearPCMFormatFlagIsPacked & format.mFormatFlags) ? ", packed in " : ", unpacked in ") << sampleSize << " bytes";
		
		// Alignment
		if((0 < sampleSize && ((sampleSize << 3) != format.mBitsPerChannel)) || (0 != (format.mBitsPerChannel & 7)))
			out << ((kLinearPCMFormatFlagIsAlignedHigh & format.mFormatFlags) ? " high-aligned" : " low-aligned");
		
		if(!isInterleaved)
			out << ", deinterleaved";
	}
	else if(kAudioFormatAppleLossless == format.mFormatID) {
		UInt32 sourceBitDepth = 0;
		switch(format.mFormatFlags) {
			case kAppleLosslessFormatFlag_16BitSourceData:		sourceBitDepth = 16;	break;
    		case kAppleLosslessFormatFlag_20BitSourceData:		sourceBitDepth = 20;	break;
    		case kAppleLosslessFormatFlag_24BitSourceData:		sourceBitDepth = 24;	break;
    		case kAppleLosslessFormatFlag_32BitSourceData:		sourceBitDepth = 32;	break;
		}
		
		if(0 != sourceBitDepth)
			out << "from " << sourceBitDepth << "-bit source, ";
		else
			out << "from UNKNOWN source bit depth, ";
		
		out << format.mFramesPerPacket << " frames/packet";
	}
	else
		out << format.mBitsPerChannel << " bits/channel, " << format.mBytesPerPacket << " bytes/packet, " << format.mFramesPerPacket << " frames/packet, " << format.mBytesPerFrame << " bytes/frame";
	
	return out;	
}
