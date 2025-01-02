//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <algorithm>
#import <cmath>
#import <cstring>
#import <vector>

#import <libkern/OSByteOrder.h>

#import <os/log.h>

#import <AVAudioPCMBuffer+SFBBufferUtilities.h>

#import "SFBShortenDecoder.h"

#import "NSData+SFBExtensions.h"
#import "NSError+SFBURLPresentation.h"

SFBAudioDecoderName const SFBAudioDecoderNameShorten = @"org.sbooth.AudioEngine.Decoder.Shorten";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenVersion = @"_version";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenInternalFileType = @"_internalFileType";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenNumberChannels = @"_channelCount";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBlockSize = @"_blocksize";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenSampleRate = @"_sampleRate";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBitsPerSample = @"_bitsPerSample";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBigEndian = @"_bigEndian";

namespace {

// MARK: Constants

constexpr auto kMinSupportedVersion 		= 1;
constexpr auto kMaxSupportedVersion 		= 3;

constexpr auto kDefaultBlockSize 			= 256;
constexpr auto kDefaultV0NMean 				= 0;
constexpr auto kDefaultV2NMean 				= 4;
constexpr auto kDefaultMaxNLPC 				= 0;


constexpr auto kChannelCountCodeSize 		= 0;
constexpr auto kEnergyCodeSize 				= 3;
constexpr auto kBitshiftCodeSize 			= 2;
constexpr auto kNWrap 						= 3;

constexpr auto kFunctionCodeSize 			= 2;
constexpr auto kFunctionDiff0 				= 0;
constexpr auto kFunctionDiff1 				= 1;
constexpr auto kFunctionDiff2 				= 2;
constexpr auto kFunctionDiff3 				= 3;
constexpr auto kFunctionQuit 				= 4;
constexpr auto kFunctionBlocksize 			= 5;
constexpr auto kFunctionBitshfit 			= 6;
constexpr auto kFunctionQLPC 				= 7;
constexpr auto kFunctionZero 				= 8;
constexpr auto kFunctionVerbatim 			= 9;

constexpr auto kVerbatimChunkSizeCodeSize 	= 5;
constexpr auto kVerbatimByteCodeSize 		= 8;
constexpr auto kVerbatimChunkMaxSizeBytes	= 256;

constexpr auto kUInt32CodeSize 				= 2;
constexpr auto kSkipBytesCodeSize 			= 1;
constexpr auto kLPCQuantCodeSize 			= 2;
constexpr auto kExtraByteCodeSize 			= 7;

constexpr auto kFileTypeCodeSize			= 4;
constexpr auto kFileTypeSInt8 				= 1;
constexpr auto kFileTypeUInt8 				= 2;
constexpr auto kFileTypeSInt16BE 			= 3;
constexpr auto kFileTypeUInt16BE 			= 4;
constexpr auto kFileTypeSInt16LE 			= 5;
constexpr auto kFileTypeUInt16LE 			= 6;
constexpr auto kFileTypeµLaw 				= 7;
constexpr auto kFileTypeALaw 				= 10;

constexpr auto kSeekTableRevision 			= 1;

constexpr auto kSeekHeaderSizeBytes 		= 12;
constexpr auto kSeekTrailerSizeBytes 		= 12;
constexpr auto kSeekEntrySizeBytes 			= 80;

constexpr auto kV2LPCQuantOffset 			= (1 << kLPCQuantCodeSize);

constexpr auto kMaxChannelCount 			= 8;
constexpr auto kMaxBlocksizeBytes			= 65535;

constexpr auto kCanonicalHeaderSizeBytes	= 44;

constexpr auto kWAVEFormatPCMTag 			= 0x0001;

constexpr int32_t RoundedShiftDown(int32_t x, int k) noexcept
{
	return (k == 0) ? x : (x >> (k - 1)) >> 1;
}

/// Returns a two-dimensional `rows` x `cols` array using one allocation from `malloc`
template <typename T>
T ** AllocateContiguous2DArray(size_t rows, size_t cols) noexcept
{
	T **result = static_cast<T **>(std::malloc((rows * sizeof(T *)) + (rows * cols * sizeof(T))));
	if(!result)
		return nullptr;
	T *tmp = reinterpret_cast<T *>(result + rows);
	for(size_t i = 0; i < rows; ++i)
		result[i] = tmp + i * cols;
	return result;
}

/// Variable-length input using Golomb-Rice coding
class VariableLengthInput {
public:
	static constexpr uint32_t sMaskTable [] = {
		0x0,
		0x1,		0x3,		0x7,		0xf,
		0x1f,		0x3f,		0x7f,		0xff,
		0x1ff,		0x3ff,		0x7ff,		0xfff,
		0x1fff,		0x3fff,		0x7fff,		0xffff,
		0x1ffff,	0x3ffff,	0x7ffff,	0xfffff,
		0x1fffff,	0x3fffff,	0x7fffff,	0xffffff,
		0x1ffffff,	0x3ffffff,	0x7ffffff,	0xfffffff,
		0x1fffffff,	0x3fffffff,	0x7fffffff,	0xffffffff
	};

	static constexpr size_t sizeof_uvar(uint32_t val, size_t nbin) noexcept
	{
		return (val >> nbin) + nbin;
	}

	static constexpr size_t sizeof_var(int32_t val, size_t nbin) noexcept
	{
		return static_cast<size_t>(labs(val) >> nbin) + nbin + 1;
	}

	/// Creates an empty `VariableLengthInput` object
	/// - important: `Allocate()` must be called before using
	VariableLengthInput() noexcept = default;

	~VariableLengthInput()
	{
		delete [] mByteBuffer;
	}

	VariableLengthInput(const VariableLengthInput&) = delete;
	VariableLengthInput(VariableLengthInput&&) = delete;
	VariableLengthInput& operator=(const VariableLengthInput&) = delete;
	VariableLengthInput& operator=(VariableLengthInput&&) = delete;

	/// Input callback type
	using InputBlock = bool(^)(void *buf, size_t len, size_t& read);

	/// Sets the input callback
	void SetInputCallback(InputBlock block) noexcept
	{
		mInputBlock = block;
	}

	/// Allocates an internal buffer of the specified size
	/// - warning: Sizes other than `512` will break seeking
	bool Allocate(size_t size = 512) noexcept
	{
		if(mByteBuffer)
			return false;

		auto byteBuffer = new (std::nothrow) uint8_t [size];
		if(!byteBuffer)
			return false;

		mByteBuffer = byteBuffer;
		mByteBufferPosition = mByteBuffer;
		mSize = size;

		return true;
	}

	bool GetRiceGolombCode(int32_t& i32, int k) noexcept
	{
		if(mBitsAvailable == 0 && !RefillBitBuffer())
			return false;

		int32_t result;
		for(result = 0; !(mBitBuffer & (1L << --mBitsAvailable)); ++result) {
			if(mBitsAvailable == 0 && !RefillBitBuffer())
				return false;
		}

		while(k != 0) {
			if(mBitsAvailable >= k) {
				result = (result << k) | static_cast<int32_t>((mBitBuffer >> (mBitsAvailable - k)) & sMaskTable[k]);
				mBitsAvailable -= k;
				k = 0;
			}
			else {
				result = (result << mBitsAvailable) | static_cast<int32_t>(mBitBuffer & sMaskTable[mBitsAvailable]);
				k -= mBitsAvailable;
				if(!RefillBitBuffer())
					return false;
			}
		}

		i32 = result;
		return true;
	}

	bool GetInt32(int32_t& i32, int k) noexcept
	{
		int32_t var;
		if(!GetRiceGolombCode(var, k + 1))
			return false;

		uint32_t uvar = static_cast<uint32_t>(var);
		if(uvar & 1)
			i32 = ~(uvar >> 1);
		else
			i32 = (uvar >> 1);
		return true;
	}

	bool GetUInt32(uint32_t& ui32, int version, int k) noexcept
	{
		if(version > 0 && !GetRiceGolombCode(k, kUInt32CodeSize))
			return false;

		int32_t i32;
		if(!GetRiceGolombCode(i32, k))
			return false;
		ui32 = static_cast<uint32_t>(i32);
		return true;
	}

	void Reset() noexcept
	{
		mByteBufferPosition = mByteBuffer;
		mBytesAvailable = 0;
		mBitsAvailable = 0;
	}

	bool Refill() noexcept
	{
		size_t bytesRead = 0;
		if(!mInputBlock || !mInputBlock(mByteBuffer, mSize, bytesRead) || bytesRead < 4)
			return false;
		mBytesAvailable += bytesRead;
		mByteBufferPosition = mByteBuffer;
		return true;
	}

	bool SetState(uint16_t byteBufferPosition, uint16_t bytesAvailable, uint32_t bitBuffer, uint16_t bitsAvailable) noexcept
	{
		if(byteBufferPosition > mBytesAvailable || bytesAvailable > mBytesAvailable - byteBufferPosition || bitsAvailable > 32)
			return false;
		mByteBufferPosition = mByteBuffer + byteBufferPosition;
		mBytesAvailable = bytesAvailable;
		mBitBuffer = bitBuffer;
		mBitsAvailable = bitsAvailable;
		return true;
	}

private:
	/// Input callback
	InputBlock mInputBlock = nil;
	/// Size of `mByteBuffer` in bytes
	size_t mSize = 0;
	/// Byte buffer
	uint8_t *mByteBuffer = nullptr;
	/// Current position in `mByteBuffer`
	uint8_t *mByteBufferPosition = nullptr;
	/// Bytes available in `mByteBuffer`
	int mBytesAvailable = 0;
	/// Bit buffer
	uint32_t mBitBuffer = 0;
	/// Bits available in `mBitBuffer`
	int mBitsAvailable = 0;

	/// Reads a single `uint32_t` from the byte buffer, refilling if necessary
	bool RefillBitBuffer() noexcept
	{
		if(mBytesAvailable < 4 && !Refill())
			return false;

		mBitBuffer = static_cast<uint32_t>((static_cast<int32_t>(mByteBufferPosition[0]) << 24) | (static_cast<int32_t>(mByteBufferPosition[1]) << 16) | (static_cast<int32_t>(mByteBufferPosition[2]) << 8) | static_cast<int32_t>(mByteBufferPosition[3]));

		mByteBufferPosition += 4;
		mBytesAvailable -= 4;
		mBitsAvailable = 32;

		return true;
	}

};

/// Shorten seek table header
struct SeekTableHeader
{
	int8_t mSignature [4];
	uint32_t mVersion;
	uint32_t mFileSize;
};

SeekTableHeader ParseSeekTableHeader(const void *buf)
{
	SeekTableHeader header;
	std::memcpy(header.mSignature, buf, 4);
	header.mVersion = OSReadLittleInt32(buf, 4);
	header.mFileSize = OSReadLittleInt32(buf, 8);

	return header;
}

/// Shorten seek table trailer
struct SeekTableTrailer
{
	uint32_t mSeekTableSize;
	int8_t mSignature [8];
};

SeekTableTrailer ParseSeekTableTrailer(const void *buf)
{
	SeekTableTrailer trailer;
	trailer.mSeekTableSize = OSReadLittleInt32(buf, 0);
	std::memcpy(trailer.mSignature, static_cast<const uint8_t *>(buf) + 4, 8);

	return trailer;
}

/// A Shorten seek table entry
struct SeekTableEntry
{
	uint32_t mFrameNumber;
	uint32_t mByteOffsetInFile;
	uint32_t mLastBufferReadPosition;
	uint16_t mBytesAvailable;
	uint16_t mByteBufferPosition;
	uint16_t mBitBufferPosition;
	uint32_t mBitBuffer;
	uint16_t mBitshift;
	int32_t mCBuf0 [3];
	int32_t mCBuf1 [3];
	int32_t mOffset0 [4];
	int32_t mOffset1 [4];
};

SeekTableEntry ParseSeekTableEntry(const void *buf)
{
	SeekTableEntry entry;
	entry.mFrameNumber = OSReadLittleInt32(buf, 0);
	entry.mByteOffsetInFile = OSReadLittleInt32(buf, 4);
	entry.mLastBufferReadPosition = OSReadLittleInt32(buf, 8);
	entry.mBytesAvailable = OSReadLittleInt16(buf, 12);
	entry.mByteBufferPosition = OSReadLittleInt16(buf, 14);
	entry.mBitBufferPosition = OSReadLittleInt16(buf, 16);
	entry.mBitBuffer = OSReadLittleInt32(buf, 18);
	entry.mBitshift = OSReadLittleInt16(buf, 22);
	for(auto i = 0; i < 3; ++i)
		entry.mCBuf0[i] = static_cast<int32_t>(OSReadLittleInt32(buf, 24 + 4 * i));
	for(auto i = 0; i < 3; ++i)
		entry.mCBuf1[i] = static_cast<int32_t>(OSReadLittleInt32(buf, 36 + 4 * i));
	for(auto i = 0; i < 4; ++i)
		entry.mOffset0[i] = static_cast<int32_t>(OSReadLittleInt32(buf, 48 + 4 * i));
	for(auto i = 0; i < 4; ++i)
		entry.mOffset1[i] = static_cast<int32_t>(OSReadLittleInt32(buf, 64 + 4 * i));

	return entry;
}

/// Locates the most suitable seek table entry for `frame`
std::vector<SeekTableEntry>::const_iterator FindSeekTableEntry(std::vector<SeekTableEntry>::const_iterator begin, std::vector<SeekTableEntry>::const_iterator end, AVAudioFramePosition frame)
{
	auto it = std::upper_bound(begin, end, frame, [](AVAudioFramePosition value, const SeekTableEntry& entry) {
		return value < entry.mFrameNumber;
	});
	return it == begin ? end : --it;
}

/// Decodes a µ-law sample to a linear value.
constexpr int16_t µLawToLinear(uint8_t µLaw) noexcept
{
	const auto bias = 0x84;

	µLaw = ~µLaw;
	int t = (((µLaw & 0x0F) << 3) + bias) << (static_cast<int>(µLaw & 0x70) >> 4);
	return static_cast<int16_t>((µLaw & 0x80) ? (bias - t) : (t - bias));
}

/// Decodes a A-law sample to a linear value.
constexpr int16_t ALawToLinear(uint8_t alaw) noexcept
{
	const auto mask = 0x55;

	alaw ^= mask;
	int i = (alaw & 0x0F) << 4;
	if(auto seg = static_cast<int>(alaw & 0x70) >> 4; seg)
		i = (i + 0x108) << (seg - 1);
	else
		i += 8;
	return static_cast<int16_t>((alaw & 0x80) ? i : -i);
}

} /* namespace */

@interface SFBShortenDecoder ()
{
@private
	VariableLengthInput _input;
	int _version;
	int32_t _lpcQuantOffset;
	int _internalFileType;
	int _channelCount;
	int _nmean;
	int _blocksize;
	int _maxnlpc;
	int _nwrap;

	uint32_t _sampleRate;
	uint32_t _bitsPerSample;
	bool _bigEndian;

	int32_t **_buffer;
	int32_t **_offset;
	int *_qlpc;
	int _bitshift;

	bool _eos;
	std::vector<SeekTableEntry> _seekTableEntries;

	AVAudioPCMBuffer *_frameBuffer;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
	uint64_t _blocksDecoded;
}
- (BOOL)parseShortenHeaderReturningError:(NSError **)error;
- (BOOL)parseRIFFChunk:(const uint8_t *)chunkData size:(size_t)size error:(NSError **)error;
- (BOOL)parseFORMChunk:(const uint8_t *)chunkData size:(size_t)size error:(NSError **)error;
- (BOOL)decodeBlockReturningError:(NSError **)error;
- (BOOL)scanForSeekTableReturningError:(NSError **)error;
- (std::vector<SeekTableEntry>)parseExternalSeekTable:(NSURL *)url;
- (BOOL)seekTableIsValid:(std::vector<SeekTableEntry>)entries startOffset:(NSInteger)startOffset;
@end

@implementation SFBShortenDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"shn"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/x-shorten"];
}

+ (SFBAudioDecoderName)decoderName
{
	return SFBAudioDecoderNameShorten;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);
	NSParameterAssert(formatIsSupported != NULL);

	NSData *header = [inputSource readHeaderOfLength:SFBShortenDetectionSize skipID3v2Tag:NO error:error];
	if(!header)
		return NO;

	if([header isShortenHeader])
		*formatIsSupported = SFBTernaryTruthValueTrue;
	else
		*formatIsSupported = SFBTernaryTruthValueFalse;

	return YES;
}

- (BOOL)decodingIsLossless
{
	return YES;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error] || ![self parseShortenHeaderReturningError:error])
		return NO;

	// Sanity checks
	if(_bitsPerSample != 8 && _bitsPerSample != 16) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth: %u", _bitsPerSample);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported bit depth", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's bit depth is not supported.", @"")];
		return NO;
	}

	if((_bitsPerSample == 8 && (_internalFileType != kFileTypeUInt8 && _internalFileType != kFileTypeSInt8 && _internalFileType != kFileTypeµLaw && _internalFileType != kFileTypeALaw)) || (_bitsPerSample == 16 && (_internalFileType != kFileTypeUInt16BE && _internalFileType != kFileTypeUInt16LE && _internalFileType != kFileTypeSInt16BE && _internalFileType != kFileTypeSInt16LE))) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth/audio type combination: %u, %u", _bitsPerSample, _internalFileType);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported bit depth/audio type combination", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's bit depth and audio type is not supported.", @"")];
		return NO;
	}

	if(![self scanForSeekTableReturningError:error])
		return NO;

	// Set up the processing format
	AudioStreamBasicDescription processingStreamDescription{};

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
	// Apparently *16BE isn't true for 'AIFF'
//	if(_internalFileType == kFileTypeUInt16BE || _internalFileType == kFileTypeSInt16BE)
	if(_bigEndian)
		processingStreamDescription.mFormatFlags	|= kAudioFormatFlagIsBigEndian;
	if(_internalFileType == kFileTypeSInt8 || _internalFileType == kFileTypeSInt16BE || _internalFileType == kFileTypeSInt16LE)
		processingStreamDescription.mFormatFlags	|= kAudioFormatFlagIsSignedInteger;
	if(_internalFileType != kFileTypeµLaw || _internalFileType != kFileTypeALaw)
		processingStreamDescription.mFormatFlags	|= kAudioFormatFlagIsSignedInteger;

	processingStreamDescription.mSampleRate			= _sampleRate;
	processingStreamDescription.mChannelsPerFrame	= static_cast<UInt32>(_channelCount);
	processingStreamDescription.mBitsPerChannel		= _bitsPerSample;

	processingStreamDescription.mBytesPerPacket		= (_bitsPerSample + 7) / 8;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

	AVAudioChannelLayout *channelLayout = nil;
	switch(_channelCount) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
			// FIXME: Is there a standard ordering for multichannel files? WAVEFORMATEX?
		default:
			channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | _channelCount)];
			break;
	}

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription{};

	sourceStreamDescription.mFormatID			= kSFBAudioFormatShorten;

	sourceStreamDescription.mSampleRate			= _sampleRate;
	sourceStreamDescription.mChannelsPerFrame	= static_cast<UInt32>(_channelCount);
	sourceStreamDescription.mBitsPerChannel		= _bitsPerSample;

	sourceStreamDescription.mFramesPerPacket	= static_cast<UInt32>(_blocksize);

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	// Populate codec properties
	_properties = @{
		SFBAudioDecodingPropertiesKeyShortenVersion: @(_version),
		SFBAudioDecodingPropertiesKeyShortenInternalFileType: @(_internalFileType),
		SFBAudioDecodingPropertiesKeyShortenNumberChannels: @(_channelCount),
		SFBAudioDecodingPropertiesKeyShortenBlockSize: @(_blocksize),
		SFBAudioDecodingPropertiesKeyShortenSampleRate: @(_sampleRate),
		SFBAudioDecodingPropertiesKeyShortenBitsPerSample: @(_bitsPerSample),
		SFBAudioDecodingPropertiesKeyShortenBigEndian: _bigEndian ? @YES : @NO,
	};

	_frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:static_cast<AVAudioFrameCount>(_blocksize)];

	// Allocate decoding buffers
	_buffer = AllocateContiguous2DArray<int32_t>(static_cast<size_t>(_channelCount), static_cast<size_t>(_blocksize + _nwrap));
	if(!_buffer) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	_offset = AllocateContiguous2DArray<int32_t>(static_cast<size_t>(_channelCount), static_cast<size_t>(std::max(1, _nmean)));
	if(!_offset) {
		std::free(_buffer);
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	for(auto i = 0; i < _channelCount; ++i) {
		for(auto j = 0; j < _nwrap; ++j) {
			_buffer[i][j] = 0;
		}
		_buffer[i] += _nwrap;
	}

	if(_maxnlpc > 0)
		_qlpc = new int [static_cast<size_t>(_maxnlpc)];

	// Initialize offset
	int32_t mean = 0;
	switch(_internalFileType) {
		case kFileTypeSInt8:
		case kFileTypeSInt16BE:
		case kFileTypeSInt16LE:
		case kFileTypeµLaw:
		case kFileTypeALaw:
			mean = 0;
			break;
		case kFileTypeUInt8:
			mean = 0x80;
			break;
		case kFileTypeUInt16BE:
		case kFileTypeUInt16LE:
			mean = 0x8000;
			break;
		default:
			os_log_error(gSFBAudioDecoderLog, "Unsupported audio type: %u", _internalFileType);
			return NO;
	}

	for(auto chan = 0; chan < _channelCount; ++chan) {
		for(auto i = 0; i < std::max(1, _nmean); ++i) {
			_offset[chan][i] = mean;
		}
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_buffer) {
		std::free(_buffer);
		_buffer = nullptr;
	}
	if(_offset) {
		std::free(_offset);
		_offset = nullptr;
	}
	if(_qlpc) {
		delete [] _qlpc;
		_qlpc = nullptr;
	}

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _buffer != nullptr;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return _frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	if(frameLength == 0)
		return YES;

	AVAudioFrameCount framesProcessed = 0;

	for(;;) {
		AVAudioFrameCount framesRemaining = frameLength - framesProcessed;
		AVAudioFrameCount framesCopied = [buffer appendFromBuffer:_frameBuffer readingFromOffset:0 frameLength:framesRemaining];
		[_frameBuffer trimAtOffset:0 frameLength:framesCopied];

		framesProcessed += framesCopied;

		// All requested frames were read or EOS reached
		if(framesProcessed == frameLength || _eos)
			break;

		// Decode the next _blocksize frames
		if(![self decodeBlockReturningError:error]) {
			os_log_error(gSFBAudioDecoderLog, "Error decoding Shorten block");
			return NO;
		}
	}

	_framePosition += framesProcessed;

	return YES;
}

- (BOOL)supportsSeeking
{
	return !_seekTableEntries.empty();
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	if(frame >= self.frameLength)
		return NO;

	auto entry = FindSeekTableEntry(_seekTableEntries.cbegin(), _seekTableEntries.cend(), frame);
	if(entry == _seekTableEntries.end()) {
		os_log_error(gSFBAudioDecoderLog, "No seek table entry for frame %lld", frame);
		return NO;
	}

#if DEBUG
	os_log_debug(gSFBAudioDecoderLog, "Using seek table entry %ld for frame %d to seek to frame %lld", std::distance(_seekTableEntries.cbegin(), entry), entry->mFrameNumber, frame);
#endif

	if(![_inputSource seekToOffset:entry->mLastBufferReadPosition error:error])
		return NO;

	_input.Reset();
	if(!_input.Refill() || !_input.SetState(entry->mByteBufferPosition, entry->mBytesAvailable, entry->mBitBuffer, entry->mBitBufferPosition))
		return NO;

	_buffer[0][-1] = entry->mCBuf0[0];
	_buffer[0][-2] = entry->mCBuf0[1];
	_buffer[0][-3] = entry->mCBuf0[2];
	if(_channelCount == 2) {
		_buffer[1][-1] = entry->mCBuf1[0];
		_buffer[1][-2] = entry->mCBuf1[1];
		_buffer[1][-3] = entry->mCBuf1[2];
	}

	for(auto i = 0; i < std::max(1, _nmean); ++i) {
		_offset[0][i] = entry->mOffset0[i];
		if(_channelCount == 2)
			_offset[1][i] = entry->mOffset1[i];
	}

	_bitshift = entry->mBitshift;

	_framePosition = entry->mFrameNumber;
	_frameBuffer.frameLength = 0;

	AVAudioFrameCount framesToSkip = static_cast<AVAudioFrameCount>(frame - entry->mFrameNumber);
	AVAudioFrameCount framesSkipped = 0;

	for(;;) {
		// Decode the next _blocksize frames
		if(![self decodeBlockReturningError:error])
			os_log_error(gSFBAudioDecoderLog, "Error decoding Shorten block");

		AVAudioFrameCount framesToTrim = std::min(framesToSkip - framesSkipped, _frameBuffer.frameLength);
		[_frameBuffer trimAtOffset:0 frameLength:framesToTrim];

		framesSkipped += framesToTrim;

		// All requested frames were skipped or EOS reached
		if(framesSkipped == framesToSkip || _eos)
			break;
	}

	_framePosition += framesSkipped;

	return YES;
}

- (BOOL)parseShortenHeaderReturningError:(NSError **)error
{
	// Read magic number
	uint32_t magic;
	if(![_inputSource readUInt32BigEndian:&magic error:nil] || magic != 'ajkg') {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// Read file version
	uint8_t version;
	if(![_inputSource readUInt8:&version error:nil] || version < kMinSupportedVersion || version > kMaxSupportedVersion) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported version: %u", version);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Version not supported", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's version is not supported.", @"")];
		return NO;
	}
	_version = version;

	// Default nmean
	_nmean = _version < 2 ? kDefaultV0NMean : kDefaultV2NMean;

	// Set up variable length input
	if(!_input.Allocate()) {
		os_log_error(gSFBAudioDecoderLog, "Unable to allocate variable-length input");
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	__weak SFBInputSource *inputSource = self->_inputSource;
	_input.SetInputCallback(^bool(void *buf, size_t len, size_t &read) {
		NSInteger bytesRead;
		if(![inputSource readBytes:buf length:static_cast<NSInteger>(len) bytesRead:&bytesRead error:nil])
			return false;
		read = static_cast<size_t>(bytesRead);
		return true;
	});

	// Read internal file type
	uint32_t internalFileType;
	if(!_input.GetUInt32(internalFileType, _version, kFileTypeCodeSize)) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}
	if(internalFileType != kFileTypeUInt8 && internalFileType != kFileTypeSInt8 && internalFileType != kFileTypeUInt16BE && internalFileType != kFileTypeUInt16LE && internalFileType != kFileTypeSInt16BE && internalFileType != kFileTypeSInt16LE) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported audio type: %u", internalFileType);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Invalid or unsupported audio type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported audio type.", @"")];
		return NO;
	}
	_internalFileType = static_cast<int>(internalFileType);

	// Read number of channels
	uint32_t channelCount = 0;
	if(!_input.GetUInt32(channelCount, _version, kChannelCountCodeSize) || channelCount == 0 || channelCount > kMaxChannelCount) {
		os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported channel count: %u", channelCount);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Invalid or unsupported number of channels", @"")
							   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported number of channels.", @"")];
		return NO;
	}
	_channelCount = static_cast<int>(channelCount);

	// Read blocksize if version > 0
	if(_version > 0) {
		uint32_t blocksize = 0;
		if(!_input.GetUInt32(blocksize, _version, static_cast<int>(std::log2(kDefaultBlockSize))) || blocksize == 0 || blocksize > kMaxBlocksizeBytes) {
			os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported block size: %u", blocksize);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Invalid or unsupported block size", @"")
								   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported block size.", @"")];
			return NO;
		}
		_blocksize = static_cast<int>(blocksize);

		uint32_t maxnlpc = 0;
		if(!_input.GetUInt32(maxnlpc, _version, kLPCQuantCodeSize) || maxnlpc > 1024) {
			os_log_error(gSFBAudioDecoderLog, "Invalid maxnlpc: %u", maxnlpc);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
		_maxnlpc = static_cast<int>(maxnlpc);

		uint32_t nmean = 0;
		if(!_input.GetUInt32(nmean, _version, 0) || nmean > 32768) {
			os_log_error(gSFBAudioDecoderLog, "Invalid nmean: %u", nmean);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
		_nmean = static_cast<int>(nmean);

		uint32_t skipCount;
		if(!_input.GetUInt32(skipCount, _version, kSkipBytesCodeSize) /* || nskip > bits_remaining_in_input */) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		for(uint32_t i = 0; i < skipCount; ++i) {
			uint32_t dummy;
			if(!_input.GetUInt32(dummy, _version, kExtraByteCodeSize)) {
				if(error)
					*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
													 code:SFBAudioDecoderErrorCodeInvalidFormat
							descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
													  url:_inputSource.url
											failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
									   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
				return NO;
			}
		}
	}
	else {
		_blocksize = kDefaultBlockSize;
		_maxnlpc = kDefaultMaxNLPC;
	}

	_nwrap = std::max(kNWrap, static_cast<int>(_maxnlpc));

	if(_version > 1)
		_lpcQuantOffset = kV2LPCQuantOffset;

	// Parse the WAVE or AIFF header in the verbatim section

	int32_t function;
	if(!_input.GetRiceGolombCode(function, kFunctionCodeSize) || function != kFunctionVerbatim) {
		os_log_error(gSFBAudioDecoderLog, "Missing initial verbatim section");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Missing initial verbatim section", @"")
							   recoverySuggestion:NSLocalizedString(@"The file is missing the initial verbatim section.", @"")];
		return NO;
	}

	int32_t headerSize;
	if(!_input.GetRiceGolombCode(headerSize, kVerbatimChunkSizeCodeSize) || headerSize < kCanonicalHeaderSizeBytes || headerSize > kVerbatimChunkMaxSizeBytes) {
		os_log_error(gSFBAudioDecoderLog, "Incorrect header size: %u", headerSize);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	uint8_t headerBytes [headerSize];
	for(int32_t i = 0; i < headerSize; ++i) {
		int32_t byte;
		if(!_input.GetRiceGolombCode(byte, kVerbatimByteCodeSize)) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		headerBytes[i] = static_cast<uint8_t>(byte);
	}

	// header_bytes is at least kCanonicalHeaderSizeBytes (44) in size

	auto chunkID = OSReadBigInt32(headerBytes, 0);
//	auto chunkSize = OSReadBigInt32(header_bytes, 4);

	// WAVE
	if(chunkID == 'RIFF') {
		if(![self parseRIFFChunk:(headerBytes + 8) size:(headerSize - 8) error:error])
			return NO;
	}
	// AIFF
	else if(chunkID == 'FORM') {
		if(![self parseFORMChunk:(headerBytes + 8) size:(headerSize - 8) error:error])
			return NO;
	}
	else {
		os_log_error(gSFBAudioDecoderLog, "Unsupported data format: %u", chunkID);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported data format", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's data format is not supported.", @"")];
		return NO;
	}

	return YES;
}

- (BOOL)parseRIFFChunk:(const uint8_t *)chunkData size:(size_t)size error:(NSError **)error
{
	NSParameterAssert(chunkData != nullptr);
	NSParameterAssert(size >= 28);

	uintptr_t offset = 0;

	auto chunkID = OSReadBigInt32(chunkData, offset);
	offset += 4;
	if(chunkID != 'WAVE') {
		os_log_error(gSFBAudioDecoderLog, "Missing 'WAVE' in 'RIFF' chunk");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	auto sawFormatChunk = false;
	uint32_t dataChunkSize = 0;
	uint16_t blockAlign = 0;

	while(offset < size) {
		chunkID = OSReadBigInt32(chunkData, offset);
		offset += 4;

		auto chunkSize = OSReadLittleInt32(chunkData, offset);
		offset += 4;

		switch(chunkID) {
			case 'fmt ':
			{
				if(chunkSize < 16) {
					os_log_error(gSFBAudioDecoderLog, "'fmt ' chunk is too small (%u bytes)", chunkSize);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
										   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
					return NO;
				}

				auto formatTag = OSReadLittleInt16(chunkData, offset);
				offset += 2;
				if(formatTag != kWAVEFormatPCMTag) {
					os_log_error(gSFBAudioDecoderLog, "Unsupported WAVE format tag: %x", formatTag);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Unsupported WAVE format tag", @"")
										   recoverySuggestion:NSLocalizedString(@"The file's WAVE format tag is not supported.", @"")];
					return NO;
				}

				auto channels = OSReadLittleInt16(chunkData, offset);
				offset += 2;
				if(_channelCount != channels)
					os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'fmt ' chunk (%u)", _channelCount, channels);

				_sampleRate = OSReadLittleInt32(chunkData, offset);
				offset += 4;

				// Skip average bytes per second
				offset += 4;

				blockAlign = OSReadLittleInt16(chunkData, offset);
				offset += 2;

				_bitsPerSample = OSReadLittleInt16(chunkData, offset);
				offset += 2;

				if(chunkSize > 16)
					os_log_info(gSFBAudioDecoderLog, "%u bytes in 'fmt ' chunk not parsed", chunkSize - 16);

				sawFormatChunk = true;

				break;
			}

			case 'data':
				dataChunkSize = chunkSize;
				break;
		}
	}

	if(!sawFormatChunk) {
		os_log_error(gSFBAudioDecoderLog, "Missing 'fmt ' chunk");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	if(dataChunkSize && blockAlign)
		_frameLength = dataChunkSize / blockAlign;

	return YES;
}

- (BOOL)parseFORMChunk:(const uint8_t *)chunkData size:(size_t)size error:(NSError **)error
{
	NSParameterAssert(chunkData != nullptr);
	NSParameterAssert(size >= 30);

	uintptr_t offset = 0;

	auto chunkID = OSReadBigInt32(chunkData, offset);
	offset += 4;
	if(chunkID != 'AIFF' && chunkID != 'AIFC') {
		os_log_error(gSFBAudioDecoderLog, "Missing 'AIFF' or 'AIFC' in 'FORM' chunk");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	if(chunkID == 'AIFC')
		_bigEndian = true;

	auto sawCommonChunk = false;
	while(offset < size) {
		chunkID = OSReadBigInt32(chunkData, offset);
		offset += 4;

		auto chunkSize = OSReadBigInt32(chunkData, offset);
		offset += 4;

		// All chunks must have an even length but the pad byte is not included in ckSize
		chunkSize += (chunkSize & 1);

		switch(chunkID) {
			case 'COMM':
			{
				if(chunkSize < 18) {
					os_log_error(gSFBAudioDecoderLog, "'COMM' chunk is too small (%u bytes)", chunkSize);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
										   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
					return NO;
				}

				auto channels = OSReadBigInt16(chunkData, offset);
				offset += 2;
				if(_channelCount != channels)
					os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'COMM' chunk (%u)", _channelCount, channels);

				_frameLength = OSReadBigInt32(chunkData, offset);
				offset += 4;

				_bitsPerSample = OSReadBigInt16(chunkData, offset);
				offset += 2;

				// sample rate is IEEE 754 80-bit extended float (16-bit exponent, 1-bit integer part, 63-bit fraction)
				auto exp = static_cast<int16_t>(OSReadBigInt16(chunkData, offset)) - 16383 - 63;
				offset += 2;
				if(exp < -63 || exp > 63) {
					os_log_error(gSFBAudioDecoderLog, "exp out of range: %d", exp);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
										   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
					return NO;
				}

				auto frac = OSReadBigInt64(chunkData, offset);
				offset += 8;
				if(exp >= 0)
					_sampleRate = static_cast<uint32_t>(frac << exp);
				else
					_sampleRate = static_cast<uint32_t>((frac + (static_cast<uint64_t>(1) << (-exp - 1))) >> -exp);

				if(chunkSize > 18)
					os_log_info(gSFBAudioDecoderLog, "%u bytes in 'COMM' chunk not parsed", chunkSize - 16);

				sawCommonChunk = true;

				break;
			}

				// Skip all other chunks
			default:
				offset += chunkSize;
				break;
		}
	}

	if(!sawCommonChunk) {
		os_log_error(gSFBAudioDecoderLog, "Missing 'COMM' chunk");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	return YES;
}

- (BOOL)decodeBlockReturningError:(NSError **)error
{
	int chan = 0;
	for(;;) {
		int32_t cmd;
		if(!_input.GetRiceGolombCode(cmd, kFunctionCodeSize)) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		if(cmd == kFunctionQuit) {
			_eos = true;
			return YES;
		}

		switch(cmd) {
			case kFunctionZero:
			case kFunctionDiff0:
			case kFunctionDiff1:
			case kFunctionDiff2:
			case kFunctionDiff3:
			case kFunctionQLPC:
			{
				int32_t coffset, *cbuffer = _buffer[chan];
				int resn = 0, nlpc;

				if(cmd != kFunctionZero) {
					if(!_input.GetRiceGolombCode(resn, kEnergyCodeSize)) {
						if(error)
							*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
															 code:SFBAudioDecoderErrorCodeInvalidFormat
									descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
															  url:_inputSource.url
													failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
											   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
						return NO;
					}
					// Versions > 0 changed the behavior
					if(_version == 0)
						resn--;
				}

				if(_nmean == 0)
					coffset = _offset[chan][0];
				else {
					int32_t sum = (_version < 2) ? 0 : _nmean / 2;
					for(auto i = 0; i < _nmean; i++) {
						sum += _offset[chan][i];
					}
					if(_version < 2)
						coffset = sum / _nmean;
					else
						coffset = RoundedShiftDown(sum / _nmean, _bitshift);
				}

				switch(cmd) {
					case kFunctionZero:
						for(auto i = 0; i < _blocksize; ++i) {
							cbuffer[i] = 0;
						}
						break;
					case kFunctionDiff0:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.GetInt32(var, resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInvalidFormat
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + coffset;
						}
						break;
					case kFunctionDiff1:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.GetInt32(var, resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInvalidFormat
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + cbuffer[i - 1];
						}
						break;
					case kFunctionDiff2:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.GetInt32(var, resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInvalidFormat
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + (2 * cbuffer[i - 1] - cbuffer[i - 2]);
						}
						break;
					case kFunctionDiff3:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.GetInt32(var, resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInvalidFormat
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + 3 * (cbuffer[i - 1] -  cbuffer[i - 2]) + cbuffer[i - 3];
						}
						break;
					case kFunctionQLPC:
						if(!_input.GetRiceGolombCode(nlpc, kLPCQuantCodeSize)) {
							if(error)
								*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																 code:SFBAudioDecoderErrorCodeInvalidFormat
										descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																  url:_inputSource.url
														failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
												   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
							return NO;
						}

						for(auto i = 0; i < nlpc; ++i) {
							if(!_input.GetInt32(_qlpc[i], kLPCQuantCodeSize)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInvalidFormat
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
						}
						for(auto i = 0; i < nlpc; ++i) {
							cbuffer[i - nlpc] -= coffset;
						}
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t sum = _lpcQuantOffset;

							for(auto j = 0; j < nlpc; ++j) {
								sum += _qlpc[j] * cbuffer[i - j - 1];
							}
							int32_t var;
							if(!_input.GetInt32(var, resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInvalidFormat
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + (sum >> kLPCQuantCodeSize);
						}
						if(coffset != 0) {
							for(auto i = 0; i < _blocksize; ++i) {
								cbuffer[i] += coffset;
							}
						}
						break;
				}

				if(_nmean > 0) {
					int32_t sum = (_version < 2) ? 0 : _blocksize / 2;

					for(auto i = 0; i < _blocksize; ++i) {
						sum += cbuffer[i];
					}

					for(auto i = 1; i < _nmean; ++i) {
						_offset[chan][i - 1] = _offset[chan][i];
					}
					if(_version < 2)
						_offset[chan][_nmean - 1] = sum / _blocksize;
					else
						_offset[chan][_nmean - 1] = (sum / _blocksize) << _bitshift;
				}

				for(auto i = -_nwrap; i < 0; i++) {
					cbuffer[i] = cbuffer[i + _blocksize];
				}

				if(_bitshift != 0) {
					for(auto i = 0; i < _blocksize; ++i) {
						cbuffer[i] <<= _bitshift;
					}
				}

				if(chan == _channelCount - 1) {
					auto abl = _frameBuffer.audioBufferList;

					switch(_internalFileType) {
						case kFileTypeUInt8:
							for(auto channel = 0; channel < _channelCount; ++channel) {
								auto channel_buf = static_cast<uint8_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample)
									channel_buf[sample] = static_cast<uint8_t>(std::clamp(_buffer[channel][sample], 0, UINT8_MAX));
							}
							break;
						case kFileTypeSInt8:
							for(auto channel = 0; channel < _channelCount; ++channel) {
								auto channel_buf = static_cast<int8_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample)
									channel_buf[sample] = static_cast<int8_t>(std::clamp(_buffer[channel][sample], INT8_MIN, INT8_MAX));
							}
							break;
						case kFileTypeµLaw:
							for(auto channel = 0; channel < _channelCount; ++channel) {
								auto channel_buf = static_cast<int8_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample) {
									auto value = µLawToLinear(_buffer[channel][sample]);
									channel_buf[sample] = static_cast<int8_t>(std::clamp(value >> 3, INT8_MIN, INT8_MAX));
								}
							}
							break;
						case kFileTypeALaw:
							for(auto channel = 0; channel < _channelCount; ++channel) {
								auto channel_buf = static_cast<int8_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample) {
									auto value = ALawToLinear(_buffer[channel][sample]);
									channel_buf[sample] = static_cast<int8_t>(std::clamp(value >> 3, INT8_MIN, INT8_MAX));
								}
							}
							break;
						case kFileTypeUInt16BE:
						case kFileTypeUInt16LE:
							for(auto channel = 0; channel < _channelCount; ++channel) {
								auto channel_buf = static_cast<uint16_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample)
									channel_buf[sample] = static_cast<uint16_t>(std::clamp(_buffer[channel][sample], 0, UINT16_MAX));
							}
							break;
						case kFileTypeSInt16BE:
						case kFileTypeSInt16LE:
							for(auto channel = 0; channel < _channelCount; ++channel) {
								auto channel_buf = static_cast<int16_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample) {
									channel_buf[sample] = static_cast<int16_t>(std::clamp(_buffer[channel][sample], INT16_MIN, INT16_MAX));
								}
							}
							break;
					}

					_frameBuffer.frameLength = static_cast<AVAudioFrameCount>(_blocksize);

					++_blocksDecoded;
					return YES;
				}
				chan = (chan + 1) % _channelCount;
				break;
			}

			case kFunctionBlocksize:
			{
				uint32_t uint = 0;
				if(!_input.GetUInt32(uint, _version, static_cast<int>(std::log2(_blocksize))) || uint == 0 || uint > kMaxBlocksizeBytes || static_cast<int>(uint) > _blocksize) {
					os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported block size: %u", uint);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Invalid or unsupported block size", @"")
										   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported block size.", @"")];
					return NO;
				}
				_blocksize = static_cast<int>(uint);
				break;
			}
			case kFunctionBitshfit:
				if(!_input.GetRiceGolombCode(_bitshift, kBitshiftCodeSize) || _bitshift > 32) {
					os_log_error(gSFBAudioDecoderLog, "Invald or unsupported bitshift: %u", _bitshift);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Invalid or unsupported bitshift", @"")
										   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported bitshift.", @"")];
					return NO;
				}
				break;
			case kFunctionVerbatim:
			{
				int32_t chunk_len;
				if(!_input.GetRiceGolombCode(chunk_len, kVerbatimChunkSizeCodeSize) || chunk_len < 0 || chunk_len > kVerbatimChunkMaxSizeBytes) {
					os_log_error(gSFBAudioDecoderLog, "Invald verbatim length: %u", chunk_len);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
										   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
					return NO;
				}
				while(chunk_len--) {
					int32_t dummy;
					if(!_input.GetRiceGolombCode(dummy, kVerbatimByteCodeSize)) {
						if(error)
							*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
															 code:SFBAudioDecoderErrorCodeInvalidFormat
									descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
															  url:_inputSource.url
													failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
											   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
						return NO;
					}
				}
				break;
			}

			default:
				os_log_error(gSFBAudioDecoderLog, "sanity check failed for function: %d", cmd);
				if(error)
					*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
													 code:SFBAudioDecoderErrorCodeInvalidFormat
							descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
													  url:_inputSource.url
											failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
									   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
				return NO;
		}
	}

	return YES;
}

// A return value of YES indicates that decoding may continue, not that no errors exist with the seek table itself
- (BOOL)scanForSeekTableReturningError:(NSError **)error
{
	// Non-seekable input source; not an error
	if(!_inputSource.supportsSeeking)
		return YES;

	NSInteger startOffset;
	if(![_inputSource getOffset:&startOffset error:error])
		return NO;

	NSInteger fileLength;
	if(![_inputSource getLength:&fileLength error:error] || ![_inputSource seekToOffset:(fileLength - kSeekTrailerSizeBytes) error:error])
		return NO;

	SeekTableTrailer trailer;
	{
		uint8_t buf [kSeekTrailerSizeBytes];
		NSInteger bytesRead;
		if(![_inputSource readBytes:buf length:kSeekTrailerSizeBytes bytesRead:&bytesRead error:error] || bytesRead != kSeekTrailerSizeBytes)
			return NO;
		trailer = ParseSeekTableTrailer(buf);
	}

	// No appended seek table found; not an error
	if(memcmp("SHNAMPSK", trailer.mSignature, 8)) {
		// Check for separate seek table
		NSURL *externalSeekTableURL = [_inputSource.url.URLByDeletingPathExtension URLByAppendingPathExtension:@"skt"];
		if([externalSeekTableURL checkResourceIsReachableAndReturnError:nil]) {
			auto entries = [self parseExternalSeekTable:externalSeekTableURL];
			if(!entries.empty() && [self seekTableIsValid:entries startOffset:startOffset])
				_seekTableEntries = entries;
		}
		if(![_inputSource seekToOffset:startOffset error:error])
			return NO;
		return YES;
	}

	if(![_inputSource seekToOffset:(fileLength - trailer.mSeekTableSize) error:error])
		return NO;

	SeekTableHeader header;
	{
		uint8_t buf [kSeekHeaderSizeBytes];
		NSInteger bytesRead;
		if(![_inputSource readBytes:buf length:kSeekHeaderSizeBytes bytesRead:&bytesRead error:error] || bytesRead != kSeekHeaderSizeBytes)
			return NO;
		header = ParseSeekTableHeader(buf);
	}

	// A corrupt seek table is an error, however YES is returned to try and permit decoding to continue
	if(memcmp("SEEK", header.mSignature, 4)) {
		os_log_error(gSFBAudioDecoderLog, "Unexpected seek table header signature: %{public}.4s", header.mSignature);
		if(![_inputSource seekToOffset:startOffset error:error])
			return NO;
		return YES;
	}

	std::vector<SeekTableEntry> entries;

	auto count = (trailer.mSeekTableSize - kSeekTrailerSizeBytes - kSeekHeaderSizeBytes) / kSeekEntrySizeBytes;
	for(uint32_t i = 0; i < count; ++i) {
		uint8_t buf [kSeekEntrySizeBytes];
		NSInteger bytesRead;
		if(![_inputSource readBytes:buf length:kSeekEntrySizeBytes bytesRead:&bytesRead error:error] || bytesRead != kSeekEntrySizeBytes)
			return NO;

		auto entry = ParseSeekTableEntry(buf);
		entries.push_back(entry);
	}

	// Reset file marker
	if(![_inputSource seekToOffset:startOffset error:error])
		return NO;

	if(!entries.empty() && [self seekTableIsValid:entries startOffset:startOffset])
		_seekTableEntries = entries;

	return YES;
}

- (std::vector<SeekTableEntry>)parseExternalSeekTable:(NSURL *)url
{
	NSParameterAssert(url != nil);

	NSError *error;
	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:&error];
	if(!inputSource || ![inputSource openReturningError:&error]) {
		os_log_error(gSFBAudioDecoderLog, "Error opening external seek table: %{public}@", error);
		return {};
	}

	{
		uint8_t buf [kSeekHeaderSizeBytes];
		NSInteger bytesRead;
		if(![inputSource readBytes:buf length:kSeekHeaderSizeBytes bytesRead:&bytesRead error:&error] || bytesRead != kSeekHeaderSizeBytes) {
			os_log_error(gSFBAudioDecoderLog, "Error reading external seek table header: %{public}@", error);
			return {};
		}

		auto header = ParseSeekTableHeader(buf);
		if(memcmp("SEEK", header.mSignature, 4)) {
			os_log_error(gSFBAudioDecoderLog, "Unexpected seek table header signature: %{public}.4s", header.mSignature);
			return {};
		}
	}

	std::vector<SeekTableEntry> entries;

	for(;;) {
		uint8_t buf [kSeekEntrySizeBytes];
		NSInteger bytesRead;
		if(![inputSource readBytes:buf length:kSeekEntrySizeBytes bytesRead:&bytesRead error:&error] || bytesRead != kSeekEntrySizeBytes) {
			os_log_error(gSFBAudioDecoderLog, "Error reading external seek table entry: %{public}@", error);
			return {};
		}

		auto entry = ParseSeekTableEntry(buf);
		entries.push_back(entry);

		if(inputSource.atEOF)
			break;
	}

	return entries;
}

- (BOOL)seekTableIsValid:(std::vector<SeekTableEntry>)entries startOffset:(NSInteger)startOffset
{
	if(entries.empty())
		return NO;
	else if(startOffset != entries[0].mByteOffsetInFile) {
		os_log_error(gSFBAudioDecoderLog, "Seek table error: Mismatch between actual data start (%ld) and start in first seek table entry (%d)", (long)startOffset, entries[0].mByteOffsetInFile);
		return NO;
	}
	else if(_bitshift != entries[0].mBitshift) {
		os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid bitshift (%d) in first seek table entry", entries[0].mBitshift);
		return NO;
	}
	else if(_channelCount != 1 && _channelCount != 2) {
		os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid channel count (%d); mono or stereo required", _channelCount);
		return NO;
	}
	else if(_maxnlpc > 3) {
		os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid maxnlpc (%d); [0, 3] required", _maxnlpc);
		return NO;
	}
	else if(_nmean > 4) {
		os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid nmean (%d); [0, 4] required", _nmean);
		return NO;
	}

	return YES;
}

@end
