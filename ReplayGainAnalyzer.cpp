/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

/*
 *  ReplayGainAnalysis - analyzes input samples and give the recommended dB change
 *  Copyright (C) 2001 David Robinson and Glen Sawyer
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  concept and filter values by David Robinson (David@Robinson.org)
 *    -- blame him if you think the idea is flawed
 *  original coding by Glen Sawyer (glensawyer@hotmail.com)
 *    -- blame him if you think this runs too slowly, or the coding is otherwise flawed
 *
 *  lots of code improvements by Frank Klemm ( http://www.uni-jena.de/~pfk/mpp/ )
 *    -- credit him for all the _good_ programming ;)
 *
 *  minor cosmetic tweaks to integrate with FLAC by Josh Coalson
 *
 *
 *  For an explanation of the concepts and the basic algorithms involved, go to:
 *    http://www.replaygain.org/
 */

/*
 *  Here's the deal. Call
 *
 *    InitGainAnalysis ( long samplefreq );
 *
 *  to initialize everything. Call
 *
 *    AnalyzeSamples ( const Float_t*  left_samples,
 *                     const Float_t*  right_samples,
 *                     size_t          num_samples,
 *                     int             num_channels );
 *
 *  as many times as you want, with as many or as few samples as you want.
 *  If mono, pass the sample buffer in through left_samples, leave
 *  right_samples NULL, and make sure num_channels = 1.
 *
 *    GetTitleGain()
 *
 *  will return the recommended dB level change for all samples analyzed
 *  SINCE THE LAST TIME you called GetTitleGain() OR InitGainAnalysis().
 *
 *    GetAlbumGain()
 *
 *  will return the recommended dB level change for all samples analyzed
 *  since InitGainAnalysis() was called and finalized with GetTitleGain().
 *
 *  Pseudo-code to process an album:
 *
 *    Float_t       l_samples [4096];
 *    Float_t       r_samples [4096];
 *    size_t        num_samples;
 *    unsigned int  num_songs;
 *    unsigned int  i;
 *
 *    InitGainAnalysis ( 44100 );
 *    for ( i = 1; i <= num_songs; i++ ) {
 *        while ( ( num_samples = getSongSamples ( song[i], left_samples, right_samples ) ) > 0 )
 *            AnalyzeSamples ( left_samples, right_samples, num_samples, 2 );
 *        fprintf ("Recommended dB change for song %2d: %+6.2f dB\n", i, GetTitleGain() );
 *    }
 *    fprintf ("Recommended dB change for whole album: %+6.2f dB\n", GetAlbumGain() );
 */

/*
 *  So here's the main source of potential code confusion:
 *
 *  The filters applied to the incoming samples are IIR filters,
 *  meaning they rely on up to <filter order> number of previous samples
 *  AND up to <filter order> number of previous filtered samples.
 *
 *  I set up the AnalyzeSamples routine to minimize memory usage and interface
 *  complexity. The speed isn't compromised too much (I don't think), but the
 *  internal complexity is higher than it should be for such a relatively
 *  simple routine.
 *
 *  Optimization/clarity suggestions are welcome.
 */

#include <cmath>
#include <cstring>
#include <algorithm>

#include <Accelerate/Accelerate.h>

#include "ReplayGainAnalyzer.h"
#include "AudioConverter.h"
#include "AudioDecoder.h"
#include "AudioBufferList.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"

// ========================================
// Error Codes
// ========================================
const CFStringRef SFB::Audio::ReplayGainAnalyzer::ErrorDomain = CFSTR("org.sbooth.AudioEngine.ErrorDomain.ReplayGainAnalyzer");

// ========================================
// RG constants
// ========================================
#define YULE_ORDER					10
#define BUTTER_ORDER				2
#define RMS_PERCENTILE				0.95		/* percentile which is louder than the proposed level */
#define MAX_SAMP_FREQ				48000.		/* maximum allowed sample frequency [Hz] */
#define RMS_WINDOW_TIME				0.050		/* Time slice size [s] */
#define STEPS_per_dB				100.		/* Table entries per dB */
#define MAX_dB						120.		/* Table entries for 0...MAX_dB (normal max. values are 70...80 dB) */

#define MAX_ORDER					(BUTTER_ORDER > YULE_ORDER ? BUTTER_ORDER : YULE_ORDER)
/* [JEC] the following was originally #defined as:
 *   (size_t) (MAX_SAMP_FREQ * RMS_WINDOW_TIME)
 * but that seemed to fail to take into account the ceil() part of the
 * sampleWindow calculation in ResetSampleFrequency(), and was causing
 * buffer overflows for 48kHz analysis, hence the +1.
 */
#define MAX_SAMPLES_PER_WINDOW		(size_t) (MAX_SAMP_FREQ * RMS_WINDOW_TIME + 1.)		/* max. Samples per Time slice */
#define PINK_REF					64.82		/* 298640883795 */						/* calibration value */

namespace {
	/* for each filter:
	 [0] 48 kHz, [1] 44.1 kHz, [2] 32 kHz, [3] 24 kHz, [4] 22050 Hz, [5] 16 kHz, [6] 12 kHz, [7] is 11025 Hz, [8] 8 kHz */

	const float AYule [9] [11] = {
		{ 1., -3.84664617118067f,  7.81501653005538f,-11.34170355132042f, 13.05504219327545f,-12.28759895145294f,  9.48293806319790f, -5.87257861775999f,  2.75465861874613f, -0.86984376593551f, 0.13919314567432f },
		{ 1., -3.47845948550071f,  6.36317777566148f, -8.54751527471874f,  9.47693607801280f, -8.81498681370155f,  6.85401540936998f, -4.39470996079559f,  2.19611684890774f, -0.75104302451432f, 0.13149317958808f },
		{ 1., -2.37898834973084f,  2.84868151156327f, -2.64577170229825f,  2.23697657451713f, -1.67148153367602f,  1.00595954808547f, -0.45953458054983f,  0.16378164858596f, -0.05032077717131f, 0.02347897407020f },
		{ 1., -1.61273165137247f,  1.07977492259970f, -0.25656257754070f, -0.16276719120440f, -0.22638893773906f,  0.39120800788284f, -0.22138138954925f,  0.04500235387352f,  0.02005851806501f, 0.00302439095741f },
		{ 1., -1.49858979367799f,  0.87350271418188f,  0.12205022308084f, -0.80774944671438f,  0.47854794562326f, -0.12453458140019f, -0.04067510197014f,  0.08333755284107f, -0.04237348025746f, 0.02977207319925f },
		{ 1., -0.62820619233671f,  0.29661783706366f, -0.37256372942400f,  0.00213767857124f, -0.42029820170918f,  0.22199650564824f,  0.00613424350682f,  0.06747620744683f,  0.05784820375801f, 0.03222754072173f },
		{ 1., -1.04800335126349f,  0.29156311971249f, -0.26806001042947f,  0.00819999645858f,  0.45054734505008f, -0.33032403314006f,  0.06739368333110f, -0.04784254229033f,  0.01639907836189f, 0.01807364323573f },
		{ 1., -0.51035327095184f, -0.31863563325245f, -0.20256413484477f,  0.14728154134330f,  0.38952639978999f, -0.23313271880868f, -0.05246019024463f, -0.02505961724053f,  0.02442357316099f, 0.01818801111503f },
		{ 1., -0.25049871956020f, -0.43193942311114f, -0.03424681017675f, -0.04678328784242f,  0.26408300200955f,  0.15113130533216f, -0.17556493366449f, -0.18823009262115f,  0.05477720428674f, 0.04704409688120f }
	};

	const float BYule [9] [11] = {
		{ 0.03857599435200f, -0.02160367184185f, -0.00123395316851f, -0.00009291677959f, -0.01655260341619f,  0.02161526843274f, -0.02074045215285f,  0.00594298065125f,  0.00306428023191f,  0.00012025322027f,  0.00288463683916f },
		{ 0.05418656406430f, -0.02911007808948f, -0.00848709379851f, -0.00851165645469f, -0.00834990904936f,  0.02245293253339f, -0.02596338512915f,  0.01624864962975f, -0.00240879051584f,  0.00674613682247f, -0.00187763777362f },
		{ 0.15457299681924f, -0.09331049056315f, -0.06247880153653f,  0.02163541888798f, -0.05588393329856f,  0.04781476674921f,  0.00222312597743f,  0.03174092540049f, -0.01390589421898f,  0.00651420667831f, -0.00881362733839f },
		{ 0.30296907319327f, -0.22613988682123f, -0.08587323730772f,  0.03282930172664f, -0.00915702933434f, -0.02364141202522f, -0.00584456039913f,  0.06276101321749f, -0.00000828086748f,  0.00205861885564f, -0.02950134983287f },
		{ 0.33642304856132f, -0.25572241425570f, -0.11828570177555f,  0.11921148675203f, -0.07834489609479f, -0.00469977914380f, -0.00589500224440f,  0.05724228140351f,  0.00832043980773f, -0.01635381384540f, -0.01760176568150f },
		{ 0.44915256608450f, -0.14351757464547f, -0.22784394429749f, -0.01419140100551f,  0.04078262797139f, -0.12398163381748f,  0.04097565135648f,  0.10478503600251f, -0.01863887810927f, -0.03193428438915f,  0.00541907748707f },
		{ 0.56619470757641f, -0.75464456939302f,  0.16242137742230f,  0.16744243493672f, -0.18901604199609f,  0.30931782841830f, -0.27562961986224f,  0.00647310677246f,  0.08647503780351f, -0.03788984554840f, -0.00588215443421f },
		{ 0.58100494960553f, -0.53174909058578f, -0.14289799034253f,  0.17520704835522f,  0.02377945217615f,  0.15558449135573f, -0.25344790059353f,  0.01628462406333f,  0.06920467763959f, -0.03721611395801f, -0.00749618797172f },
		{ 0.53648789255105f, -0.42163034350696f, -0.00275953611929f,  0.04267842219415f, -0.10214864179676f,  0.14590772289388f, -0.02459864859345f, -0.11202315195388f, -0.04060034127000f,  0.04788665548180f, -0.02217936801134f }
	};

	const float AButter [9] [3] = {
		{ 1., -1.97223372919527f, 0.97261396931306f },
		{ 1., -1.96977855582618f, 0.97022847566350f },
		{ 1., -1.95835380975398f, 0.95920349965459f },
		{ 1., -1.95002759149878f, 0.95124613669835f },
		{ 1., -1.94561023566527f, 0.94705070426118f },
		{ 1., -1.92783286977036f, 0.93034775234268f },
		{ 1., -1.91858953033784f, 0.92177618768381f },
		{ 1., -1.91542108074780f, 0.91885558323625f },
		{ 1., -1.88903307939452f, 0.89487434461664f }
	};

	const float BButter [9] [3] = {
		{ 0.98621192462708f, -1.97242384925416f, 0.98621192462708f },
		{ 0.98500175787242f, -1.97000351574484f, 0.98500175787242f },
		{ 0.97938932735214f, -1.95877865470428f, 0.97938932735214f },
		{ 0.97531843204928f, -1.95063686409857f, 0.97531843204928f },
		{ 0.97316523498161f, -1.94633046996323f, 0.97316523498161f },
		{ 0.96454515552826f, -1.92909031105652f, 0.96454515552826f },
		{ 0.96009142950541f, -1.92018285901082f, 0.96009142950541f },
		{ 0.95856916599601f, -1.91713833199203f, 0.95856916599601f },
		{ 0.94597685600279f, -1.89195371200558f, 0.94597685600279f }
	};

	void filter(const float *input, float *output, size_t nSamples, const float *a, const float *b, size_t order)
	{
		for(size_t i = 0; i < nSamples; ++i) {
			double y = input[i] * b[0];
			for(size_t k = 1; k <= order; ++k)
				y += input[i - k] * b[k] - output[i - k] * a[k];
			output[i] = (float)y;
		}
	}

	bool analyzeResult(uint32_t *Array, size_t len, float& result)
	{
		uint32_t elems = 0;
		for(size_t i = 0; i < len; ++i)
			elems += Array[i];

		if(0 == elems)
			return false;

		int32_t upper = (int32_t) ceil(elems * (1. - RMS_PERCENTILE));
		size_t i = len;
		while( i-- > 0) {
			if((upper -= Array[i]) <= 0)
				break;
		}

		result = (float) (PINK_REF - i / STEPS_per_dB);
		return true;
	}

}

// This class exists to hide the internal state from the world
class SFB::Audio::ReplayGainAnalyzer::ReplayGainAnalyzerPrivate
{
public:
	float			linprebuf	[MAX_ORDER * 2];
	float			*linpre;											/* left input samples, with pre-buffer */
	float			lstepbuf	[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	float			*lstep;												/* left "first step" (i.e. post first filter) samples */
	float			loutbuf		[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	float			*lout;												/* left "out" (i.e. post second filter) samples */
	float			rinprebuf	[MAX_ORDER * 2];
	float			*rinpre;											/* right input samples ... */
	float			rstepbuf	[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	float			*rstep;
	float			routbuf		[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	float			*rout;
	unsigned int	sampleWindow;										/* number of samples required to reach number of milliseconds required for RMS window */
	unsigned long	totsamp;
	double			lsum;
	double			rsum;
	int				freqindex;
	uint32_t		A			[(size_t)(STEPS_per_dB * MAX_dB)];
	uint32_t		B			[(size_t)(STEPS_per_dB * MAX_dB)];

	float			trackPeak;
	float			albumPeak;

	ReplayGainAnalyzerPrivate()
		: sampleWindow(0), totsamp(0), lsum(0), rsum(0), freqindex(0), trackPeak(0), albumPeak(0)
	{
		linpre	= linprebuf + MAX_ORDER;
		rinpre	= rinprebuf + MAX_ORDER;
		lstep	= lstepbuf  + MAX_ORDER;
		rstep	= rstepbuf  + MAX_ORDER;
		lout	= loutbuf   + MAX_ORDER;
		rout	= routbuf   + MAX_ORDER;

		memset(A, 0, sizeof(A));
		memset(B, 0, sizeof(B));
	}

	/* zero out initial values */
	void Zero()
	{
		for(int i = 0; i < MAX_ORDER; ++i)
			linprebuf[i] = lstepbuf[i] = loutbuf[i] = rinprebuf[i] = rstepbuf[i] = routbuf[i] = 0;
	}
};


float SFB::Audio::ReplayGainAnalyzer::GetReferenceLoudness()
{
	return 89.0;
}

int32_t SFB::Audio::ReplayGainAnalyzer::GetMaximumSupportedSampleRate()
{
	return 48000;
}

int32_t SFB::Audio::ReplayGainAnalyzer::GetMinimumSupportedSampleRate()
{
	return 8000;
}

bool SFB::Audio::ReplayGainAnalyzer::SampleRateIsSupported(int32_t sampleRate)
{
	switch(sampleRate) {
		case 48000:
		case 44100:
		case 32000:
		case 24000:
		case 22050:
		case 16000:
		case 12000:
		case 11025:
		case  8000:
			return true;

		default:
			return false;
	}
}

bool SFB::Audio::ReplayGainAnalyzer::EvenMultipleSampleRateIsSupported(int32_t sampleRate)
{
	const int32_t minSampleRate = GetMinimumSupportedSampleRate();
	for(int32_t newSampleRate = sampleRate; newSampleRate > minSampleRate; newSampleRate /= 2) {
		if(SampleRateIsSupported(newSampleRate))
			return true;
	}

	const int32_t maxSampleRate = GetMaximumSupportedSampleRate();
	for(int32_t newSampleRate = sampleRate; newSampleRate < maxSampleRate; newSampleRate *= 2) {
		if(SampleRateIsSupported(newSampleRate))
			return true;
	}

	return false;
}

int32_t SFB::Audio::ReplayGainAnalyzer::GetBestReplayGainSampleRateForSampleRate(int32_t sampleRate)
{
	// Avoid resampling if possible
	if(SampleRateIsSupported(sampleRate))
		return sampleRate;

	// Next attempt to use even multiples
	const int32_t minSampleRate = GetMinimumSupportedSampleRate();
	for(int32_t newSampleRate = sampleRate; newSampleRate > minSampleRate; newSampleRate /= 2) {
		if(SampleRateIsSupported(newSampleRate))
			return newSampleRate;
	}

	const int32_t maxSampleRate = GetMaximumSupportedSampleRate();
	for(int32_t newSampleRate = sampleRate; newSampleRate < maxSampleRate; newSampleRate *= 2) {
		if(SampleRateIsSupported(newSampleRate))
			return newSampleRate;
	}

	// If not an even multiple of a supported rate just resample to the next lower supported rate
	if(48000 < sampleRate)
		return 48000;
	else if(44100 < sampleRate)
		return 44100;
	else if(32000 < sampleRate)
		return 32000;
	else if(24000 < sampleRate)
		return 24000;
	else if(22050 < sampleRate)
		return 22050;
	else if(16000 < sampleRate)
		return 16000;
	else if(12000 < sampleRate)
		return 12000;
	else if(11025 < sampleRate)
		return 11025;
	else if(8000 < sampleRate)
		return 8000;

	// Just use the redbook sample rate if all else fails
	return 44100;
}

SFB::Audio::ReplayGainAnalyzer::ReplayGainAnalyzer()
	: priv(new ReplayGainAnalyzerPrivate)
{}

// Empty destructor is required for unique_ptr with an incomplete type
// See http://stackoverflow.com/questions/9954518/stdunique-ptr-with-an-incomplete-type-wont-compile and  http://stackoverflow.com/questions/6012157/is-stdunique-ptrt-required-to-know-the-full-definition-of-t/6089065#6089065
SFB::Audio::ReplayGainAnalyzer::~ReplayGainAnalyzer()
{}

bool SFB::Audio::ReplayGainAnalyzer::AnalyzeURL(CFURLRef url, CFErrorRef *error)
{
	if(nullptr == url)
		return false;

	auto decoder = Decoder::CreateForURL(url, error);
	if(!decoder || !decoder->Open(error))
		return false;

	AudioStreamBasicDescription inputFormat = decoder->GetFormat();

	// Higher sampling rates aren't natively supported but are handled via resampling
	int32_t decoderSampleRate = (int32_t)inputFormat.mSampleRate;

	bool validSampleRate = EvenMultipleSampleRateIsSupported(decoderSampleRate);
	if(!validSampleRate) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” does not contain audio at a supported sample rate."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Only sample rates of 8.0 KHz, 11.025 KHz, 12.0 KHz, 16.0 KHz, 22.05 KHz, 24.0 KHz, 32.0 KHz, 44.1 KHz, 48 KHz and multiples are supported."), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(ReplayGainAnalyzer::ErrorDomain, ReplayGainAnalyzer::FileFormatNotSupportedError, description, url, failureReason, recoverySuggestion);
		}

		return false;
	}

	Float64 replayGainSampleRate = GetBestReplayGainSampleRateForSampleRate(decoderSampleRate);

	if(!(1 == inputFormat.mChannelsPerFrame || 2 == inputFormat.mChannelsPerFrame)) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” does not contain mono or stereo audio."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Only mono or stereo files supported"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(ReplayGainAnalyzer::ErrorDomain, ReplayGainAnalyzer::FileFormatNotSupportedError, description, url, failureReason, recoverySuggestion);
		}

		return false;
	}

	AudioStreamBasicDescription outputFormat = {
		.mFormatID				= kAudioFormatLinearPCM,
		.mFormatFlags			= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved,
		.mReserved				= 0,
		.mSampleRate			= replayGainSampleRate,
		.mChannelsPerFrame		= inputFormat.mChannelsPerFrame,
		.mBitsPerChannel		= 32,
		.mBytesPerPacket		= 4,
		.mBytesPerFrame			= 4,
		.mFramesPerPacket		= 1
	};

	if(!SetSampleRate((int32_t)outputFormat.mSampleRate)) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” does not contain audio at a supported sample rate."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Only sample rates of 8.0 KHz, 11.025 KHz, 12.0 KHz, 16.0 KHz, 22.05 KHz, 24.0 KHz, 32.0 KHz, 44.1 KHz, 48 KHz and multiples are supported."), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(ReplayGainAnalyzer::ErrorDomain, ReplayGainAnalyzer::FileFormatNotSupportedError, description, url, failureReason, recoverySuggestion);
		}

		return false;
	}

	// Converter takes ownership of decoder
	Converter converter(std::move(decoder), outputFormat);
	if(!converter.Open(error))
		return false;

	const UInt32 bufferSizeFrames = 512;
	BufferList outputBuffer(outputFormat, bufferSizeFrames);

	bool isStereo = (2 == outputFormat.mChannelsPerFrame);

	for(;;) {
		UInt32 frameCount = converter.ConvertAudio(outputBuffer, bufferSizeFrames);
		if(0 == frameCount)
			break;

		// Find the peak sample magnitude
		float lpeak, rpeak;
		vDSP_maxmgv((const float *)outputBuffer->mBuffers[0].mData, 1, &lpeak, frameCount);
		if(isStereo) {
			vDSP_maxmgv((const float *)outputBuffer->mBuffers[1].mData, 1, &rpeak, frameCount);
			priv->trackPeak = std::max(priv->trackPeak, std::max(lpeak, rpeak));
		}
		else
			priv->trackPeak = std::max(priv->trackPeak, lpeak);

		// The replay gain analyzer expects 16-bit sample size passed as floats
		const float scale = 1u << 15;
		vDSP_vsmul((const float *)outputBuffer->mBuffers[0].mData, 1, &scale, (float *)outputBuffer->mBuffers[0].mData, 1, frameCount);
		if(isStereo) {
			vDSP_vsmul((const float *)outputBuffer->mBuffers[1].mData, 1, &scale, (float *)outputBuffer->mBuffers[1].mData, 1, frameCount);
			AnalyzeSamples((const float *)outputBuffer->mBuffers[0].mData, (const float *)outputBuffer->mBuffers[1].mData, frameCount, true);
		}
		else
			AnalyzeSamples((const float *)outputBuffer->mBuffers[0].mData, nullptr, frameCount, false);
	}

	priv->albumPeak = std::max(priv->albumPeak, priv->trackPeak);

	return true;
}

bool SFB::Audio::ReplayGainAnalyzer::GetTrackGain(float& trackGain)
{
	if(!analyzeResult(priv->A, sizeof(priv->A) / sizeof(*(priv->A)), trackGain))
		return false;

	for(uint32_t i = 0; i < sizeof(priv->A) / sizeof(*(priv->A)); ++i) {
		priv->B[i] += priv->A[i];
		priv->A[i]  = 0;
	}

	priv->Zero();

	priv->totsamp	= 0;
	priv->lsum		= priv->rsum = 0.;

	return true;
}

bool SFB::Audio::ReplayGainAnalyzer::GetTrackPeak(float& trackPeak)
{
	trackPeak = priv->trackPeak;
	priv->trackPeak = 0.;
	return true;
}

bool SFB::Audio::ReplayGainAnalyzer::GetAlbumGain(float& albumGain)
{
    return analyzeResult(priv->B, sizeof(priv->B) / sizeof(*(priv->B)), albumGain);
}

bool SFB::Audio::ReplayGainAnalyzer::GetAlbumPeak(float& albumPeak)
{
	albumPeak = priv->albumPeak;
	return true;
}

bool SFB::Audio::ReplayGainAnalyzer::SetSampleRate(int32_t sampleRate)
{
	priv->Zero();

	switch(sampleRate) {
		case 48000: priv->freqindex = 0; break;
		case 44100: priv->freqindex = 1; break;
		case 32000: priv->freqindex = 2; break;
		case 24000: priv->freqindex = 3; break;
		case 22050: priv->freqindex = 4; break;
		case 16000: priv->freqindex = 5; break;
		case 12000: priv->freqindex = 6; break;
		case 11025: priv->freqindex = 7; break;
		case  8000: priv->freqindex = 8; break;
		default:
			return false;
	}

	priv->sampleWindow		= (unsigned int) ceil(sampleRate * RMS_WINDOW_TIME);

	priv->lsum				= 0.;
	priv->rsum				= 0.;
	priv->totsamp			= 0;

	memset(priv->A, 0, sizeof(priv->A));

	return true;
}

bool SFB::Audio::ReplayGainAnalyzer::AnalyzeSamples(const float *left_samples, const float *right_samples, size_t num_samples, bool stereo)
{
	if(0 == num_samples)
		return true;

	const float *curleft;
	const float *curright;

	long cursamplepos = 0;
	long batchsamples = (long)num_samples;

	if(!stereo)
		right_samples = left_samples;

	if(num_samples < MAX_ORDER) {
		memcpy(priv->linprebuf + MAX_ORDER, left_samples,  num_samples * sizeof(float));
		memcpy(priv->rinprebuf + MAX_ORDER, right_samples, num_samples * sizeof(float));
	}
	else {
		memcpy(priv->linprebuf + MAX_ORDER, left_samples,  MAX_ORDER   * sizeof(float));
		memcpy(priv->rinprebuf + MAX_ORDER, right_samples, MAX_ORDER   * sizeof(float));
	}

	while(batchsamples > 0) {
		long cursamples = std::min((long)priv->sampleWindow - (long)priv->totsamp, batchsamples);
		if(cursamplepos < MAX_ORDER) {
			curleft  = priv->linpre + cursamplepos;
			curright = priv->rinpre + cursamplepos;
			if(cursamples > MAX_ORDER - cursamplepos)
				cursamples = MAX_ORDER - cursamplepos;
		}
		else {
			curleft  = left_samples  + cursamplepos;
			curright = right_samples + cursamplepos;
		}

		filter(curleft , priv->lstep + priv->totsamp, (size_t)cursamples, AYule[priv->freqindex], BYule[priv->freqindex], YULE_ORDER);
		filter(curright, priv->rstep + priv->totsamp, (size_t)cursamples, AYule[priv->freqindex], BYule[priv->freqindex], YULE_ORDER);

		filter(priv->lstep + priv->totsamp, priv->lout + priv->totsamp, (size_t)cursamples, AButter[priv->freqindex], BButter[priv->freqindex], BUTTER_ORDER);
		filter(priv->rstep + priv->totsamp, priv->rout + priv->totsamp, (size_t)cursamples, AButter[priv->freqindex], BButter[priv->freqindex], BUTTER_ORDER);

		/* Get the squared values */
		float sum;
		vDSP_svesq(priv->lout + priv->totsamp, 1, &sum, (vDSP_Length)cursamples);
		priv->lsum += sum;

		vDSP_svesq(priv->rout + priv->totsamp, 1, &sum, (vDSP_Length)cursamples);
		priv->rsum += sum;

		batchsamples -= cursamples;
		cursamplepos += cursamples;
		priv->totsamp += (unsigned long)cursamples;

		/* Get the Root Mean Square (RMS) for this set of samples */
		if(priv->totsamp == priv->sampleWindow) {
			double  val  = STEPS_per_dB * 10. * log10((priv->lsum + priv->rsum) / priv->totsamp * 0.5 + 1.e-37);
			int     ival = (int) val;
			if(ival < 0)
				ival = 0;
			if(ival >= (int)(sizeof(priv->A)/sizeof(*(priv->A))))
				ival = (int)(sizeof(priv->A)/sizeof(*(priv->A))) - 1;

			priv->A [ival]++;
			priv->lsum = priv->rsum = 0.;

			memmove(priv->loutbuf , priv->loutbuf  + priv->totsamp, MAX_ORDER * sizeof(float));
			memmove(priv->routbuf , priv->routbuf  + priv->totsamp, MAX_ORDER * sizeof(float));
			memmove(priv->lstepbuf, priv->lstepbuf + priv->totsamp, MAX_ORDER * sizeof(float));
			memmove(priv->rstepbuf, priv->rstepbuf + priv->totsamp, MAX_ORDER * sizeof(float));

			priv->totsamp = 0;
		}

		/* somehow I really screwed up: Error in programming! Contact author about totsamp > sampleWindow */
		if(priv->totsamp > priv->sampleWindow)
			return false;
	}

	if(num_samples < MAX_ORDER) {
		memmove(priv->linprebuf,                           priv->linprebuf + num_samples, (MAX_ORDER-num_samples) * sizeof(float));
		memmove(priv->rinprebuf,                           priv->rinprebuf + num_samples, (MAX_ORDER-num_samples) * sizeof(float));
		memcpy (priv->linprebuf + MAX_ORDER - num_samples, left_samples,                  num_samples             * sizeof(float));
		memcpy (priv->rinprebuf + MAX_ORDER - num_samples, right_samples,                 num_samples             * sizeof(float));
	}
	else {
		memcpy (priv->linprebuf, left_samples  + num_samples - MAX_ORDER, MAX_ORDER * sizeof(float));
		memcpy (priv->rinprebuf, right_samples + num_samples - MAX_ORDER, MAX_ORDER * sizeof(float));
	}

    return true;
}
