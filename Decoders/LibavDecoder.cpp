/*
 * Copyright (c) 2013 - 2018 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "LibavDecoder.h"
#include "AudioBufferList.h"
#include "AudioChannelLayout.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/mathematics.h>
}


namespace {

#define BUF_SIZE 4096
#define ERRBUF_SIZE 512

	void RegisterLibavDecoder() __attribute__ ((constructor));
	void RegisterLibavDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::LibavDecoder>(-75);
	}

	#pragma mark Initialization

	void SetupLibav() __attribute__ ((constructor));
	void SetupLibav()
	{
		// Register codecs and disable logging
		av_register_all();
		av_log_set_level(AV_LOG_QUIET);
	}

	#pragma mark Callbacks

	int my_read_packet(void *opaque, uint8_t *buf, int buf_size)
	{
		assert(nullptr != opaque);

		SFB::Audio::LibavDecoder *decoder = static_cast<SFB::Audio::LibavDecoder *>(opaque);
		return (int)decoder->GetInputSource().Read(buf, buf_size);
	}

	int64_t my_seek(void *opaque, int64_t offset, int whence)
	{
		assert(nullptr != opaque);

		SFB::Audio::LibavDecoder *decoder = static_cast<SFB::Audio::LibavDecoder *>(opaque);
		SFB::InputSource& inputSource = decoder->GetInputSource();

		if(!inputSource.SupportsSeeking())
			return -1;

		// Adjust offset as required
		switch(whence) {
			case SEEK_SET:
				/* offset remains unchanged */
				break;
			case SEEK_CUR:
				offset += inputSource.GetOffset();
				break;
			case SEEK_END:
				offset += inputSource.GetLength();
				break;
			case AVSEEK_SIZE:
				return inputSource.GetLength();
				/* break; */
		}

		if(!inputSource.SeekToOffset(offset))
			return -1;

		return inputSource.GetOffset();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::LibavDecoder::CreateSupportedFileExtensions()
{
	CFMutableArrayRef supportedExtensions = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	// Loop through each input format
	AVInputFormat *inputFormat = nullptr;
	while((inputFormat = av_iformat_next(inputFormat))) {
		if(inputFormat->extensions) {
			SFB::CFString extensions(inputFormat->extensions, kCFStringEncodingUTF8);
			if(extensions) {
				SFB::CFArray extensionsArray(CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, extensions, CFSTR(",")));
				if(extensionsArray)
					CFArrayAppendArray(supportedExtensions, extensionsArray, CFRangeMake(0, CFArrayGetCount(extensionsArray)));
			}
		}
	}

	return supportedExtensions;
}

CFArrayRef SFB::Audio::LibavDecoder::CreateSupportedMIMETypes()
{
	CFMutableArrayRef supportedMIMETypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	// Loop through each input format
	AVInputFormat *inputFormat = nullptr;
	while((inputFormat = av_iformat_next(inputFormat))) {
		if(inputFormat->extensions) {
			SFB::CFString mimeTypes(inputFormat->mime_type, kCFStringEncodingUTF8);
			if(mimeTypes) {
				SFB::CFArray mimeTypesArray(CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, mimeTypes, CFSTR(",")));
				if(mimeTypesArray)
					CFArrayAppendArray(supportedMIMETypes, mimeTypesArray, CFRangeMake(0, CFArrayGetCount(mimeTypesArray)));
			}
		}
	}

	return supportedMIMETypes;
}

bool SFB::Audio::LibavDecoder::HandlesFilesWithExtension(CFStringRef extension)
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

	CFRelease(supportedExtensions);
	supportedExtensions = nullptr;

	return extensionIsSupported;
}

bool SFB::Audio::LibavDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	CFArrayRef supportedMIMETypes = CreateSupportedMIMETypes();

	if(nullptr == supportedMIMETypes)
		return false;

	bool mimeTypeIsSupported = false;

	CFIndex numberOfSupportedMIMETypes = CFArrayGetCount(supportedMIMETypes);
	for(CFIndex currentIndex = 0; currentIndex < numberOfSupportedMIMETypes; ++currentIndex) {
		CFStringRef currentMIMEType = (CFStringRef)CFArrayGetValueAtIndex(supportedMIMETypes, currentIndex);
		if(kCFCompareEqualTo == CFStringCompare(mimeType, currentMIMEType, kCFCompareCaseInsensitive)) {
			mimeTypeIsSupported = true;
			break;
		}
	}

	CFRelease(supportedMIMETypes);
	supportedMIMETypes = nullptr;

	return mimeTypeIsSupported;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LibavDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new LibavDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::LibavDecoder::LibavDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mStreamIndex(-1), mCurrentFrame(0)
{}

#pragma mark Functionality

bool SFB::Audio::LibavDecoder::_Open(CFErrorRef *error)
{
	auto ioContext = unique_AVIOContext_ptr(avio_alloc_context((unsigned char *)av_malloc(BUF_SIZE), BUF_SIZE, 0, this, my_read_packet, nullptr, my_seek),
											[](AVIOContext *context) { av_free(context); });

	auto formatContext = unique_AVFormatContext_ptr(avformat_alloc_context(),
													[](AVFormatContext *context) { avformat_free_context(context); });
	formatContext->pb = ioContext.get();

	char filename [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mInputSource->GetURL(), false, (UInt8 *)filename, PATH_MAX))
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "CFURLGetFileSystemRepresentation failed");

	auto rawFormatContext = formatContext.get();
	int result = avformat_open_input(&rawFormatContext, filename, nullptr, nullptr);
	if(0 != result) {
		char errbuf [ERRBUF_SIZE];
		if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avformat_open_input failed: " << errbuf);
		}
		else
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avformat_open_input failed: " << result);

		if(nullptr != error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Retrieve stream information
    if(0 > avformat_find_stream_info(formatContext.get(), nullptr)) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "Could not find stream information");

		if(nullptr != error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
    }

	// Use the best audio stream present in the file
	AVCodec *decoder = nullptr;
	result = av_find_best_stream(formatContext.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
	if(0 > result) {
		char errbuf [ERRBUF_SIZE];
		if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "av_find_best_stream failed: " << errbuf);
		}
		else
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "av_find_best_stream failed: " << result);

		if(nullptr != error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	mStreamIndex = result;

	auto codecContext = unique_AVCodecContext_ptr(avcodec_alloc_context3(decoder),
												  [](AVCodecContext *context) { avcodec_free_context(&context); });
	if(!codecContext) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_alloc_context3 failed");

		if(nullptr != error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	result = avcodec_parameters_to_context(codecContext.get(), formatContext->streams[mStreamIndex]->codecpar);

	result = avcodec_open2(codecContext.get(), decoder, nullptr);
	if(0 != result) {
		char errbuf [ERRBUF_SIZE];
		if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_open2 failed: " << errbuf);
		}
		else
			LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_open2 failed: " << result);

		if(nullptr != error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The format of the file “%@” was not recognized."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("File Format Not Recognized"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Generate PCM output
	mFormat.mFormatID			= kAudioFormatLinearPCM;

	mFormat.mSampleRate			= formatContext->streams[mStreamIndex]->codecpar->sample_rate;
	mFormat.mChannelsPerFrame	= (UInt32)formatContext->streams[mStreamIndex]->codecpar->channels;

	switch(formatContext->streams[mStreamIndex]->codecpar->format) {

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

	mFormat.mReserved					= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= 'LBAV';

	mSourceFormat.mSampleRate			= formatContext->streams[mStreamIndex]->codecpar->sample_rate;;
	mSourceFormat.mChannelsPerFrame		= (UInt32)formatContext->streams[mStreamIndex]->codecpar->channels;

	mSourceFormat.mFormatFlags			= mFormat.mFormatFlags;
	mSourceFormat.mBitsPerChannel		= mFormat.mBitsPerChannel;

	switch(formatContext->streams[mStreamIndex]->codecpar->channel_layout) {
		case AV_CH_LAYOUT_MONO:
			mChannelLayout = SFB::Audio::ChannelLayout::Mono;
			break;

		case AV_CH_LAYOUT_STEREO:
			mChannelLayout = SFB::Audio::ChannelLayout::Stereo;
			break;

//		default:
//			mChannelLayout = SFB::Audio::ChannelLayout::ChannelLayoutWithBitmap(formatContext->streams[mStreamIndex]->codecpar->channel_layout);
//			break;
	}

	// TODO: Determine max frame size
	if(!mBufferList.Allocate(mFormat, 4096)) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Decoder.Libav", "Unable to allocate memory")

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}

	mFrame = unique_AVFrame_ptr(av_frame_alloc(),
								[](AVFrame *f) { av_frame_free(&f); });

	mIOContext = std::move(ioContext);
	mFormatContext = std::move(formatContext);
	mCodecContext = std::move(codecContext);

	return true;
}

bool SFB::Audio::LibavDecoder::_Close(CFErrorRef */*error*/)
{
	mStreamIndex = -1;

	mFrame.reset();
	mIOContext.reset();
	mFormatContext.reset();

	mBufferList.Deallocate();

	return true;
}

SFB::CFString SFB::Audio::LibavDecoder::_GetSourceFormatDescription() const
{
	auto codecDescriptor = avcodec_descriptor_get(mFormatContext->streams[mStreamIndex]->codecpar->codec_id);

	return CFString(nullptr,
					CFSTR("%s, %u channels, %u Hz"),
					codecDescriptor ? codecDescriptor->long_name : "",
					mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

SInt64 SFB::Audio::LibavDecoder::_GetTotalFrames() const
{
	if(mFormatContext->streams[mStreamIndex]->nb_frames)
		return mFormatContext->streams[mStreamIndex]->nb_frames;

	return av_rescale(mFormatContext->streams[mStreamIndex]->duration, mFormatContext->streams[mStreamIndex]->time_base.num, mFormatContext->streams[mStreamIndex]->time_base.den) * (SInt64)mFormat.mSampleRate;
}

UInt32 SFB::Audio::LibavDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
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

		AVPacket packet;
		av_init_packet(&packet);
		packet.data = nullptr;
		packet.size = 0;

		// Read a single frame from the file
		int result = av_read_frame(mFormatContext.get(), &packet);
		if(0 > result) {
			// EOF reached
			if(AVERROR_EOF == result) {
				if(CODEC_CAP_DELAY & mCodecContext->codec->capabilities) {
					// TODO: Flush buffer
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

//		av_frame_unref(mFrame.get());

		// Send the packet with the compressed data to the decoder
		result = avcodec_send_packet(mCodecContext.get(), &packet);
		if(0 != result) {
			char errbuf [ERRBUF_SIZE];
			if(0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
				LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_send_packet failed: " << errbuf);
			}
			else
				LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.Libav", "avcodec_send_packet failed: " << result);

			break;
		}

		// Read the output frames
		while(result >= 0) {
			result = avcodec_receive_frame(mCodecContext.get(), mFrame.get());
			if(AVERROR(EAGAIN) == result || AVERROR_EOF == result)
				break;

			// Planar formats are not interleaved
			if(av_sample_fmt_is_planar(mCodecContext->sample_fmt)) {
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

		av_packet_unref(&packet);
	}

	mCurrentFrame += framesRead;

	return framesRead;
}

SInt64 SFB::Audio::LibavDecoder::_SeekToFrame(SInt64 frame)
{
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

	avcodec_flush_buffers(mCodecContext.get());

	mCurrentFrame = frame;
	return mCurrentFrame;
}
