/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
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

#import <algorithm>
#import <cmath>
#import <cstring>

#import <Accelerate/Accelerate.h>

#import "AudioBufferList.h"
#import "AudioConverter.h"
#import "AudioDecoder.h"
#import "CFWrapper.h"
#import "NSError+SFBURLPresentation.h"
#import "SFBReplayGainAnalyzer.h"

// NSError domain for SFBReplayGainAnalyzer
NSErrorDomain const SFBReplayGainAnalyzerErrorDomain = @"org.sbooth.AudioEngine.ReplayGainAnalyzer";


// Key names for the metadata dictionary
NSString * const SFBReplayGainAnalyzerGainKey = @"Gain";
NSString * const SFBReplayGainAnalyzerPeakKey = @"Peak";


// RG constants
#define YULE_ORDER					10
#define BUTTER_ORDER				2
#define RMS_PERCENTILE				0.95		/* percentile which is louder than the proposed level */
#define MAX_SAMP_FREQ				48000.		/* maximum allowed sample frequency [Hz] */
#define RMS_WINDOW_TIME				0.050		/* Time slice size [s] */
#define STEPS_per_dB				100.		/* Table entries per dB */
#define MAX_dB						120.		/* Table entries for 0...MAX_dB (normal max. values are 70...80 dB) */

#define MAX_ORDER					(BUTTER_ORDER > YULE_ORDER ? BUTTER_ORDER : YULE_ORDER)
#define MAX_SAMPLES_PER_WINDOW		(size_t) (MAX_SAMP_FREQ * RMS_WINDOW_TIME + 1.)		/* max. Samples per Time slice */
#define PINK_REF					64.82		/* 298640883795 */						/* calibration value */


namespace {
	const float SFBReplayGainAnalyzerInsufficientSamples = -24601; // Preserve nod to Les Mis

	/* for each filter:
	 [0] 48 kHz, [1] 44.1 kHz, [2] 32 kHz, [3] 24 kHz, [4] 22050 Hz, [5] 16 kHz, [6] 12 kHz, [7] is 11025 Hz, [8] 8 kHz */

	const float aYule [9] [11] = {
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

	const float bYule [9] [11] = {
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

	const float aButter [9] [3] = {
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

	const float bButter [9] [3] = {
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

	void Filter(const float *input, float *output, size_t nSamples, const float *a, const float *b, size_t order)
	{
		for(size_t i = 0; i < nSamples; ++i) {
			double y = input[i] * b[0];
			for(size_t k = 1; k <= order; ++k)
				y += input[i - k] * b[k] - output[i - k] * a[k];
			output[i] = (float)y;
		}
	}

	float AnalyzeResult(uint32_t *array, size_t len)
	{
		uint32_t elems = 0;
		for(size_t i = 0; i < len; ++i)
			elems += array[i];

		if(0 == elems)
			return SFBReplayGainAnalyzerInsufficientSamples;

		int32_t upper = (int32_t)ceil(elems * (1. - RMS_PERCENTILE));
		size_t i = len;
		while(i-- > 0) {
			if((upper -= array[i]) <= 0)
				break;
		}

		return (float)(PINK_REF - i / STEPS_per_dB);
	}

}


@interface SFBReplayGainAnalyzer ()
{
@private
	float			_linprebuf	[MAX_ORDER * 2];
	float			*_linpre;											/* left input samples, with pre-buffer */
	float			_lstepbuf	[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	float			*_lstep;											/* left "first step" (i.e. post first filter) samples */
	float			_loutbuf	[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	float			*_lout;												/* left "out" (i.e. post second filter) samples */
	float			_rinprebuf	[MAX_ORDER * 2];
	float			*_rinpre;											/* right input samples ... */
	float			_rstepbuf	[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	float			*_rstep;
	float			_routbuf	[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	float			*_rout;
	unsigned int	_sampleWindow;										/* number of samples required to reach number of milliseconds required for RMS window */
	unsigned long	_totsamp;
	double			_lsum;
	double			_rsum;
	int				_freqindex;
	uint32_t		_A			[(size_t)(STEPS_per_dB * MAX_dB)];
	uint32_t		_B			[(size_t)(STEPS_per_dB * MAX_dB)];

	float			_trackPeak;
	float			_albumPeak;
}

@property (class, nonatomic, readonly)  NSInteger maximumSupportedSampleRate;
@property (class, nonatomic, readonly)  NSInteger minimumSupportedSampleRate;

+ (BOOL)sampleRateIsSupported:(NSInteger)sampleRate;
+ (BOOL)evenMultipleSampleRateIsSupported:(NSInteger)sampleRate;
+ (NSInteger)bestReplayGainSampleRateForSampleRate:(NSInteger)sampleRate;

- (void)resetState;
- (void)setSampleRate:(NSInteger)sampleRate;
- (BOOL)analyzeLeftSamples:(const float *)leftSamples rightSamples:(const float *)rightSamples sampleCount:(size_t)sampleCount isStereo:(BOOL)stereo;
@end

@implementation SFBReplayGainAnalyzer

+ (float)referenceLoudness
{
	return 89.0;
}

+ (NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error
{
	SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];
	NSMutableDictionary *result = [NSMutableDictionary dictionary];
	for(NSURL *url in urls) {
		if(![analyzer analyzeTrack:url error:error])
			return nil;
		result[url] = analyzer.trackGainAndPeakSample;
	}

	NSDictionary *albumGainAndPeakSample = analyzer.albumGainAndPeakSample;
	if(albumGainAndPeakSample)
		[result addEntriesFromDictionary:albumGainAndPeakSample];

	return [result copy];
}

- (instancetype)init
{
	if((self = [super init])) {
		_trackPeak = -FLT_MAX;
		_albumPeak = -FLT_MAX;
		_linpre	= _linprebuf + MAX_ORDER;
		_rinpre	= _rinprebuf + MAX_ORDER;
		_lstep	= _lstepbuf  + MAX_ORDER;
		_rstep	= _rstepbuf  + MAX_ORDER;
		_lout	= _loutbuf   + MAX_ORDER;
		_rout	= _routbuf   + MAX_ORDER;
	}
	return self;
}

- (BOOL)analyzeTrack:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFB::CFError err;
	auto decoder = SFB::Audio::Decoder::CreateForURL((__bridge CFURLRef)url, &err);
	if(!decoder || !decoder->Open(&err)) {
		if(error)
			*error = (__bridge_transfer NSError *)err.Relinquish();
		return NO;
	}

	AudioStreamBasicDescription inputFormat = decoder->GetFormat();

	// Higher sampling rates aren't natively supported but are handled via resampling
	NSInteger decoderSampleRate = (NSInteger)inputFormat.mSampleRate;

	bool validSampleRate = [SFBReplayGainAnalyzer evenMultipleSampleRateIsSupported:decoderSampleRate];
	if(!validSampleRate) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBReplayGainAnalyzerErrorDomain
											 code:SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” does not contain audio at a supported sample rate.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Unsupported sample rate", @"")
							   recoverySuggestion:NSLocalizedString(@"Only sample rates of 8.0 KHz, 11.025 KHz, 12.0 KHz, 16.0 KHz, 22.05 KHz, 24.0 KHz, 32.0 KHz, 44.1 KHz, 48 KHz and multiples are supported.", @"")];
		return NO;
	}

	Float64 replayGainSampleRate = [SFBReplayGainAnalyzer bestReplayGainSampleRateForSampleRate:decoderSampleRate];

	if(!(1 == inputFormat.mChannelsPerFrame || 2 == inputFormat.mChannelsPerFrame)) {
		if(error)
		*error = [NSError SFB_errorWithDomain:SFBReplayGainAnalyzerErrorDomain
										 code:SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported
				descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” does not contain mono or stereo audio.", @"")
										  url:url
								failureReason:NSLocalizedString(@"Unsupported number of channels", @"")
						   recoverySuggestion:NSLocalizedString(@"Only mono and stereo files supported.", @"")];
		return NO;
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

	// Will NSAssert() if an invalid sample rate is passed
	[self setSampleRate:(NSInteger)outputFormat.mSampleRate];

	// Converter takes ownership of decoder
	SFB::Audio::Converter converter(std::move(decoder), outputFormat);

	const UInt32 bufferSizeFrames = 512;
	if(!converter.Open(bufferSizeFrames, &err)) {
		if(error)
			*error = (__bridge_transfer NSError *)err.Relinquish();
		return NO;
	}

	SFB::Audio::BufferList outputBuffer(outputFormat, bufferSizeFrames);

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
			_trackPeak = std::max(_trackPeak, std::max(lpeak, rpeak));
		}
		else
			_trackPeak = std::max(_trackPeak, lpeak);

		// The replay gain analyzer expects 16-bit sample size passed as floats
		const float scale = 1u << 15;
		vDSP_vsmul((const float *)outputBuffer->mBuffers[0].mData, 1, &scale, (float *)outputBuffer->mBuffers[0].mData, 1, frameCount);
		if(isStereo) {
			vDSP_vsmul((const float *)outputBuffer->mBuffers[1].mData, 1, &scale, (float *)outputBuffer->mBuffers[1].mData, 1, frameCount);
			[self analyzeLeftSamples:(const float *)outputBuffer->mBuffers[0].mData rightSamples:(const float *)outputBuffer->mBuffers[1].mData sampleCount:frameCount isStereo:YES];
		}
		else
			[self analyzeLeftSamples:(const float *)outputBuffer->mBuffers[0].mData rightSamples:NULL sampleCount:frameCount isStereo:NO];
	}

	_albumPeak = std::max(_albumPeak, _trackPeak);

	return YES;
}

- (NSDictionary *)trackGainAndPeakSample
{

	float gain = AnalyzeResult(_A, sizeof(_A) / sizeof(*(_A)));

	for(uint32_t i = 0; i < sizeof(_A) / sizeof(*(_A)); ++i) {
		_B[i] += _A[i];
		_A[i]  = 0;
	}

	[self resetState];

	_totsamp	= 0;
	_lsum		= _rsum = 0.;

	float peak 	= _trackPeak;
	_trackPeak 	= 0;

	if(gain == SFBReplayGainAnalyzerInsufficientSamples)
		return nil;

	return @{ SFBReplayGainAnalyzerGainKey: @(gain), SFBReplayGainAnalyzerPeakKey: @(peak) };
}

- (NSDictionary *)albumGainAndPeakSample
{
	float gain = AnalyzeResult(_B, sizeof(_B) / sizeof(*(_B)));

	float peak 	= _albumPeak;
	_albumPeak 	= 0;

	if(gain == SFBReplayGainAnalyzerInsufficientSamples)
		return nil;

	return @{ SFBReplayGainAnalyzerGainKey: @(gain), SFBReplayGainAnalyzerPeakKey: @(peak) };
}

#pragma mark Internal

+ (NSInteger)maximumSupportedSampleRate
{
	return 48000;
}

+ (NSInteger)minimumSupportedSampleRate
{
	return 8000;
}

+ (BOOL)sampleRateIsSupported:(NSInteger)sampleRate
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
			return YES;

		default:
			return NO;
	}
}

+ (BOOL)evenMultipleSampleRateIsSupported:(NSInteger)sampleRate
{
	const NSInteger minSampleRate = self.minimumSupportedSampleRate;
	for(NSInteger newSampleRate = sampleRate; newSampleRate > minSampleRate; newSampleRate /= 2) {
		if([self sampleRateIsSupported:newSampleRate])
			return YES;
	}

	const NSInteger maxSampleRate = self.maximumSupportedSampleRate;
	for(NSInteger newSampleRate = sampleRate; newSampleRate < maxSampleRate; newSampleRate *= 2) {
		if([self sampleRateIsSupported:newSampleRate])
			return YES;
	}

	return NO;
}

+ (NSInteger)bestReplayGainSampleRateForSampleRate:(NSInteger)sampleRate
{
	// Avoid resampling if possible
	if([self sampleRateIsSupported:sampleRate])
		return sampleRate;

	// Next attempt to use even multiples
	const NSInteger minSampleRate = self.minimumSupportedSampleRate;
	for(NSInteger newSampleRate = sampleRate; newSampleRate > minSampleRate; newSampleRate /= 2) {
		if([self sampleRateIsSupported:newSampleRate])
			return newSampleRate;
	}

	const NSInteger maxSampleRate = self.maximumSupportedSampleRate;
	for(NSInteger newSampleRate = sampleRate; newSampleRate < maxSampleRate; newSampleRate *= 2) {
		if([self sampleRateIsSupported:newSampleRate])
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

- (void)resetState
{
	for(int i = 0; i < MAX_ORDER; ++i)
		_linprebuf[i] = _lstepbuf[i] = _loutbuf[i] = _rinprebuf[i] = _rstepbuf[i] = _routbuf[i] = 0;
}

- (void)setSampleRate:(NSInteger)sampleRate
{
	NSParameterAssert([SFBReplayGainAnalyzer sampleRateIsSupported:sampleRate]);

	[self resetState];

	switch(sampleRate) {
		case 48000: _freqindex = 0; break;
		case 44100: _freqindex = 1; break;
		case 32000: _freqindex = 2; break;
		case 24000: _freqindex = 3; break;
		case 22050: _freqindex = 4; break;
		case 16000: _freqindex = 5; break;
		case 12000: _freqindex = 6; break;
		case 11025: _freqindex = 7; break;
		case  8000: _freqindex = 8; break;
		default:
			NSAssert(0, @"Unsupported sample rate %ld", (long)sampleRate);
	}

	_sampleWindow		= (unsigned int)ceil(sampleRate * RMS_WINDOW_TIME);

	_lsum				= 0.;
	_rsum				= 0.;
	_totsamp			= 0;

	memset(_A, 0, sizeof(_A));
}

- (BOOL)analyzeLeftSamples:(const float *)left_samples rightSamples:(const float *)right_samples sampleCount:(size_t)num_samples isStereo:(BOOL)stereo
{
	if(0 == num_samples)
		return YES;

	const float *curleft;
	const float *curright;

	long cursamplepos = 0;
	long batchsamples = (long)num_samples;

	if(!stereo)
		right_samples = left_samples;

	if(num_samples < MAX_ORDER) {
		memcpy(_linprebuf + MAX_ORDER, left_samples,  num_samples * sizeof(float));
		memcpy(_rinprebuf + MAX_ORDER, right_samples, num_samples * sizeof(float));
	}
	else {
		memcpy(_linprebuf + MAX_ORDER, left_samples,  MAX_ORDER   * sizeof(float));
		memcpy(_rinprebuf + MAX_ORDER, right_samples, MAX_ORDER   * sizeof(float));
	}

	while(batchsamples > 0) {
		long cursamples = std::min((long)_sampleWindow - (long)_totsamp, batchsamples);
		if(cursamplepos < MAX_ORDER) {
			curleft  = _linpre + cursamplepos;
			curright = _rinpre + cursamplepos;
			if(cursamples > MAX_ORDER - cursamplepos)
				cursamples = MAX_ORDER - cursamplepos;
		}
		else {
			curleft  = left_samples  + cursamplepos;
			curright = right_samples + cursamplepos;
		}

		Filter(curleft , _lstep + _totsamp, (size_t)cursamples, aYule[_freqindex], bYule[_freqindex], YULE_ORDER);
		Filter(curright, _rstep + _totsamp, (size_t)cursamples, aYule[_freqindex], bYule[_freqindex], YULE_ORDER);

		Filter(_lstep + _totsamp, _lout + _totsamp, (size_t)cursamples, aButter[_freqindex], bButter[_freqindex], BUTTER_ORDER);
		Filter(_rstep + _totsamp, _rout + _totsamp, (size_t)cursamples, aButter[_freqindex], bButter[_freqindex], BUTTER_ORDER);

		/* Get the squared values */
		float sum;
		vDSP_svesq(_lout + _totsamp, 1, &sum, (vDSP_Length)cursamples);
		_lsum += sum;

		vDSP_svesq(_rout + _totsamp, 1, &sum, (vDSP_Length)cursamples);
		_rsum += sum;

		batchsamples -= cursamples;
		cursamplepos += cursamples;
		_totsamp += (unsigned long)cursamples;

		/* Get the Root Mean Square (RMS) for this set of samples */
		if(_totsamp == _sampleWindow) {
			double  val  = STEPS_per_dB * 10. * log10((_lsum + _rsum) / _totsamp * 0.5 + 1.e-37);
			int     ival = (int) val;
			if(ival < 0)
				ival = 0;
			if(ival >= (int)(sizeof(_A)/sizeof(*(_A))))
				ival = (int)(sizeof(_A)/sizeof(*(_A))) - 1;

			_A [ival]++;
			_lsum = _rsum = 0.;

			memmove(_loutbuf , _loutbuf  + _totsamp, MAX_ORDER * sizeof(float));
			memmove(_routbuf , _routbuf  + _totsamp, MAX_ORDER * sizeof(float));
			memmove(_lstepbuf, _lstepbuf + _totsamp, MAX_ORDER * sizeof(float));
			memmove(_rstepbuf, _rstepbuf + _totsamp, MAX_ORDER * sizeof(float));

			_totsamp = 0;
		}

		/* somehow I really screwed up: Error in programming! Contact author about totsamp > sampleWindow */
		if(_totsamp > _sampleWindow)
			return NO;
	}

	if(num_samples < MAX_ORDER) {
		memmove(_linprebuf,                           _linprebuf + num_samples, (MAX_ORDER-num_samples) * sizeof(float));
		memmove(_rinprebuf,                           _rinprebuf + num_samples, (MAX_ORDER-num_samples) * sizeof(float));
		memcpy (_linprebuf + MAX_ORDER - num_samples, left_samples,                  num_samples             * sizeof(float));
		memcpy (_rinprebuf + MAX_ORDER - num_samples, right_samples,                 num_samples             * sizeof(float));
	}
	else {
		memcpy (_linprebuf, left_samples  + num_samples - MAX_ORDER, MAX_ORDER * sizeof(float));
		memcpy (_rinprebuf, right_samples + num_samples - MAX_ORDER, MAX_ORDER * sizeof(float));
	}

    return YES;
}

@end
