/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010 Stephen F. Booth <me@sbooth.org>
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

#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdexcept>

#include "AudioEngineDefines.h"
#include "MPEGDecoder.h"
#include "CreateDisplayNameForURL.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"


#define INPUT_BUFFER_SIZE	(5 * 8192)
#define LAME_HEADER_SIZE	((8 * 5) + 4 + 4 + 8 + 32 + 16 + 16 + 4 + 4 + 8 + 12 + 12 + 8 + 8 + 2 + 3 + 11 + 32 + 32 + 32)

#define BIT_RESOLUTION		24

// From vbrheadersdk:
// ========================================
// A Xing header may be present in the ancillary
// data field of the first frame of an mp3 bitstream
// The Xing header (optionally) contains
//      frames      total number of audio frames in the bitstream
//      bytes       total number of bytes in the bitstream
//      toc         table of contents

// toc (table of contents) gives seek points
// for random access
// the ith entry determines the seek point for
// i-percent duration
// seek point in bytes = (toc[i]/256.0) * total_bitstream_bytes
// e.g. half duration seek point = (toc[50]/256.0) * total_bitstream_bytes

#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008

// Clipping and rounding code from madplay(audio.c):
/*
 * madplay - MPEG audio decoder and player
 * Copyright (C) 2000-2004 Robert Leslie
 */
static int32_t 
audio_linear_round(unsigned int bits, 
				   mad_fixed_t sample)
{
	enum {
		MIN = -MAD_F_ONE,
		MAX =  MAD_F_ONE - 1
	};
	
	/* round */
	sample += (1 << (MAD_F_FRACBITS - bits));
	
	/* clip */
	if(MAX < sample)
		sample = MAX;
	else if(MIN > sample)
		sample = MIN;
	
	/* quantize and scale */
	return sample >> (MAD_F_FRACBITS + 1 - bits);
}
// End madplay code


#pragma mark Static Methods


CFArrayRef MPEGDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("mp3") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedExtensions), 1, &kCFTypeArrayCallBacks);
}

CFArrayRef MPEGDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/mpeg") };
	return CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(supportedMIMETypes), 1, &kCFTypeArrayCallBacks);
}

bool MPEGDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	assert(NULL != extension);
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mp3"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

bool MPEGDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	assert(NULL != mimeType);	
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mpeg"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}


#pragma mark Creation and Destruction


MPEGDecoder::MPEGDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mMPEGFramesDecoded(0), mTotalMPEGFrames(0), mSamplesToSkipInNextFrame(0), mCurrentFrame(0), mTotalFrames(0), mEncoderDelay(0), mEncoderPadding(0), mSamplesDecoded(0), mSamplesPerMPEGFrame(0), mFoundXingHeader(0), mFoundLAMEHeader(0)
{
	mInputBuffer = static_cast<unsigned char *>(calloc(INPUT_BUFFER_SIZE + MAD_BUFFER_GUARD, sizeof(unsigned char)));
	
	if(NULL == mInputBuffer)
		throw std::bad_alloc();
	
	mad_stream_init(&mStream);
	mad_frame_init(&mFrame);
	mad_synth_init(&mSynth);
	
	memset(mXingTOC, 0, 100 * sizeof(uint8_t));
}

MPEGDecoder::~MPEGDecoder()
{
	if(FileIsOpen())
		CloseFile();

	if(mInputBuffer)
		free(mInputBuffer), mInputBuffer = NULL;
	
	mad_synth_finish(&mSynth);
	mad_frame_finish(&mFrame);
	mad_stream_finish(&mStream);
}


#pragma mark Functionality


bool MPEGDecoder::OpenFile(CFErrorRef *error)
{
	// Scan file to determine sample rate, channels, total frames, etc
	if(!this->ScanFile(!mInputSource->SupportsSeeking())) {
		if(error) {
			CFMutableDictionaryRef errorDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																			   32,
																			   &kCFTypeDictionaryKeyCallBacks,
																			   &kCFTypeDictionaryValueCallBacks);
			
			CFStringRef displayName = CreateDisplayNameForURL(mInputSource->GetURL());
			CFStringRef errorString = CFStringCreateWithFormat(kCFAllocatorDefault, 
															   NULL, 
															   CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MP3 file."), ""), 
															   displayName);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedDescriptionKey, 
								 errorString);
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedFailureReasonKey, 
								 CFCopyLocalizedString(CFSTR("Not an MP3 file"), ""));
			
			CFDictionarySetValue(errorDictionary, 
								 kCFErrorLocalizedRecoverySuggestionKey, 
								 CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));
			
			CFRelease(errorString), errorString = NULL;
			CFRelease(displayName), displayName = NULL;
			
			*error = CFErrorCreate(kCFAllocatorDefault, 
								   AudioDecoderErrorDomain, 
								   AudioDecoderInputOutputError, 
								   errorDictionary);
			
			CFRelease(errorDictionary), errorDictionary = NULL;				
		}
		
		return false;
	}

	// Canonical Core Audio format
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mFormat.mBitsPerChannel		= 8 * sizeof(float);
	
	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
	
	mFormat.mReserved			= 0;
		
	// Allocate the buffer list
	mBufferList = AllocateABL(mFormat.mChannelsPerFrame, sizeof(float), false, mSamplesPerMPEGFrame);
	
	if(NULL == mBufferList) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);

		return false;
	}

	return true;
}

bool MPEGDecoder::CloseFile(CFErrorRef */*error*/)
{
	if(mBufferList)
		mBufferList = DeallocateABL(mBufferList);

	return true;
}

CFStringRef MPEGDecoder::CreateSourceFormatDescription()
{
	CFStringRef layerDescription = NULL;
	switch(mMPEGLayer) {
		case MAD_LAYER_I:
			mSourceFormat.mFormatID = kAudioFormatMPEGLayer1;
			layerDescription = CFSTR("Layer I");
			break;

		case MAD_LAYER_II:
			mSourceFormat.mFormatID = kAudioFormatMPEGLayer2;
			layerDescription = CFSTR("Layer II");
			break;
		
		case MAD_LAYER_III:
			mSourceFormat.mFormatID = kAudioFormatMPEGLayer3;
			layerDescription = CFSTR("Layer III");
			break;
	}
	
	CFStringRef channelDescription = NULL;
	switch(mMode) {  
		case MAD_MODE_SINGLE_CHANNEL:	channelDescription = CFSTR("Single Channel");	break;
		case MAD_MODE_DUAL_CHANNEL:		channelDescription = CFSTR("Dual Channel");		break;
		case MAD_MODE_JOINT_STEREO:		channelDescription = CFSTR("Joint Stereo");		break;
		case MAD_MODE_STEREO:			channelDescription = CFSTR("Stereo");			break;
	}

	return CFStringCreateWithFormat(kCFAllocatorDefault, 
									NULL, 
									CFSTR("MPEG-1 Audio (%@), %@, %u Hz"), 
									layerDescription,
									channelDescription,
									static_cast<unsigned int>(mSourceFormat.mSampleRate));
}

SInt64 MPEGDecoder::SeekToFrame(SInt64 frame)
{
	if(mFoundLAMEHeader)
		return this->SeekToFrameAccurately(frame);
	else
		return this->SeekToFrameApproximately(frame);
}

UInt32 MPEGDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(bufferList->mNumberBuffers == mFormat.mChannelsPerFrame);
	assert(0 < frameCount);
	
	UInt32 framesRead = 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;
	
	for(;;) {
		
		UInt32	framesRemaining	= frameCount - framesRead;
		UInt32	framesToSkip	= static_cast<UInt32>(bufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesInBuffer	= static_cast<UInt32>(mBufferList->mBuffers[0].mDataByteSize / sizeof(float));
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);
		
		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			float *floatBuffer = static_cast<float *>(bufferList->mBuffers[i].mData);
			memcpy(floatBuffer + framesToSkip, mBufferList->mBuffers[i].mData, framesToCopy * sizeof(float));
			bufferList->mBuffers[i].mDataByteSize += static_cast<UInt32>(framesToCopy * sizeof(float));
			
			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				floatBuffer = static_cast<float *>(mBufferList->mBuffers[i].mData);
				memmove(floatBuffer, floatBuffer + framesToCopy, (framesInBuffer - framesToCopy) * sizeof(float));
			}
			
			mBufferList->mBuffers[i].mDataByteSize -= static_cast<UInt32>(framesToCopy * sizeof(float));
		}
		
		framesRead += framesToCopy;

		// All requested frames were read
		if(framesRead == frameCount)
			break;
		
		// If the file contains a Xing header but not LAME gapless information,
		// decode the number of MPEG frames specified by the Xing header
		if(mFoundXingHeader && !mFoundLAMEHeader && 1 + mMPEGFramesDecoded == mTotalMPEGFrames)
			break;
		
		// The LAME header indicates how many samples are in the file
		if(mFoundLAMEHeader && this->GetTotalFrames() == mSamplesDecoded)
			break;
		
		// Decode and synthesize the next MPEG frame
		if(!DecodeMPEGFrame())
			break;

		SynthesizeMPEGFrame();
	}
	
	mCurrentFrame += framesRead;

	return framesRead;
}

bool MPEGDecoder::DecodeMPEGFrame(bool decoderHeaderOnly)
{
	bool readEOF = false;

	for(;;) {
		// Feed the input buffer if necessary
		if(NULL == mStream.buffer || MAD_ERROR_BUFLEN == mStream.error) {
			SInt64 bytesToRead;
			ptrdiff_t bytesRemaining;
			unsigned char *readStartPointer;
			
			if(NULL != mStream.next_frame) {
				bytesRemaining = mStream.bufend - mStream.next_frame;
				memmove(mInputBuffer, mStream.next_frame, bytesRemaining);
				
				readStartPointer	= mInputBuffer + bytesRemaining;
				bytesToRead			= INPUT_BUFFER_SIZE - bytesRemaining;
			}
			else {
				bytesToRead			= INPUT_BUFFER_SIZE,
				readStartPointer	= mInputBuffer,
				bytesRemaining		= 0;
			}
			
			// Read raw bytes from the MP3 file
			SInt64 bytesRead = mInputSource->Read(readStartPointer, bytesToRead);
			if(0 == bytesRead && !mInputSource->AtEOF()) {
#if DEBUG
				LOG("Read error");
#endif
				return false;
			}
			
			// MAD_BUFFER_GUARD zeroes are required to decode the last MPEG frame of the file
			if(mInputSource->AtEOF()) {
				memset(readStartPointer + bytesRead, 0, MAD_BUFFER_GUARD);
				bytesRead += MAD_BUFFER_GUARD;
				readEOF = true;
			}
			
			mad_stream_buffer(&mStream, mInputBuffer, bytesRead + bytesRemaining);
			mStream.error = MAD_ERROR_NONE;
		}
		
		// Decode the MPEG frame's header
		int result;
		if(decoderHeaderOnly)
			result = mad_header_decode(&mFrame.header, &mStream);
		else
			result = mad_frame_decode(&mFrame, &mStream);

		if(-1 == result) {
			if(MAD_RECOVERABLE(mStream.error)) {
				// Prevent ID3 tags from reporting recoverable frame errors
				const uint8_t	*buffer			= mStream.this_frame;
				ptrdiff_t		buflen			= mStream.bufend - mStream.this_frame;
				uint32_t		id3_length		= 0;
				
				if(10 <= buflen && 0x49 == buffer[0] && 0x44 == buffer[1] && 0x33 == buffer[2]) {
					id3_length = (((buffer[6] & 0x7F) << (3 * 7)) | ((buffer[7] & 0x7F) << (2 * 7)) |
								  ((buffer[8] & 0x7F) << (1 * 7)) | ((buffer[9] & 0x7F) << (0 * 7)));
					
					// Add 10 bytes for ID3 header
					id3_length += 10;
					
					mad_stream_skip(&mStream, id3_length);
				}
#if DEBUG
				else
					LOG("Recoverable frame level error (%s)", mad_stream_errorstr(&mStream));
#endif
				
				continue;
			}
			// EOS for non-Xing streams occurs when EOF is reached and no further frames can be decoded
			else if(MAD_ERROR_BUFLEN == mStream.error && readEOF)
				return false;
			// The buffer did not contain the desired data
			else if(MAD_ERROR_BUFLEN == mStream.error)
				continue;
			else {
#if DEBUG
				LOG("Unrecoverable frame level error (%s)", mad_stream_errorstr(&mStream));
#endif
				break;
			}
		}
		
		// At this point a frame was successfully decoded
		++mMPEGFramesDecoded;
		
		break;
	}

	return true;
}

bool MPEGDecoder::SynthesizeMPEGFrame()
{
	const float scaleFactor = (1L << (BIT_RESOLUTION - 1));

	// Synthesize the frame into PCM
	mad_synth_frame(&mSynth, &mFrame);
	
	// Skip any samples that remain from last frame
	// This can happen if the encoder delay is greater than the number of samples in a frame
	uint32_t startingSample = mSamplesToSkipInNextFrame;
	
	// Skip the Xing header (it contains empty audio)
	if(mFoundXingHeader && 1 == mMPEGFramesDecoded)
		return true;
	// Adjust the first real audio frame for gapless playback
	else if(mFoundLAMEHeader && 2 == mMPEGFramesDecoded)
		startingSample += mEncoderDelay;
	
	// The number of samples in this frame
	uint32_t sampleCount = mSynth.pcm.length;
	// Skip this entire frame if necessary
	if(startingSample > sampleCount) {
		mSamplesToSkipInNextFrame += startingSample - sampleCount;
		return true;
	}
	else
		mSamplesToSkipInNextFrame = 0;
	
	// If a LAME header was found, the total number of audio frames (AKA samples) 
	// is known.  Ensure only that many are output
	if(mFoundLAMEHeader && this->GetTotalFrames() < mSamplesDecoded + (sampleCount - startingSample))
		sampleCount = static_cast<uint32_t>(this->GetTotalFrames() - mSamplesDecoded);
	
	// Output samples in 32-bit float PCM
	int32_t audioSample;
	for(uint32_t channel = 0; channel < MAD_NCHANNELS(&mFrame.header); ++channel) {
		float *floatBuffer = static_cast<float *>(mBufferList->mBuffers[channel].mData);
		
		for(uint32_t sample = startingSample; sample < sampleCount; ++sample) {
			audioSample = audio_linear_round(BIT_RESOLUTION, mSynth.pcm.samples[channel][sample]);
			*floatBuffer++ = static_cast<float>(audioSample / scaleFactor);
		}
		
		mBufferList->mBuffers[channel].mNumberChannels	= 1;
		mBufferList->mBuffers[channel].mDataByteSize	= static_cast<UInt32>((sampleCount - startingSample) * sizeof(float));
	}
	
	mSamplesDecoded += (sampleCount - startingSample);
	
	return true;
}

bool MPEGDecoder::ScanFile(bool estimateTotalFrames)
{
	// Attempt to decode the first MPEG frame, in hope of a Xing header
	if(!DecodeMPEGFrame()) {
		ERR("Could not decode first MPEG frame");
		return false;
	}

	// Save the relevant parameters
	mMPEGLayer					= mFrame.header.layer;
	mMode						= mFrame.header.mode;
	mEmphasis					= mFrame.header.emphasis;

	mFormat.mSampleRate			= mFrame.header.samplerate;
	mFormat.mChannelsPerFrame	= MAD_NCHANNELS(&mFrame.header);
	
	mSamplesPerMPEGFrame		= 32 * MAD_NSBSAMPLES(&mFrame.header);

	// Set up the source format
	switch(mMPEGLayer) {
		case MAD_LAYER_I:		mSourceFormat.mFormatID = kAudioFormatMPEGLayer1;	break;
		case MAD_LAYER_II:		mSourceFormat.mFormatID = kAudioFormatMPEGLayer2;	break;
		case MAD_LAYER_III:		mSourceFormat.mFormatID = kAudioFormatMPEGLayer3;	break;
	}
	
	mSourceFormat.mSampleRate			= mFrame.header.samplerate;
	mSourceFormat.mChannelsPerFrame		= MAD_NCHANNELS(&mFrame.header);
	mSourceFormat.mFramesPerPacket		= mSamplesPerMPEGFrame;

	// MAD_NCHANNELS always returns 1 or 2
	mChannelLayout.mChannelLayoutTag	= (1 == MAD_NCHANNELS(&mFrame.header) ? kAudioChannelLayoutTag_Mono : kAudioChannelLayoutTag_Stereo);

	// Look for a Xing header in the first MPEG frame
	// Reference http://www.codeproject.com/audio/MPEGAudioInfo.asp
	unsigned long magic;
	unsigned ancillaryBitsRemaining = mStream.anc_bitlen;
	if(32 > ancillaryBitsRemaining)
		goto frame2;
	
	/*unsigned long*/ magic = mad_bit_read(&mStream.anc_ptr, 32);
	ancillaryBitsRemaining -= 32;
	
	if('Xing' == magic || 'Info' == magic) {
		if(32 > ancillaryBitsRemaining)
			goto frame2;
		
		unsigned long flags = mad_bit_read(&mStream.anc_ptr, 32);
		ancillaryBitsRemaining -= 32;
		
		// 4 byte value containing total frames
		// For LAME-encoded MP3s, the number of MPEG frames in the file is one greater than this frame
		if(FRAMES_FLAG & flags) {
			if(32 > ancillaryBitsRemaining)
				goto frame2;
			
			unsigned long frames = mad_bit_read(&mStream.anc_ptr, 32);
			ancillaryBitsRemaining -= 32;
			
			mTotalMPEGFrames = static_cast<uint32_t>(frames);
			
			// Determine number of samples, discounting encoder delay and padding
			// Our concept of a frame is the same as CoreAudio's- one sample across all channels
			mTotalFrames = frames * mSamplesPerMPEGFrame;
		}
		
		// 4 byte value containing total bytes
		if(BYTES_FLAG & flags) {
			if(32 > ancillaryBitsRemaining)
				goto frame2;
			
			/*uint32_t bytes =*/ mad_bit_read(&mStream.anc_ptr, 32);
			ancillaryBitsRemaining -= 32;
		}
		
		// 100 bytes containing TOC information
		if(TOC_FLAG & flags) {
			if(8 * 100 > ancillaryBitsRemaining)
				goto frame2;
			
			for(unsigned i = 0; i < 100; ++i)
				mXingTOC[i] = mad_bit_read(&mStream.anc_ptr, 8);
			
			ancillaryBitsRemaining -= (8 * 100);
		}
		
		// 4 byte value indicating encoded vbr scale
		if(VBR_SCALE_FLAG & flags) {
			if(32 > ancillaryBitsRemaining)
				goto frame2;
			
			/*uint32_t vbrScale =*/ mad_bit_read(&mStream.anc_ptr, 32);
			ancillaryBitsRemaining -= 32;
		}
		
		mFoundXingHeader = true;
		
		// Loook for the LAME header next
		// http://gabriel.mp3-tech.org/mp3infotag.html				
		if(32 > ancillaryBitsRemaining)
			goto frame2;
		magic = mad_bit_read(&mStream.anc_ptr, 32);
		
		ancillaryBitsRemaining -= 32;
		
		if('LAME' == magic) {
			if(LAME_HEADER_SIZE > ancillaryBitsRemaining)
				goto frame2;
			
			/*unsigned char versionString [5 + 1];
			memset(versionString, 0, 6);*/
			
			for(unsigned i = 0; i < 5; ++i)
				/*versionString[i] =*/ mad_bit_read(&mStream.anc_ptr, 8);
			
			/*uint8_t infoTagRevision =*/ mad_bit_read(&mStream.anc_ptr, 4);
			/*uint8_t vbrMethod =*/ mad_bit_read(&mStream.anc_ptr, 4);
			
			/*uint8_t lowpassFilterValue =*/ mad_bit_read(&mStream.anc_ptr, 8);
			
			/*float peakSignalAmplitude =*/ mad_bit_read(&mStream.anc_ptr, 32);
			/*uint16_t radioReplayGain =*/ mad_bit_read(&mStream.anc_ptr, 16);
			/*uint16_t audiophileReplayGain =*/ mad_bit_read(&mStream.anc_ptr, 16);
			
			/*uint8_t encodingFlags =*/ mad_bit_read(&mStream.anc_ptr, 4);
			/*uint8_t athType =*/ mad_bit_read(&mStream.anc_ptr, 4);
			
			/*uint8_t lameBitrate =*/ mad_bit_read(&mStream.anc_ptr, 8);
			
			uint16_t encoderDelay = mad_bit_read(&mStream.anc_ptr, 12);
			uint16_t encoderPadding = mad_bit_read(&mStream.anc_ptr, 12);
								
			// Adjust encoderDelay and encoderPadding for MDCT/filterbank delays
			mEncoderDelay = encoderDelay + 528 + 1;
			mEncoderPadding = encoderPadding - (528 + 1);

			mTotalFrames -= (mEncoderDelay + mEncoderPadding);
			
			/*uint8_t misc =*/ mad_bit_read(&mStream.anc_ptr, 8);
			
			/*uint8_t mp3Gain =*/ mad_bit_read(&mStream.anc_ptr, 8);
			
			/*uint8_t unused =*/mad_bit_read(&mStream.anc_ptr, 2);
			/*uint8_t surroundInfo =*/ mad_bit_read(&mStream.anc_ptr, 3);
			/*uint16_t presetInfo =*/ mad_bit_read(&mStream.anc_ptr, 11);
			
			/*uint32_t musicGain =*/ mad_bit_read(&mStream.anc_ptr, 32);
			
			/*uint32_t musicCRC =*/ mad_bit_read(&mStream.anc_ptr, 32);
			
			/*uint32_t tagCRC =*/ mad_bit_read(&mStream.anc_ptr, 32);
			
			ancillaryBitsRemaining -= LAME_HEADER_SIZE;
			
			mFoundLAMEHeader = true;
		}
	}

	// If a Xing or LAME header was found, the total number of MPEG frames or sample frames is known
	if((mFoundXingHeader && 0 != mTotalFrames) || mFoundLAMEHeader)
		return true;
	
frame2:

	// Estimate the number of frames based on the file's size (for non-seekable input sources)
	if(estimateTotalFrames) {
		mTotalFrames = static_cast<SInt64>(static_cast<float>(mFrame.header.samplerate) * ((mInputSource->GetLength() /*- id3_length*/) / (mFrame.header.bitrate / 8.0)));

		// If the frame didn't contain a Xing header, synthesize the audio
		if(!mFoundXingHeader)
			SynthesizeMPEGFrame();
	}
	// Scan the file for each frame's header and keep count of the number of MPEG frames found
	else {
		// Decode the header for all MPEG frames
		while(DecodeMPEGFrame(true))
			;

		mTotalFrames = mSamplesPerMPEGFrame * mMPEGFramesDecoded;
		
		// Rewind to the beginning of the file
		if(!mInputSource->SeekToOffset(0))
			return false;
		
		// Reset decoder parameters
		mMPEGFramesDecoded			= 0;
		mCurrentFrame				= 0;
		mSamplesToSkipInNextFrame	= 0;
		mSamplesDecoded				= 0;
		
		mad_stream_buffer(&mStream, NULL, 0);
	}

	return true;
}

SInt64 MPEGDecoder::SeekToFrameApproximately(SInt64 frame)
{
	double	fraction	= static_cast<double>(frame) / this->GetTotalFrames();
	long	seekPoint	= 0;
	
	// If a Xing header was found, interpolate in TOC
	if(mFoundXingHeader) {
		double		percent		= 100 * fraction;
		uint32_t	firstIndex	= static_cast<uint32_t>(ceil(percent));
		
		if(99 < firstIndex)
			firstIndex = 99;
		
		double firstOffset	= mXingTOC[firstIndex];
		double secondOffset	= 256;
		
		if(99 > firstIndex)
			secondOffset = mXingTOC[firstIndex + 1];;
			
			double x = firstOffset + (secondOffset - firstOffset) * (percent - firstIndex);
			seekPoint = static_cast<long>((1.0 / 256.0) * x * mInputSource->GetLength());
	}
	else
		seekPoint = static_cast<long>(mInputSource->GetLength() * fraction);
	
	bool result = mInputSource->SeekToOffset(seekPoint);
	if(result) {
		mad_stream_buffer(&mStream, NULL, 0);
		
		// Reset frame count to prevent early termination of playback
		mMPEGFramesDecoded			= 0;
		mSamplesDecoded				= 0;
		mSamplesToSkipInNextFrame	= 0;
		
		mCurrentFrame				= frame;
	}
	
	// Right now it's only possible to return an approximation of the audio frame
	return (result ? frame : -1);
}

SInt64 MPEGDecoder::SeekToFrameAccurately(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < this->GetTotalFrames());
	
	// Brute force seeking is necessary since frame-accurate seeking is required
	
	// To seek to a frame earlier in the file, rewind to the beginning
	if(this->GetCurrentFrame() > frame) {
		if(!mInputSource->SeekToOffset(0))
			return -1;
		
		// Reset decoder parameters
		mMPEGFramesDecoded			= 0;
		mCurrentFrame				= 0;
		mSamplesToSkipInNextFrame	= 0;
		mSamplesDecoded				= 0;

		mad_stream_buffer(&mStream, NULL, 0);
	}
	// Mark any buffered audio as read
	else
		mCurrentFrame += mBufferList->mBuffers[0].mDataByteSize / sizeof(float);
	
	// Zero the buffers
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;
	
	for(;;) {
		// All requested frames were skipped or read
		if(mSamplesDecoded >= frame)
			break;

		// Decode and synthesize the next MPEG frame
		if(!DecodeMPEGFrame())
			break;

		SynthesizeMPEGFrame();

		mCurrentFrame += mSamplesPerMPEGFrame;

		// If the MPEG frame that was just decoded contains the desired frame, adjust the buffers
		// so the desired frame is first

		if(mSamplesDecoded >= frame) {
			UInt32 framesInBuffer = static_cast<UInt32>(mBufferList->mBuffers[0].mDataByteSize / sizeof(float));
			UInt32 framesToSkip = static_cast<UInt32>(mSamplesDecoded - frame);

			for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
				float *floatBuffer = static_cast<float *>(mBufferList->mBuffers[i].mData);
				memmove(floatBuffer, floatBuffer + framesToSkip, (framesInBuffer - framesToSkip) * sizeof(float));
			
				mBufferList->mBuffers[i].mDataByteSize -= static_cast<UInt32>(framesToSkip * sizeof(float));
			}
			
			mCurrentFrame -= framesToSkip;
		}
	}
	
	return mCurrentFrame;
}
