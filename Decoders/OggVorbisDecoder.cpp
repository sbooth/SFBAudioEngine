/*
 *  Copyright (C) 2006, 2007, 2008, 2009 Stephen F. Booth <me@sbooth.org>
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
#include <AudioToolbox/AudioFormat.h>

#include "AudioEngineDefines.h"
#include "OggVorbisDecoder.h"


#pragma mark Static Methods


bool OggVorbisDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("ogg"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool OggVorbisDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg-vorbis"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}


#pragma mark Creation and Destruction


OggVorbisDecoder::OggVorbisDecoder(CFURLRef url)
	: AudioDecoder(url)
{
	assert(NULL != url);
	
	UInt8 buf [PATH_MAX];
	if(FALSE == CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX))
		ERR("CFURLGetFileSystemRepresentation failed");

	FILE *file = fopen(reinterpret_cast<const char *>(buf), "r");
//	NSAssert1(NULL != file, @"Unable to open the input file (%s).", strerror(errno));	
	
	int result = ov_test(file, &mVorbisFile, NULL, 0);
//	NSAssert(0 == result, NSLocalizedStringFromTable(@"The file does not appear to be a valid Ogg Vorbis file.", @"Errors", @""));
	
	result = ov_test_open(&mVorbisFile);
//	NSAssert(0 == result, NSLocalizedStringFromTable(@"Unable to open the input file.", @"Errors", @""));
	
	vorbis_info *ovInfo = ov_info(&mVorbisFile, -1);
//	NSAssert(NULL != ovInfo, @"Unable to get information on Ogg Vorbis stream.");
	
	mFormat.mSampleRate			= ovInfo->rate;
	mFormat.mChannelsPerFrame	= ovInfo->channels;
	
	switch(ovInfo->channels) {
		// Default channel layouts from Vorbis I specification section 4.3.9
		case 1:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;				break;
		case 2:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;			break;
			// FIXME: Is this the right tag for 3 channels?
		case 3:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_3_0_A;		break;
		case 4:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Quadraphonic;		break;
		case 5:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_5_0_C;		break;
		case 6:		mChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_5_1_C;		break;
	}
}

OggVorbisDecoder::~OggVorbisDecoder()
{
	if(0 != ov_clear(&mVorbisFile))
		ERR("ov_clear failed");
}

#pragma mark Functionality
	
SInt64 OggVorbisDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < this->GetTotalFrames());
	
	int result = ov_pcm_seek(&mVorbisFile, frame);
	if(result)
		return -1;
	
	return this->GetCurrentFrame();
}

UInt32 OggVorbisDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(bufferList->mNumberBuffers == mFormat.mChannelsPerFrame);
	assert(0 < frameCount);
		
	int16_t		*buffer			= static_cast<int16_t *>(calloc(frameCount * mFormat.mChannelsPerFrame, sizeof(int16_t)));
	unsigned	bufferSize		= static_cast<unsigned>(frameCount * mFormat.mChannelsPerFrame * sizeof(int16_t));
	
	if(NULL == buffer) {
		ERR("Unable to allocate memory");
		return 0;
	}
	
	int			currentSection	= 0;	
	long		currentBytes	= 0;
	long		bytesRead		= 0;
	char		*readPtr		= reinterpret_cast<char *>(buffer);
	
	for(;;) {
#if __BIG_ENDIAN__
		currentBytes = ov_read(&mVorbisFile, readPtr + bytesRead, static_cast<int>(bufferSize - bytesRead), true, sizeof(int16_t), true, &currentSection);
#else
		currentBytes = ov_read(&mVorbisFile, readPtr + bytesRead, static_cast<int>(bufferSize - bytesRead), false, sizeof(int16_t), true, &currentSection);
#endif
		
		if(0 > currentBytes) {
			LOG("Ogg Vorbis decode error");
			free(buffer), buffer = NULL;
			return 0;
		}
		
		bytesRead += currentBytes;
		
		if(0 == currentBytes || 0 == bufferSize - bytesRead)
			break;
	}
	
	UInt32		framesRead		= static_cast<UInt32>((bytesRead / sizeof(int16_t)) / mFormat.mChannelsPerFrame);
	float		scaleFactor		= (1 << (16 - 1));
	
	// Deinterleave the 16-bit samples and convert to float
	for(unsigned channel = 0; channel < mFormat.mChannelsPerFrame; ++channel) {
		float *floatBuffer = static_cast<float *>(bufferList->mBuffers[channel].mData);
		
		for(unsigned sample = channel; sample < framesRead * mFormat.mChannelsPerFrame; sample += mFormat.mChannelsPerFrame)
			*floatBuffer++ = static_cast<float>(buffer[sample] / scaleFactor);
		
		bufferList->mBuffers[channel].mNumberChannels	= 1;
		bufferList->mBuffers[channel].mDataByteSize		= static_cast<UInt32>(framesRead * sizeof(float));
	}
	
	free(buffer), buffer = NULL;
	
	return framesRead;
}
