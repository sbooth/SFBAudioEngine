/*
 *  Copyright (C) 2013 Stephen F. Booth <me@sbooth.org>
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

#include "LibavDecoder.h"
#include "AllocateABL.h"
#include "DeallocateABL.h"
#include "CreateChannelLayout.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/mathematics.h>
}


#define BUF_SIZE 4096
#define ERRBUF_SIZE 512

static void RegisterLibavDecoder() __attribute__ ((constructor));
static void RegisterLibavDecoder()
{
	AudioDecoder::RegisterSubclass<LibavDecoder>(-1);
}

#pragma mark Initialization

static void Setuplibav() __attribute__ ((constructor));
static void Setuplibav()
{
	// Register codecs and disable logging
	av_register_all();
	av_log_set_level(AV_LOG_QUIET);
}

#pragma mark Callbacks

static int
my_read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	assert(nullptr != opaque);

	LibavDecoder *decoder = static_cast<LibavDecoder *>(opaque);
	return (int)decoder->GetInputSource()->Read(buf, buf_size);
}

static int64_t
my_seek(void *opaque, int64_t offset, int whence)
{
	assert(nullptr != opaque);

	LibavDecoder *decoder = static_cast<LibavDecoder *>(opaque);
	InputSource *inputSource = decoder->GetInputSource();

	if(!inputSource->SupportsSeeking())
		return -1;

	// Adjust offset as required
	switch(whence) {
		case SEEK_SET:		/* offset remains unchanged */			break;
		case SEEK_CUR:		offset += inputSource->GetOffset();		break;
		case SEEK_END:		offset += inputSource->GetLength();		break;
		case AVSEEK_SIZE:	return inputSource->GetLength();		/* break; */
	}

	if(!inputSource->SeekToOffset(offset))
		return -1;

	return inputSource->GetOffset();
}

#pragma mark Static Methods

CFArrayRef LibavDecoder::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	// Loop through each input format
	AVInputFormat *inputFormat = nullptr;
	while((inputFormat = av_iformat_next(inputFormat))) {
		if(inputFormat->extensions) {
			SFB::CFString extensions = CFStringCreateWithCString(kCFAllocatorDefault, inputFormat->extensions, kCFStringEncodingUTF8);
			if(extensions) {
				SFB::CFArray extensionsArray = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, extensions, CFSTR(","));
				if(extensionsArray)
					CFArrayAppendArray(supportedExtensions, extensionsArray, CFRangeMake(0, CFArrayGetCount(extensionsArray)));
			}
		}
	}

	return supportedExtensions;
}

CFArrayRef LibavDecoder::CreateSupportedMIMETypes()
{
	return CFArrayCreate(kCFAllocatorDefault, nullptr, 0, &kCFTypeArrayCallBacks);
}

bool LibavDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	CFArrayRef supportedExtensions = CreateSupportedFileExtensions();

	if(nullptr == supportedExtensions)
		return false;

	bool extensionIsSupported = false;

	CFIndex numberOfSupportedExtensions = CFArrayGetCount(supportedExtensions);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedExtensions; ++currentIndex) {
		CFStringRef currentExtension = (CFStringRef)CFArrayGetValueAtIndex(supportedExtensions, currentIndex);
		if(kCFCompareEqualTo == CFStringCompare(extension, currentExtension, kCFCompareCaseInsensitive)) {
			extensionIsSupported = true;
			break;
		}
	}

	CFRelease(supportedExtensions), supportedExtensions = nullptr;

	return extensionIsSupported;
}

bool LibavDecoder::HandlesMIMEType(CFStringRef /*mimeType*/)
{
	return false;
}

AudioDecoder * LibavDecoder::CreateDecoder(InputSource *inputSource)
{
	return new LibavDecoder(inputSource);
}

#pragma mark Creation and Destruction

LibavDecoder::LibavDecoder(InputSource *inputSource)
	: AudioDecoder(inputSource), mBufferList(nullptr), mIOContext(nullptr), mFrame(nullptr), mFormatContext(nullptr), mStreamIndex(0), mCurrentFrame(0)
{}

LibavDecoder::~LibavDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool LibavDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Libav", "Open() called on an AudioDecoder that is already open");
		return true;
	}

	// Ensure the input source is open
	if(!mInputSource->IsOpen() && !mInputSource->Open(error))
		return false;

	auto ioContext = std::unique_ptr<AVIOContext, std::function<void (AVIOContext *)>>(avio_alloc_context((unsigned char *)av_malloc(BUF_SIZE), BUF_SIZE, 0, this, my_read_packet, nullptr, my_seek),
																				   [](AVIOContext *context) { av_free(context); });

	auto formatContext = std::unique_ptr<AVFormatContext, std::function<void (AVFormatContext *)>>(avformat_alloc_context(), [](AVFormatContext *context) { avformat_free_context(context); });
	formatContext->pb = ioContext.get();

	auto rawFormatContext = formatContext.get();
	int result = avformat_open_input(&rawFormatContext, nullptr, nullptr, nullptr);
	if(0 != result) {
		char errbuf [ERRBUF_SIZE];
		if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avformat_open_input failed: " << errbuf);
		}
		else
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avformat_open_input failed: " << result);

		if(nullptr != error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("File Format Not Recognized"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");

			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Retrieve stream information
    if(0 > avformat_find_stream_info(formatContext.get(), nullptr)) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "Could not find stream information");

		if(nullptr != error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("File Format Not Recognized"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");

			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
    }

	// Use the best audio stream present in the file
	AVCodec *codec = nullptr;
	result = av_find_best_stream(formatContext.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
	if(0 > result) {
		char errbuf [ERRBUF_SIZE];
		if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "av_find_best_stream failed: " << errbuf);
		}
		else
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "av_find_best_stream failed: " << result);

		if(nullptr != error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("File Format Not Recognized"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");

			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	mStreamIndex = result;

	auto stream = formatContext->streams[mStreamIndex];
	auto codecContext = stream->codec;

	AVCodec *decoder = avcodec_find_decoder(codecContext->codec_id);
	if(!decoder) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_find_decoder(" << codecContext->codec_id << ") failed");

		if(nullptr != error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("File Format Not Recognized"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");

			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	result = avcodec_open2(codecContext, decoder, nullptr);
	if(0 != result) {
		char errbuf [ERRBUF_SIZE];
		if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_open2 failed: " << errbuf);
		}
		else
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_open2 failed: " << result);

		if(nullptr != error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("File Format Not Recognized"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");

			*error = CreateErrorForURL(AudioDecoderErrorDomain, AudioDecoderInputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Generate PCM output
	mFormat.mFormatID			= kAudioFormatLinearPCM;

	mFormat.mSampleRate			= codecContext->sample_rate;
	mFormat.mChannelsPerFrame	= (UInt32)codecContext->channels;

	switch(codecContext->sample_fmt) {

		case AV_SAMPLE_FMT_U8P:
			mFormat.mFormatFlags		= kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
			mFormat.mBitsPerChannel		= 8;
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_U8:
			mFormat.mFormatFlags		= kAudioFormatFlagIsPacked;
			mFormat.mBitsPerChannel		= 8;
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_S16P:
			mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
			mFormat.mBitsPerChannel		= 16;
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_S16:
			mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
			mFormat.mBitsPerChannel		= 16;
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_S32P:
			mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
			mFormat.mBitsPerChannel		= 32;
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_S32:
			mFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
			mFormat.mBitsPerChannel		= 32;
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_FLTP:
			mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
			mFormat.mBitsPerChannel		= 8 * sizeof(float);
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_FLT:
			mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;
			mFormat.mBitsPerChannel		= 8 * sizeof(float);
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_DBLP:
			mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
			mFormat.mBitsPerChannel		= 8 * sizeof(double);
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8);
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		case AV_SAMPLE_FMT_DBL:
			mFormat.mFormatFlags		= kAudioFormatFlagsNativeFloatPacked;
			mFormat.mBitsPerChannel		= 8 * sizeof(double);
			mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
			mFormat.mFramesPerPacket	= 1;
			mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;
			break;

		default:
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "Unknown sample format")
			break;
	}

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= 'LBAV';

	mSourceFormat.mSampleRate			= codecContext->sample_rate;
	mSourceFormat.mChannelsPerFrame		= (UInt32)codecContext->channels;

	mSourceFormat.mFormatFlags			= mFormat.mFormatFlags;
	mSourceFormat.mBitsPerChannel		= mFormat.mBitsPerChannel;

	// TODO: Determine max frame size
	mBufferList = AllocateABL(mFormat, 4096);
	if(nullptr == mBufferList) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}

	// Allocate the libav frame
	auto frame = std::unique_ptr<AVFrame, std::function<void (AVFrame *)>>(avcodec_alloc_frame(), [](AVFrame *f) { av_free(f); });
	mFrame = std::move(frame);

	mIOContext = std::move(ioContext);
	mFormatContext = std::move(formatContext);

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	mIsOpen = true;
	return true;
}

bool LibavDecoder::Close(CFErrorRef */*error*/)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.Libav", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}

	mStreamIndex = -1;

	mFrame.reset();
	mIOContext.reset();
	mFormatContext.reset();

	mIsOpen = false;
	return true;
}

CFStringRef LibavDecoder::CreateSourceFormatDescription() const
{
	if(!IsOpen())
		return nullptr;

	return CFStringCreateWithFormat(kCFAllocatorDefault,
									nullptr,
									CFSTR("%s, %u channels, %u Hz"),
									mFormatContext->streams[mStreamIndex]->codec->codec->long_name,
									mSourceFormat.mChannelsPerFrame,
									(unsigned int)mSourceFormat.mSampleRate);
}

SInt64 LibavDecoder::GetTotalFrames() const
{
	if(!IsOpen())
		return -1;

	if(mFormatContext->streams[mStreamIndex]->nb_frames)
		return mFormatContext->streams[mStreamIndex]->nb_frames;

	return av_rescale(mFormatContext->streams[mStreamIndex]->duration, mFormatContext->streams[mStreamIndex]->time_base.num, mFormatContext->streams[mStreamIndex]->time_base.den) * (SInt64)mFormat.mSampleRate;
}

SInt64 LibavDecoder::GetCurrentFrame() const
{
	if(!IsOpen())
		return -1;

	return mCurrentFrame;
}

SInt64 LibavDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;

	int64_t timestamp = av_rescale(frame / (SInt64)mFormat.mSampleRate, mFormatContext->streams[mStreamIndex]->time_base.den, mFormatContext->streams[mStreamIndex]->time_base.num);
	int result = av_seek_frame(mFormatContext.get(), mStreamIndex, timestamp, 0);
	if(0 > result) {
		char errbuf [ERRBUF_SIZE];
		if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "av_seek_frame failed: " << errbuf);
		}
		else
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "av_seek_frame failed: " << result);

		return -1;
	}

	avcodec_flush_buffers(mFormatContext->streams[mStreamIndex]->codec);

	mCurrentFrame = frame;
	return mCurrentFrame;
}

UInt32 LibavDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;

	UInt32 framesRead = 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	for(;;) {
		UInt32 bytesRemaining	= (frameCount - framesRead) * mFormat.mBytesPerFrame;
		UInt32 bytesToSkip		= bufferList->mBuffers[0].mDataByteSize;
		UInt32 bytesInBuffer	= mBufferList->mBuffers[0].mDataByteSize;

		UInt32 bytesToCopy		= std::min(bytesInBuffer, bytesRemaining);

		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			unsigned char *buffer = (unsigned char *)bufferList->mBuffers[i].mData;
			memcpy(buffer + bytesToSkip, mBufferList->mBuffers[i].mData, bytesToCopy);
			bufferList->mBuffers[i].mDataByteSize += bytesToCopy;

			// Move remaining data in buffer to beginning
			if(bytesToCopy != bytesInBuffer) {
				buffer = (unsigned char *)mBufferList->mBuffers[i].mData;
				memmove(buffer, buffer + bytesToCopy, (bytesInBuffer - bytesToCopy));
			}

			mBufferList->mBuffers[i].mDataByteSize -= bytesToCopy;
		}

		framesRead += (bytesToCopy / mFormat.mBytesPerFrame);

		// All requested frames were read
		if(framesRead == frameCount)
			break;

		// Read a single frame from the file
		AVPacket packet;
		av_init_packet(&packet);

		int result = av_read_frame(mFormatContext.get(), &packet);
		if(0 != result) {
			// EOF reached
			if(AVERROR_EOF == result) {
				if(CODEC_CAP_DELAY & mFormatContext->streams[mStreamIndex]->codec->codec->capabilities) {
					// TODO: Flush buffer using avcodec_decode_audio4
				}

			}
			else {
				char errbuf [ERRBUF_SIZE];
				if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
					LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "av_read_frame failed: " << errbuf);
				}
				else
					LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "av_read_frame failed: " << result);
			}

			break;
		}

		if(packet.stream_index != mStreamIndex)
			continue;

		// Decode the audio
		avcodec_get_frame_defaults(mFrame.get());

		// Feed the packet to the decoder unitl no frames are returned or the input is fully consumed
		while(0 < packet.size) {
			int gotFrame = 0;
			int bytesConsumed = avcodec_decode_audio4(mFormatContext->streams[mStreamIndex]->codec, mFrame.get(), &gotFrame, &packet);

			if(0 > bytesConsumed) {
				char errbuf [ERRBUF_SIZE];
				if(0 == av_strerror(bytesConsumed, errbuf, ERRBUF_SIZE)) {
					LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_decode_audio4 failed: " << errbuf);
				}
				else
					LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_decode_audio4 failed: " << result);

				packet.size = 0;

				continue;
			}
			else if(gotFrame) {
				// Planar formats are not interleaved
				if(av_sample_fmt_is_planar(mFormatContext->streams[mStreamIndex]->codec->sample_fmt)) {
					for(UInt32 bufferIndex = 0; bufferIndex < mBufferList->mNumberBuffers; ++bufferIndex) {
						memcpy(mBufferList->mBuffers[bufferIndex].mData, mFrame->extended_data[bufferIndex], (size_t)mFrame->linesize[0]);
						mBufferList->mBuffers[bufferIndex].mDataByteSize = (UInt32)mFrame->linesize[0];
						mBufferList->mBuffers[bufferIndex].mNumberChannels = 1;
					}
				}
				else {
					memcpy(mBufferList->mBuffers[0].mData, mFrame->extended_data[0], (size_t)mFrame->linesize[0]);
					mBufferList->mBuffers[0].mDataByteSize = (UInt32)mFrame->linesize[0];
					mBufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;
				}
			}

			// Adjust packet size and buffer
			packet.data += bytesConsumed;
			packet.size -= bytesConsumed;
		}

		av_free_packet(&packet);
	}

	mCurrentFrame += framesRead;

	return framesRead;
}
