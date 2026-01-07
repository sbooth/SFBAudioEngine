//
// Copyright (c) 2011-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

@import Accelerate;

#import "SFBReplayGainAnalyzer.h"

#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"
#import "SFBPCMDecoder.h"

// NSError domain for SFBReplayGainAnalyzer
NSErrorDomain const SFBReplayGainAnalyzerErrorDomain = @"org.sbooth.AudioEngine.ReplayGainAnalyzer";

// Key names for the metadata dictionary
NSString * const SFBReplayGainAnalyzerGainKey = @"Gain";
NSString * const SFBReplayGainAnalyzerPeakKey = @"Peak";

#define BUFFER_SIZE_FRAMES 2048

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

// RG constants
#define YULE_ORDER					10
#define BUTTER_ORDER				2
#define RMS_PERCENTILE				0.95		/* percentile which is louder than the proposed level */
#define RMS_WINDOW_TIME 			50 			/* Time slice size [ms] */
#define STEPS_per_dB				100.		/* Table entries per dB */
#define MAX_dB						120.		/* Table entries for 0...MAX_dB (normal max. values are 70...80 dB) */

#define MAX_ORDER					(BUTTER_ORDER > YULE_ORDER ? BUTTER_ORDER : YULE_ORDER)
#define PINK_REF					64.82		/* 298640883795 */						/* calibration value */

static const float SFBReplayGainAnalyzerInsufficientSamples = -24601; // Preserve nod to Les Mis

struct ReplayGainFilter {
	long rate;
	uint32_t downsample;
	float BYule [YULE_ORDER + 1];
	float AYule [YULE_ORDER + 1];
	float BButter [BUTTER_ORDER + 1];
	float AButter [BUTTER_ORDER + 1];
};

static const struct ReplayGainFilter sReplayGainFilters [] = {

	{
		48000, 0, /* ORIGINAL */
		{ 0.03857599435200,  -0.02160367184185,  -0.00123395316851,  -0.00009291677959,  -0.01655260341619,   0.02161526843274,  -0.02074045215285,   0.00594298065125,   0.00306428023191,   0.00012025322027,   0.00288463683916 },
		{ 1.00000000000000,  -3.84664617118067,   7.81501653005538, -11.34170355132042,  13.05504219327545, -12.28759895145294,   9.48293806319790, -5.87257861775999,   2.75465861874613,   -0.86984376593551,   0.13919314567432 },
		{ 0.98621192462708,  -1.97242384925416,   0.98621192462708 },
		{ 1.00000000000000,  -1.97223372919527,   0.97261396931306 },
	},

	{
		44100, 0, /* ORIGINAL */
		{ 0.05418656406430,  -0.02911007808948,  -0.00848709379851,  -0.00851165645469,  -0.00834990904936,   0.02245293253339,  -0.02596338512915,   0.01624864962975,  -0.00240879051584,   0.00674613682247,  -0.00187763777362 },
		{ 1.00000000000000,  -3.47845948550071,   6.36317777566148,  -8.54751527471874,   9.47693607801280,  -8.81498681370155,   6.85401540936998,  -4.39470996079559,   2.19611684890774,  -0.75104302451432,   0.13149317958808 },
		{ 0.98500175787242,  -1.97000351574484,   0.98500175787242 },
		{ 1.00000000000000,  -1.96977855582618,   0.97022847566350 },
	},

	{
		37800, 0,
		{ 0.10296717174470,  -0.04877975583256,  -0.02878009075237,  -0.03519509188311,   0.02888717172493,  -0.00609872684844,   0.00209851217112,   0.00911704668543,   0.01154404718589,  -0.00630293688700,   0.00107527155228 },
		{ 1.00000000000000,  -2.64848054923531,   3.58406058405771,  -3.83794914179161,   3.90142345804575,  -3.50179818637243,   2.67085284083076,  -1.82581142372418,   1.09530368139801,  -0.47689017820395,   0.11171431535905 },
		{ 0.98252400815195,  -1.96504801630391,   0.98252400815195 },
		{ 1.00000000000000,  -1.96474258269041,   0.96535344991740 },
	},

	{
		36000, 0,
		{ 0.11572297028613,  -0.04120916051252,  -0.04977731768022,  -0.01047308680426,   0.00750863219157,   0.00055507694408,   0.00140344192886,   0.01286095246036,   0.00998223033885,  -0.00725013810661,   0.00326503346879 },
		{ 1.00000000000000,  -2.43606802820871,   3.01907406973844,  -2.90372016038192,   2.67947188094303,  -2.17606479220391,   1.44912956803015,  -0.87785765549050,   0.53592202672557,  -0.26469344817509,   0.07495878059717 },
		{ 0.98165826840326,  -1.96331653680652,   0.98165826840326 },
		{ 1.00000000000000,  -1.96298008938934,   0.96365298422371 },
	},

	{
		32000, 0, /* ORIGINAL */
		{ 0.15457299681924,  -0.09331049056315,  -0.06247880153653,   0.02163541888798,  -0.05588393329856,   0.04781476674921,   0.00222312597743,   0.03174092540049,  -0.01390589421898,   0.00651420667831,  -0.00881362733839 },
		{ 1.00000000000000,  -2.37898834973084,   2.84868151156327,  -2.64577170229825,   2.23697657451713,  -1.67148153367602,   1.00595954808547,  -0.45953458054983,   0.16378164858596,  -0.05032077717131,   0.02347897407020 },
		{ 0.97938932735214,  -1.95877865470428,   0.97938932735214 },
		{ 1.00000000000000,  -1.95835380975398,   0.95920349965459 },
	},

	{
		28000, 0,
		{ 0.23882392323383,  -0.22007791534089,  -0.06014581950332,   0.05004458058021,  -0.03293111254977,   0.02348678189717,   0.04290549799671,  -0.00938141862174,   0.00015095146303,  -0.00712601540885,  -0.00626520210162 },
		{ 1.00000000000000,  -2.06894080899139,   1.76944699577212,  -0.81404732584187,   0.25418286850232,  -0.30340791669762,   0.35616884070937,  -0.14967310591258,  -0.07024154183279,   0.11078404345174,  -0.03551838002425 },
		{ 0.97647981663949,  -1.95295963327897,   0.97647981663949 },
		{ 1.00000000000000,  -1.95240635772520,   0.95351290883275 },

	},

	{
		24000, 0, /* ORIGINAL */
		{ 0.30296907319327,  -0.22613988682123,  -0.08587323730772,   0.03282930172664,  -0.00915702933434,  -0.02364141202522,  -0.00584456039913,   0.06276101321749,  -0.00000828086748,   0.00205861885564,  -0.02950134983287 },
		{ 1.00000000000000,  -1.61273165137247,   1.07977492259970,  -0.25656257754070,  -0.16276719120440,  -0.22638893773906,   0.39120800788284,  -0.22138138954925,   0.04500235387352,   0.02005851806501,   0.00302439095741 },
		{ 0.97531843204928,  -1.95063686409857,   0.97531843204928 },
		{ 1.00000000000000,  -1.95002759149878,   0.95124613669835 },
	},

	{
		22050, 0, /* ORIGINAL */
		{ 0.33642304856132,  -0.25572241425570,  -0.11828570177555,   0.11921148675203,  -0.07834489609479,  -0.00469977914380,  -0.00589500224440,   0.05724228140351,   0.00832043980773,  -0.01635381384540,  -0.01760176568150 },
		{ 1.00000000000000,  -1.49858979367799,   0.87350271418188,   0.12205022308084,  -0.80774944671438,   0.47854794562326,  -0.12453458140019,  -0.04067510197014,   0.08333755284107,  -0.04237348025746,   0.02977207319925 },
		{ 0.97316523498161,  -1.94633046996323,   0.97316523498161 },
		{ 1.00000000000000,  -1.94561023566527,   0.94705070426118 },
	},

	{
		18900, 0,
		{ 0.38412657295385,  -0.44533729608120,   0.20426638066221,  -0.28031676047946,   0.31484202614802,  -0.26078311203207,   0.12925201224848,  -0.01141164696062,   0.03036522115769,  -0.03776339305406,   0.00692036603586 },
		{ 1.00000000000000,  -1.74403915585708,   1.96686095832499,  -2.10081452941881,   1.90753918182846,  -1.83814263754422,   1.36971352214969,  -0.77883609116398,   0.39266422457649,  -0.12529383592986,   0.05424760697665 },
		{ 0.96535326815829,  -1.93070653631658,   0.96535326815829 },
		{ 1.00000000000000,  -1.92950577983524,   0.93190729279793 },
	},

	{
		16000, 0, /* ORIGINAL */
		{ 0.44915256608450,  -0.14351757464547,  -0.22784394429749,  -0.01419140100551,   0.04078262797139,  -0.12398163381748,   0.04097565135648,   0.10478503600251,  -0.01863887810927,  -0.03193428438915,   0.00541907748707 },
		{ 1.00000000000000,  -0.62820619233671,   0.29661783706366,  -0.37256372942400,   0.00213767857124,  -0.42029820170918,   0.22199650564824,   0.00613424350682,   0.06747620744683,   0.05784820375801,   0.03222754072173 },
		{ 0.96454515552826,  -1.92909031105652,   0.96454515552826 },
		{ 1.00000000000000,  -1.92783286977036,   0.93034775234268 },
	},

	{
		12000, 0, /* ORIGINAL */
		{ 0.56619470757641,  -0.75464456939302,   0.16242137742230,   0.16744243493672,  -0.18901604199609,   0.30931782841830,  -0.27562961986224,   0.00647310677246,   0.08647503780351,  -0.03788984554840,  -0.00588215443421 },
		{ 1.00000000000000,  -1.04800335126349,   0.29156311971249,  -0.26806001042947,   0.00819999645858,   0.45054734505008,  -0.33032403314006,   0.06739368333110,  -0.04784254229033,   0.01639907836189,   0.01807364323573 },
		{ 0.96009142950541,  -1.92018285901082,   0.96009142950541 },
		{ 1.00000000000000,  -1.91858953033784,   0.92177618768381 },
	},

	{
		11025, 0, /* ORIGINAL */
		{ 0.58100494960553,  -0.53174909058578,  -0.14289799034253,   0.17520704835522,   0.02377945217615,   0.15558449135573,  -0.25344790059353,   0.01628462406333,   0.06920467763959,  -0.03721611395801,  -0.00749618797172 },
		{ 1.00000000000000,  -0.51035327095184,  -0.31863563325245,  -0.20256413484477,   0.14728154134330,   0.38952639978999,  -0.23313271880868,  -0.05246019024463,  -0.02505961724053,   0.02442357316099,   0.01818801111503 },
		{ 0.95856916599601,  -1.91713833199203,   0.95856916599601 },
		{ 1.00000000000000,  -1.91542108074780,   0.91885558323625 },
	},

	{
		8000, 0, /* ORIGINAL */
		{ 0.53648789255105,  -0.42163034350696,  -0.00275953611929,   0.04267842219415,  -0.10214864179676,   0.14590772289388,  -0.02459864859345,  -0.11202315195388,  -0.04060034127000,   0.04788665548180,  -0.02217936801134 },
		{ 1.00000000000000,  -0.25049871956020,  -0.43193942311114,  -0.03424681017675,  -0.04678328784242,   0.26408300200955,   0.15113130533216,  -0.17556493366449,  -0.18823009262115,   0.05477720428674,   0.04704409688120 },
		{ 0.94597685600279,  -1.89195371200558,   0.94597685600279 },
		{ 1.00000000000000,  -1.88903307939452,   0.89487434461664 },
	},

};

/* When calling this procedure, make sure that ip[-order] and op[-order] point to real data! */

static void Filter(const float *input, float *output, size_t nSamples, const float *a, const float *b, size_t order, uint32_t downsample)
{
	const float *input_head = input;
	float *output_head = output;

	for(size_t i = 0; i < nSamples; i++, input_head += downsample, ++output_head) {
		const float *input_tail = input_head;
		float *output_tail = output_head;

		double y = *input_head * b[0];

		for(size_t k = 1; k <= order; k++) {
			input_tail -= downsample;
			--output_tail;
			y += *input_tail * b[k] - *output_tail * a[k];
		}

		output[i] = (float)y;
	}
}

static float AnalyzeResult(uint32_t *array, size_t len)
{
	uint32_t elems = 0;
	for(size_t i = 0; i < len; ++i)
		elems += array[i];

	if(elems == 0)
		return SFBReplayGainAnalyzerInsufficientSamples;

	int32_t upper = (int32_t)ceil(elems * (1. - RMS_PERCENTILE));
	size_t i = len;
	while(i-- > 0) {
		if((upper -= array[i]) <= 0)
			break;
	}

	return (float)(PINK_REF - i / STEPS_per_dB);
}

@interface SFBReplayGainAnalyzer ()
{
@private
	float			_linprebuf	[MAX_ORDER * 2];
	float			*_linpre;											/* left input samples, with pre-buffer */
	float			*_lstepbuf;
	float			*_lstep;											/* left "first step" (i.e. post first filter) samples */
	float			*_loutbuf;
	float			*_lout;												/* left "out" (i.e. post second filter) samples */
	float			_rinprebuf	[MAX_ORDER * 2];
	float			*_rinpre;											/* right input samples ... */
	float			*_rstepbuf;
	float			*_rstep;
	float			*_routbuf;
	float			*_rout;
	uint32_t		_sampleWindow;										/* number of samples required to reach number of milliseconds required for RMS window */
	uint64_t		_totsamp;
	double			_lsum;
	double			_rsum;
	uint32_t		_A			[(size_t)STEPS_per_dB * (size_t)MAX_dB];
	uint32_t		_B			[(size_t)STEPS_per_dB * (size_t)MAX_dB];

	struct ReplayGainFilter _filter;

	float			_trackPeak;
	float			_albumPeak;
}

@property (class, nonatomic, readonly)  NSInteger maximumSupportedSampleRate;
@property (class, nonatomic, readonly)  NSInteger minimumSupportedSampleRate;

+ (BOOL)sampleRateIsSupported:(NSInteger)sampleRate;
+ (BOOL)evenMultipleSampleRateIsSupported:(NSInteger)sampleRate;
+ (NSInteger)bestReplayGainSampleRateForSampleRate:(NSInteger)sampleRate;

- (void)resetState;
- (void)setupForAnalysisAtSampleRate:(NSInteger)sampleRate;
- (BOOL)analyzeLeftSamples:(const float *)leftSamples rightSamples:(const float *)rightSamples sampleCount:(size_t)sampleCount isStereo:(BOOL)stereo;
@end

@implementation SFBReplayGainAnalyzer

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBReplayGainAnalyzerErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		switch(err.code) {
			case SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported:
				if([userInfoKey isEqualToString:NSLocalizedDescriptionKey])
					return NSLocalizedString(@"The file's format is not supported.", @"");
				break;
				
			case SFBReplayGainAnalyzerErrorCodeInsufficientSamples:
				if([userInfoKey isEqualToString:NSLocalizedDescriptionKey])
					return NSLocalizedString(@"The file does not contain sufficient audio samples for analysis.", @"");
				break;
		}

		return nil;
	}];
}

+ (float)referenceLoudness
{
	return 89.0;
}

+ (NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error
{
	NSMutableDictionary *result = [NSMutableDictionary dictionary];

	SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];
	for(NSURL *url in urls) {
		NSDictionary *replayGain = [analyzer analyzeTrack:url error:error];
		if(!replayGain)
			return nil;
		result[url] = replayGain;
	}

	NSDictionary *albumGainAndPeakSample = [analyzer albumGainAndPeakSampleReturningError:error];
	if(!albumGainAndPeakSample)
		return nil;

	[result addEntriesFromDictionary:albumGainAndPeakSample];
	return [result copy];
}

- (instancetype)init
{
	if((self = [super init])) {
		_trackPeak = -FLT_MAX;
		_albumPeak = -FLT_MAX;
		_linpre = _linprebuf + MAX_ORDER;
		_rinpre = _rinprebuf + MAX_ORDER;
	}
	return self;
}

- (void)dealloc
{
	free(_lstepbuf);
	free(_rstepbuf);
	free(_loutbuf);
	free(_routbuf);
}

- (NSDictionary *)analyzeTrack:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBPCMDecoder *decoder = [[SFBPCMDecoder alloc] initWithURL:url error:error];
	if(!decoder || ![decoder openReturningError:error])
		return nil;

	const AudioStreamBasicDescription *inputFormat = decoder.processingFormat.streamDescription;

	// Higher sampling rates aren't natively supported but are handled via resampling
	NSInteger decoderSampleRate = (NSInteger)inputFormat->mSampleRate;

	bool validSampleRate = [SFBReplayGainAnalyzer evenMultipleSampleRateIsSupported:decoderSampleRate];
	if(!validSampleRate) {
		if(error)
			*error = SFBErrorWithLocalizedDescription(SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
													  NSLocalizedString(@"The file “%@” does not contain audio at a supported sample rate.", @""),
													  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"Only sample rates of 8.0 KHz, 11.025 KHz, 12.0 KHz, 16.0 KHz, 22.05 KHz, 24.0 KHz, 32.0 KHz, 44.1 KHz, 48 KHz and multiples are supported.", @"") },
													  SFBLocalizedNameForURL(url));
		return nil;
	}

	NSInteger replayGainSampleRate = [SFBReplayGainAnalyzer bestReplayGainSampleRateForSampleRate:decoderSampleRate];

	if(!(inputFormat->mChannelsPerFrame == 1 || inputFormat->mChannelsPerFrame == 2)) {
		if(error)
			*error = SFBErrorWithLocalizedDescription(SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
													  NSLocalizedString(@"The file “%@” does not contain mono or stereo audio.", @""),
													  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"Only mono and stereo files are supported.", @"") },
													  SFBLocalizedNameForURL(url));
		return nil;
	}

	AVAudioFormat *outputFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:replayGainSampleRate channels:inputFormat->mChannelsPerFrame interleaved:NO];

	// Will NSAssert() if an invalid sample rate is passed
	[self setupForAnalysisAtSampleRate:replayGainSampleRate];

	AVAudioConverter *converter = [[AVAudioConverter alloc] initFromFormat:decoder.processingFormat toFormat:outputFormat];
	if(!converter) {
		if(error)
			*error = SFBErrorWithLocalizedDescription(SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
													  NSLocalizedString(@"The format of the file “%@” is not supported.", @""),
													  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file's format is not supported for replay gain analysis.", @"") },
													  SFBLocalizedNameForURL(url));
		return nil;
	}

	AVAudioPCMBuffer *outputBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.outputFormat frameCapacity:BUFFER_SIZE_FRAMES];
	AVAudioPCMBuffer *decodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.inputFormat frameCapacity:BUFFER_SIZE_FRAMES];

	bool isStereo = (outputFormat.channelCount == 2);

	for(;;) {
		__block NSError *err = nil;
		AVAudioConverterOutputStatus status = [converter convertToBuffer:outputBuffer error:error withInputFromBlock:^AVAudioBuffer * _Nullable(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus * _Nonnull outStatus) {
			BOOL result = [decoder decodeIntoBuffer:decodeBuffer frameLength:inNumberOfPackets error:&err];
			if(!result)
				os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", err);

			if(result && decodeBuffer.frameLength == 0)
				*outStatus = AVAudioConverterInputStatus_EndOfStream;
			else
				*outStatus = AVAudioConverterInputStatus_HaveData;

			return decodeBuffer;
		}];

		if(status == AVAudioConverterOutputStatus_Error) {
			if(error)
				*error = err;
			return nil;
		}
		else if(status == AVAudioConverterOutputStatus_EndOfStream)
			break;

		AVAudioFrameCount frameCount = outputBuffer.frameLength;

		// Find the peak sample magnitude
		float lpeak;
		vDSP_maxmgv(outputBuffer.floatChannelData[0], 1, &lpeak, (vDSP_Length)frameCount);
		_trackPeak = MAX(_trackPeak, lpeak);
		if(isStereo) {
			float rpeak;
			vDSP_maxmgv(outputBuffer.floatChannelData[1], 1, &rpeak, (vDSP_Length)frameCount);
			_trackPeak = MAX(_trackPeak, rpeak);
		}

		// The replay gain analyzer expects 16-bit sample size passed as floats
		const float scale = 1u << 15;
		vDSP_vsmul(outputBuffer.floatChannelData[0], 1, &scale, outputBuffer.floatChannelData[0], 1, (vDSP_Length)frameCount);
		if(isStereo) {
			vDSP_vsmul(outputBuffer.floatChannelData[1], 1, &scale, outputBuffer.floatChannelData[1], 1, (vDSP_Length)frameCount);
			[self analyzeLeftSamples:outputBuffer.floatChannelData[0] rightSamples:outputBuffer.floatChannelData[1] sampleCount:(size_t)frameCount isStereo:YES];
		}
		else
			[self analyzeLeftSamples:outputBuffer.floatChannelData[0] rightSamples:NULL sampleCount:(size_t)frameCount isStereo:NO];
	}

	_albumPeak = MAX(_albumPeak, _trackPeak);

	// Calculate track RG
	float gain = AnalyzeResult(_A, sizeof(_A) / sizeof(*_A));

	for(uint32_t i = 0; i < sizeof(_A) / sizeof(*_A); ++i) {
		_B[i] += _A[i];
		_A[i]  = 0;
	}

	[self resetState];

	float peak = _trackPeak;
	_trackPeak = 0;

	if(gain == SFBReplayGainAnalyzerInsufficientSamples) {
		if(error)
			*error = SFBErrorWithLocalizedDescription(SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeInsufficientSamples,
													  NSLocalizedString(@"The file “%@” does not contain sufficient audio for analysis.", @""),
													  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The audio duration is too short for replay gain analysis.", @"") },
													  SFBLocalizedNameForURL(url));
		return nil;
	}

	return @{ SFBReplayGainAnalyzerGainKey: @(gain), SFBReplayGainAnalyzerPeakKey: @(peak) };
}

- (NSDictionary *)albumGainAndPeakSampleReturningError:(NSError **)error
{
	float gain = AnalyzeResult(_B, sizeof(_B) / sizeof(*_B));

	float peak = _albumPeak;
	_albumPeak = 0;

	if(gain == SFBReplayGainAnalyzerInsufficientSamples) {
		if(error)
			*error = [NSError errorWithDomain:SFBReplayGainAnalyzerErrorDomain
										 code:SFBReplayGainAnalyzerErrorCodeInsufficientSamples
									 userInfo:@{
										 NSLocalizedDescriptionKey: NSLocalizedString(@"The files do not contain sufficient audio for analysis.", @""),
										 NSLocalizedRecoverySuggestionErrorKey:NSLocalizedString(@"The audio duration is too short for replay gain analysis.", @"") }];
		return nil;
	}

	return @{ SFBReplayGainAnalyzerGainKey: @(gain), SFBReplayGainAnalyzerPeakKey: @(peak) };
}

#pragma mark Internal

+ (NSInteger)maximumSupportedSampleRate
{
	static NSInteger sampleRate = 0;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		for(size_t i = 0; i < sizeof(sReplayGainFilters) / sizeof(sReplayGainFilters[0]); ++i)
			sampleRate = MAX(sampleRate, sReplayGainFilters[i].rate);
	});

	return sampleRate;
}

+ (NSInteger)minimumSupportedSampleRate
{
	static NSInteger sampleRate = 0;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		for(size_t i = 0; i < sizeof(sReplayGainFilters) / sizeof(sReplayGainFilters[0]); ++i)
			sampleRate = MIN(sampleRate, sReplayGainFilters[i].rate);
	});

	return sampleRate;
}

+ (BOOL)sampleRateIsSupported:(NSInteger)sampleRate
{
	for(size_t i = 0; i < sizeof(sReplayGainFilters) / sizeof(sReplayGainFilters[0]); ++i) {
		if(sReplayGainFilters[i].rate == sampleRate)
			return YES;
	}

	return NO;
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
	NSInteger nextLowerSampleRate = 0;
	for(size_t i = 0; i < sizeof(sReplayGainFilters) / sizeof(sReplayGainFilters[0]); ++i) {
		if(sReplayGainFilters[i].rate < sampleRate)
			nextLowerSampleRate = MAX(nextLowerSampleRate, sReplayGainFilters[i].rate);
	}

	if(nextLowerSampleRate)
		return nextLowerSampleRate;

	// Just use the redbook sample rate if all else fails
	return 44100;
}

- (void)resetState
{
	/* zero out initial values */
	for(int i = 0; i < MAX_ORDER; ++i)
		_linprebuf[i] = _lstepbuf[i] = _loutbuf[i] = _rinprebuf[i] = _rstepbuf[i] = _routbuf[i] = 0;

	_lsum = _rsum = 0;
	_totsamp = 0;
}

- (void)setupForAnalysisAtSampleRate:(NSInteger)sampleRate
{
	NSParameterAssert([SFBReplayGainAnalyzer sampleRateIsSupported:sampleRate]);

	memset(&_filter, 0, sizeof(_filter));
	for(size_t i = 0; i < sizeof(sReplayGainFilters) / sizeof(sReplayGainFilters[0]); ++i) {
		if(sReplayGainFilters[i].rate == sampleRate) {
			_filter = sReplayGainFilters[i];
			_filter.downsample = 1;
			break;
		}
	}

	NSAssert(_filter.rate != 0, @"Unsupported sample rate %ld", (long)sampleRate);

	_sampleWindow = (uint32_t)((_filter.rate * RMS_WINDOW_TIME + 1000 - 1) / 1000);

	_lstepbuf = reallocf(_lstepbuf, sizeof(float) * (_sampleWindow + MAX_ORDER));
	_rstepbuf = reallocf(_rstepbuf, sizeof(float) * (_sampleWindow + MAX_ORDER));
	_loutbuf  = reallocf(_loutbuf,  sizeof(float) * (_sampleWindow + MAX_ORDER));
	_routbuf  = reallocf(_routbuf,  sizeof(float) * (_sampleWindow + MAX_ORDER));

	_lstep = _lstepbuf + MAX_ORDER;
	_rstep = _rstepbuf + MAX_ORDER;
	_lout  = _loutbuf  + MAX_ORDER;
	_rout  = _routbuf  + MAX_ORDER;

	[self resetState];

	memset(_A, 0, sizeof(_A));
}

- (BOOL)analyzeLeftSamples:(const float *)left_samples rightSamples:(const float *)right_samples sampleCount:(size_t)num_samples isStereo:(BOOL)stereo
{
	uint32_t downsample = _filter.downsample;
	num_samples /= downsample;

	if(num_samples == 0)
		return YES;

	const float *curleft;
	const float *curright;

	long prebufsamples = MAX_ORDER;
	long batchsamples = (long)num_samples;
	long cursamplepos = 0;

	if(!stereo)
		right_samples = left_samples;

	if((size_t)prebufsamples > num_samples)
		prebufsamples = (long)num_samples;

	for(long i = 0; i < prebufsamples; ++i) {
		_linprebuf[i + MAX_ORDER] = left_samples [i * downsample];
		_rinprebuf[i + MAX_ORDER] = right_samples[i * downsample];
	}

	while(batchsamples > 0) {
		long cursamples = MIN((long)(_sampleWindow - _totsamp), batchsamples);
		if(cursamplepos < MAX_ORDER) {
			downsample = 1;
			curleft  = _linpre + cursamplepos;
			curright = _rinpre + cursamplepos;
			if(cursamples > MAX_ORDER - cursamplepos)
				cursamples = MAX_ORDER - cursamplepos;
		}
		else {
			downsample = _filter.downsample;
			curleft  = left_samples  + cursamplepos;
			curright = right_samples + cursamplepos;
		}

		Filter(curleft , _lstep + _totsamp, (size_t)cursamples, _filter.AYule, _filter.BYule, YULE_ORDER, downsample);
		Filter(curright, _rstep + _totsamp, (size_t)cursamples, _filter.AYule, _filter.BYule, YULE_ORDER, downsample);

		Filter(_lstep + _totsamp, _lout + _totsamp, (size_t)cursamples, _filter.AButter, _filter.BButter, BUTTER_ORDER, downsample);
		Filter(_rstep + _totsamp, _rout + _totsamp, (size_t)cursamples, _filter.AButter, _filter.BButter, BUTTER_ORDER, downsample);

		/* Get the squared values */
		float sum;
		vDSP_svesq(_lout + _totsamp, 1, &sum, (vDSP_Length)cursamples);
		_lsum += sum;

		vDSP_svesq(_rout + _totsamp, 1, &sum, (vDSP_Length)cursamples);
		_rsum += sum;

		batchsamples -= cursamples;
		cursamplepos += cursamples;
		_totsamp += (uint32_t)cursamples;

		/* Get the Root Mean Square (RMS) for this set of samples */
		if(_totsamp == _sampleWindow) {
			double  val  = STEPS_per_dB * 10. * log10((_lsum + _rsum) / _totsamp * 0.5 + 1.e-37);
			int     ival = (int) val;
			if(ival < 0)
				ival = 0;
			if(ival >= (int)(sizeof(_A)/sizeof(*_A)))
				ival = (int)(sizeof(_A)/sizeof(*_A)) - 1;

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
		memmove(_linprebuf,                           _linprebuf + num_samples, (MAX_ORDER - num_samples) * sizeof(float));
		memmove(_rinprebuf,                           _rinprebuf + num_samples, (MAX_ORDER - num_samples) * sizeof(float));
		memcpy (_linprebuf + MAX_ORDER - num_samples, left_samples,             num_samples               * sizeof(float));
		memcpy (_rinprebuf + MAX_ORDER - num_samples, right_samples,            num_samples               * sizeof(float));
	}
	else {
		downsample = _filter.downsample;

		left_samples  += (num_samples - MAX_ORDER) * downsample;
		right_samples += (num_samples - MAX_ORDER) * downsample;

		for(long i = 0; i < MAX_ORDER; ++i) {
			_linprebuf[i] = left_samples [i * downsample];
			_rinprebuf[i] = right_samples[i * downsample];
		}
	}

    return YES;
}

@end
