/*
 * Copyright (c) 2018 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>
#include <array>

#include <os/log.h>

#include <Accelerate/Accelerate.h>

#include "CFErrorUtilities.h"
#include "DSDPCMDecoder.h"

#define DSD_FRAMES_PER_PCM_FRAME 8

namespace {

	// Bit reversal lookup table from http://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable
	static const uint8_t sBitReverseTable256 [256] =
	{
#   define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#   define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#   define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
		R6(0), R6(2), R6(1), R6(3)
	};

#pragma mark Begin DSD2PCM

	// The code performing the DSD to PCM conversion was modified from dsd2pcm.c:

	/*

	 Copyright 2009, 2011 Sebastian Gesemann. All rights reserved.

	 Redistribution and use in source and binary forms, with or without modification, are
	 permitted provided that the following conditions are met:

	 1. Redistributions of source code must retain the above copyright notice, this list of
	 conditions and the following disclaimer.

	 2. Redistributions in binary form must reproduce the above copyright notice, this list
	 of conditions and the following disclaimer in the documentation and/or other materials
	 provided with the distribution.

	 THIS SOFTWARE IS PROVIDED BY SEBASTIAN GESEMANN ''AS IS'' AND ANY EXPRESS OR IMPLIED
	 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	 FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEBASTIAN GESEMANN OR
	 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
	 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
	 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	 ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	 The views and conclusions contained in the software and documentation are those of the
	 authors and should not be interpreted as representing official policies, either expressed
	 or implied, of Sebastian Gesemann.

	 */

#define HTAPS    48             /* number of FIR constants */
#define FIFOSIZE 16             /* must be a power of two */
#define FIFOMASK (FIFOSIZE-1)   /* bit mask for FIFO offsets */
#define CTABLES ((HTAPS+7)/8)   /* number of "8 MACs" lookup tables */

#if FIFOSIZE*8 < HTAPS*2
#  error "FIFOSIZE too small"
#endif

	/*
	 * Properties of this 96-tap lowpass filter when applied on a signal
	 * with sampling rate of 44100*64 Hz:
	 *
	 * () has a delay of 17 microseconds.
	 *
	 * () flat response up to 48 kHz
	 *
	 * () if you downsample afterwards by a factor of 8, the
	 *    spectrum below 70 kHz is practically alias-free.
	 *
	 * () stopband rejection is about 160 dB
	 *
	 * The coefficient tables ("ctables") take only 6 Kibi Bytes and
	 * should fit into a modern processor's fast cache.
	 */

	/*
	 * The 2nd half (48 coeffs) of a 96-tap symmetric lowpass filter
	 */
	static const double htaps[HTAPS] = {
		0.09950731974056658,
		0.09562845727714668,
		0.08819647126516944,
		0.07782552527068175,
		0.06534876523171299,
		0.05172629311427257,
		0.0379429484910187,
		0.02490921351762261,
		0.0133774746265897,
		0.003883043418804416,
		-0.003284703416210726,
		-0.008080250212687497,
		-0.01067241812471033,
		-0.01139427235000863,
		-0.0106813877974587,
		-0.009007905078766049,
		-0.006828859761015335,
		-0.004535184322001496,
		-0.002425035959059578,
		-0.0006922187080790708,
		0.0005700762133516592,
		0.001353838005269448,
		0.001713709169690937,
		0.001742046839472948,
		0.001545601648013235,
		0.001226696225277855,
		0.0008704322683580222,
		0.0005381636200535649,
		0.000266446345425276,
		7.002968738383528e-05,
		-5.279407053811266e-05,
		-0.0001140625650874684,
		-0.0001304796361231895,
		-0.0001189970287491285,
		-9.396247155265073e-05,
		-6.577634378272832e-05,
		-4.07492895872535e-05,
		-2.17407957554587e-05,
		-9.163058931391722e-06,
		-2.017460145032201e-06,
		1.249721855219005e-06,
		2.166655190537392e-06,
		1.930520892991082e-06,
		1.319400334374195e-06,
		7.410039764949091e-07,
		3.423230509967409e-07,
		1.244182214744588e-07,
		3.130441005359396e-08
	};

	static float ctables[CTABLES][256];

	void dsd2pcm_precalc()
	{
		int t, e, m, k;
		double acc;
		for (t=0; t<CTABLES; ++t) {
			k = HTAPS - t*8;
			if (k>8) k=8;
			for (e=0; e<256; ++e) {
				acc = 0.0;
				for (m=0; m<k; ++m) {
					acc += (((e >> (7-m)) & 1)*2-1) * htaps[t*8+m];
				}
				ctables[CTABLES-1-t][e] = (float)acc;
			}
		}
	}

	struct dsd2pcm_ctx
	{
		unsigned char fifo[FIFOSIZE];
		unsigned fifopos;
	};

	/**
	 * resets the internal state for a fresh new stream
	 */
	void dsd2pcm_reset(dsd2pcm_ctx *ptr)
	{
		int i;
		for (i=0; i<FIFOSIZE; ++i)
			ptr->fifo[i] = 0x69; /* my favorite silence pattern */
		ptr->fifopos = 0;
		/* 0x69 = 01101001
		 * This pattern "on repeat" makes a low energy 352.8 kHz tone
		 * and a high energy 1.0584 MHz tone which should be filtered
		 * out completely by any playback system --> silence
		 */
	}

	/**
	 * initializes a "dsd2pcm engine" for one channel
	 * (allocates memory)
	 */
	dsd2pcm_ctx * dsd2pcm_init()
	{
		dsd2pcm_ctx *ptr;
		ptr = (dsd2pcm_ctx *) malloc(sizeof(dsd2pcm_ctx));
		if (ptr) dsd2pcm_reset(ptr);
		return ptr;
	}

	/**
	 * deinitializes a "dsd2pcm engine"
	 * (releases memory, don't forget!)
	 */
	void dsd2pcm_destroy(dsd2pcm_ctx *ptr)
	{
		free(ptr);
	}

	/**
	 * clones the context and returns a pointer to the
	 * newly allocated copy
	 */
	dsd2pcm_ctx * dsd2pcm_clone(dsd2pcm_ctx *ptr)
	{
		dsd2pcm_ctx *p2;
		p2 = (dsd2pcm_ctx *) malloc(sizeof(dsd2pcm_ctx));
		if (p2) {
			memcpy(p2,ptr,sizeof(dsd2pcm_ctx));
		}
		return p2;
	}

	/**
	 * "translates" a stream of octets to a stream of floats
	 * (8:1 decimation)
	 * @param ctx -- pointer to abstract context (buffers)
	 * @param samples -- number of octets/samples to "translate"
	 * @param src -- pointer to first octet (input)
	 * @param src_stride -- src pointer increment
	 * @param lsbitfirst -- bitorder, 0=msb first, 1=lsbfirst
	 * @param dst -- pointer to first float (output)
	 * @param dst_stride -- dst pointer increment
	 */
	void dsd2pcm_translate(dsd2pcm_ctx *ptr, size_t samples, const unsigned char *src, ptrdiff_t src_stride, int lsbf, float *dst, ptrdiff_t dst_stride)
	{
		unsigned ffp;
		unsigned i;
		unsigned bite1, bite2;
		unsigned char* p;
		double acc;
		ffp = ptr->fifopos;
		lsbf = lsbf ? 1 : 0;
		while (samples-- > 0) {
			bite1 = *src & 0xFFu;
			if (lsbf) bite1 = sBitReverseTable256[bite1];
			ptr->fifo[ffp] = (unsigned char)bite1; src += src_stride;
			p = ptr->fifo + ((ffp-CTABLES) & FIFOMASK);
			*p = sBitReverseTable256[*p & 0xFF];
			acc = 0;
			for (i=0; i<CTABLES; ++i) {
				bite1 = ptr->fifo[(ffp              -i) & FIFOMASK] & 0xFF;
				bite2 = ptr->fifo[(ffp-(CTABLES*2-1)+i) & FIFOMASK] & 0xFF;
				acc += ctables[i][bite1] + ctables[i][bite2];
			}
			*dst = (float)acc; dst += dst_stride;
			ffp = (ffp + 1) & FIFOMASK;
		}
		ptr->fifopos = ffp;
	}

#pragma mark End DSD2PCM

	// Support DSD64 (64x the CD sample rate of 44.1 KHz)
	static const std::array<Float64, 1> sSupportedSampleRates = { {2822400} };

#pragma mark Initialization

	void SetupDSD2PCM() __attribute__ ((constructor));
	void SetupDSD2PCM()
	{
		dsd2pcm_precalc();
	}

}

#pragma mark DXD

namespace SFB {
	namespace Audio {
		class DSDPCMDecoder::DXD {
		public:
			DXD()
				: handle(dsd2pcm_init())
			{
				if(nullptr == handle)
					throw std::bad_alloc();
			}

			DXD(DXD const& x)
				: handle(dsd2pcm_clone(x.handle))
			{
				if(nullptr == handle)
					throw std::bad_alloc();
			}

			~DXD()
			{
				dsd2pcm_destroy(handle);
			}

			friend void Swap(DXD& a, DXD& b)
			{
				std::swap(a.handle, b.handle);
			}

			DXD& operator=(DXD x)
			{
				Swap(*this, x);
				return *this;
			}

			void Translate(size_t samples, const unsigned char *src, ptrdiff_t src_stride, bool lsbitfirst, float *dst, ptrdiff_t dst_stride)
			{
				dsd2pcm_translate(handle, samples, src, src_stride, lsbitfirst, dst, dst_stride);
			}

		private:
			dsd2pcm_ctx *handle;
		};

	}
}

#pragma mark Factory Methods

SFB::Audio::Decoder::unique_ptr SFB::Audio::DSDPCMDecoder::CreateForURL(CFURLRef url, CFErrorRef *error)
{
	return CreateForInputSource(InputSource::CreateForURL(url, 0, error), error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::DSDPCMDecoder::CreateForInputSource(InputSource::unique_ptr inputSource, CFErrorRef *error)
{
	if(!inputSource)
		return nullptr;

	return CreateForDecoder(Decoder::CreateForInputSource(std::move(inputSource), error), error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::DSDPCMDecoder::CreateForDecoder(unique_ptr decoder, CFErrorRef *error)
{
#pragma unused(error)

	if(!decoder)
		return nullptr;

	return unique_ptr(new DSDPCMDecoder(std::move(decoder)));
}

// 6 dBFS gain -> powf(10.f, 6.f / 20.f) -> 0x1.fec984p+0 (approximately 1.99526231496888)
SFB::Audio::DSDPCMDecoder::DSDPCMDecoder(Decoder::unique_ptr decoder)
	: mDecoder(std::move(decoder)), mLinearGain(0x1.fec984p+0)
{
	assert(nullptr != mDecoder);
}

bool SFB::Audio::DSDPCMDecoder::_Open(CFErrorRef *error)
{
	if(!mDecoder->IsOpen() && !mDecoder->Open(error))
		return false;

	const auto& decoderFormat = mDecoder->GetFormat();

	if(!decoderFormat.IsDSD()) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid DSD file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a DSD file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	if(std::end(sSupportedSampleRates) == std::find(std::begin(sSupportedSampleRates), std::end(sSupportedSampleRates), decoderFormat.mSampleRate)) {
		os_log_error(OS_LOG_DEFAULT, "Unsupported sample rate: %f", decoderFormat.mSampleRate);

		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not supported."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Unsupported DSD sample rate"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's sample rate is not supported for DSD to PCM conversion."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Generate non-interleaved 32-bit float output
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;

	mFormat.mSampleRate			= decoderFormat.mSampleRate / DSD_FRAMES_PER_PCM_FRAME;
	mFormat.mChannelsPerFrame	= decoderFormat.mChannelsPerFrame;
	mFormat.mBitsPerChannel		= 32;

	mFormat.mBytesPerPacket		= mFormat.mBitsPerChannel / 8;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	mChannelLayout 				= mDecoder->GetChannelLayout();

	if(!mBufferList.Allocate(decoderFormat, 16384)) {
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);

		return false;
	}

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	mContext.resize(mFormat.mChannelsPerFrame);

	return true;
}

bool SFB::Audio::DSDPCMDecoder::_Close(CFErrorRef *error)
{
	if(!mDecoder->Close(error))
		return false;

	mBufferList.Deallocate();
	mContext.clear();

	return true;
}

SFB::CFString SFB::Audio::DSDPCMDecoder::_GetSourceFormatDescription() const
{
	return CFString(mDecoder->CreateSourceFormatDescription());
}

#pragma mark Functionality

UInt32 SFB::Audio::DSDPCMDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	// Only multiples of 8 frames can be read
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 != frameCount % 8) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	UInt32 framesRead = 0;
	const float linearGain = mLinearGain;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	for(;;) {
		// Grab the DSD audio
		UInt32 framesRemaining 		= frameCount - framesRead;
		UInt32 dsdFramesRemaining 	= DSD_FRAMES_PER_PCM_FRAME * framesRemaining;
		UInt32 dsdFramesDecoded 	= mDecoder->ReadAudio(mBufferList, std::min(mBufferList.GetCapacityFrames(), dsdFramesRemaining));
		if(0 == dsdFramesDecoded)
			break;

		UInt32 framesDecoded 		= dsdFramesDecoded / DSD_FRAMES_PER_PCM_FRAME;

		// Convert to PCM
		// NB: Currently DSDIFFDecoder and DSFDecoder only produce non-interleaved output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			mContext[i].Translate(framesDecoded,
								  (const unsigned char *)mBufferList->mBuffers[i].mData + bufferList->mBuffers[i].mDataByteSize, 1,
								  !mBufferList.GetFormat().IsBigEndian(),
								  (float *)bufferList->mBuffers[i].mData, 1);

			// Boost signal by 6 dBFS
			vDSP_vsmul((float *)bufferList->mBuffers[i].mData, 1, &linearGain, (float *)bufferList->mBuffers[i].mData, 1, framesDecoded);

			bufferList->mBuffers[i].mDataByteSize += mFormat.FrameCountToByteCount(framesDecoded);
		}

		framesRead += framesDecoded;

		// All requested frames were read
		if(framesRead == frameCount)
			break;
	}

	return framesRead;
}

SInt64 SFB::Audio::DSDPCMDecoder::_GetTotalFrames() const
{
	return mDecoder->GetTotalFrames() / DSD_FRAMES_PER_PCM_FRAME;
}

SInt64 SFB::Audio::DSDPCMDecoder::_GetCurrentFrame() const
{
	return mDecoder->GetCurrentFrame() / DSD_FRAMES_PER_PCM_FRAME;
}

SInt64 SFB::Audio::DSDPCMDecoder::_SeekToFrame(SInt64 frame)
{
	if(-1 == mDecoder->SeekToFrame(DSD_FRAMES_PER_PCM_FRAME * frame))
		return -1;

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	return _GetCurrentFrame();
}
