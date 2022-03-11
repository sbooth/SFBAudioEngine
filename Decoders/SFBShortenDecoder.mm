//
// Copyright (c) 2020 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import <algorithm>
#import <vector>

#import "SFBShortenDecoder.h"

#import "SFBByteStream.hpp"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "NSError+SFBURLPresentation.h"

SFBAudioDecoderName const SFBAudioDecoderNameShorten = @"org.sbooth.AudioEngine.Decoder.Shorten";

#define MIN_SUPPORTED_VERSION 1
#define MAX_SUPPORTED_VERSION 3

#define DEFAULT_BLOCK_SIZE  256
#define DEFAULT_V0NMEAN     0
#define DEFAULT_V2NMEAN     4
#define DEFAULT_MAXNLPC     0

#define CHANSIZE      0
#define ENERGYSIZE    3
#define BITSHIFTSIZE  2
#define NWRAP         3

#define FNSIZE       2
#define FN_DIFF0     0
#define FN_DIFF1     1
#define FN_DIFF2     2
#define FN_DIFF3     3
#define FN_QUIT      4
#define FN_BLOCKSIZE 5
#define FN_BITSHIFT  6
#define FN_QLPC      7
#define FN_ZERO      8
#define FN_VERBATIM  9

#define VERBATIM_CKSIZE_SIZE 5   /* a var_put code size */
#define VERBATIM_BYTE_SIZE   8   /* code size 8 on single bytes means no compression at all */
#define VERBATIM_CHUNK_MAX   256 /* max. size of a FN_VERBATIM chunk */

#define ULONGSIZE 2
#define NSKIPSIZE 1
#define LPCQSIZE  2
#define LPCQUANT  5
#define XBYTESIZE 7

#define TYPESIZE            4
#define TYPE_S8             1  /* signed 8 bit characters                   */
#define TYPE_U8             2  /* unsigned 8 bit characters                 */
#define TYPE_S16HL          3  /* signed 16 bit shorts: high-low            */
#define TYPE_U16HL          4  /* unsigned 16 bit shorts: high-low          */
#define TYPE_S16LH          5  /* signed 16 bit shorts: low-high            */
#define TYPE_U16LH          6  /* unsigned 16 bit shorts: low-high          */

#define SEEK_TABLE_REVISION 1

#define SEEK_HEADER_SIZE  12
#define SEEK_TRAILER_SIZE 12
#define SEEK_ENTRY_SIZE   80

#define V2LPCQOFFSET (1 << LPCQUANT)

#define MAX_CHANNELS 8
#define MAX_BLOCKSIZE 65535

#define CANONICAL_HEADER_SIZE 44

#define WAVE_FORMAT_PCM 0x0001

#define ROUNDEDSHIFTDOWN(x, n) (((n) == 0) ? (x) : ((x) >> ((n) - 1)) >> 1)

namespace {

/// Returns a two-dimensional \c rows x \c cols array using one allocation from \c malloc
template <typename T>
T ** AllocateContiguous2DArray(size_t rows, size_t cols)
{
	T **result = static_cast<T **>(std::malloc((rows * sizeof(T *)) + (rows * cols * sizeof(T))));
	T *tmp = reinterpret_cast<T *>(result + rows);
	for(size_t i = 0; i < rows; ++i)
		result[i] = tmp + i * cols;
	return result;
}

/// Clips values to the interval [lower, upper]
template <typename T>
constexpr const T& clip(const T& n, const T& lower, const T& upper) {
	return std::max(lower, std::min(n, upper));
}

///// Returns @c v clamped to the interval @c [lo,hi]
//template<typename T>
//constexpr const T& clamp(const T& v, const T& lo, const T& hi)
//{
//	assert(!(hi < lo));
//	return (v < lo) ? lo : (hi < v) ? hi : v;
//}

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

	/// Creates a new \c VariableLengthInput object with an internal buffer of the specified size
	/// @warning Sizes other than \c 512 will break seeking
	VariableLengthInput(size_t size = 512)
	: mInputBlock(nil), mSize(size), mBytesAvailable(0), mBitBuffer(0), mBitsAvailable(0)
	{
		mByteBuffer = new uint8_t [mSize];
		mByteBufferPosition = mByteBuffer;
	}

	~VariableLengthInput()
	{
		delete [] mByteBuffer;
	}

	/// Input callback type
	using InputBlock = bool(^)(void *buf, size_t len, size_t& read);

	/// Sets the input callback
	void SetInputCallback(InputBlock block)
	{
		mInputBlock = block;
	}

	/// Reads a single unsigned value from the specified bin
	bool uvar_get(int32_t& i32, size_t bin)
	{
		if(mBitsAvailable == 0) {
			if(!word_get(mBitBuffer))
				return false;
			mBitsAvailable = 32;
		}

		int32_t result;
		for(result = 0; !(mBitBuffer & (1L << --mBitsAvailable)); ++result) {
			if(mBitsAvailable == 0) {
				if(!word_get(mBitBuffer))
					return false;
				mBitsAvailable = 32;
			}
		}

		while(bin != 0) {
			if(mBitsAvailable >= bin) {
				result = (result << bin) | static_cast<int32_t>((mBitBuffer >> (mBitsAvailable - bin)) & sMaskTable[bin]);
				mBitsAvailable -= bin;
				bin = 0;
			}
			else {
				result = (result << mBitsAvailable) | static_cast<int32_t>(mBitBuffer & sMaskTable[mBitsAvailable]);
				bin -= mBitsAvailable;
				if(!word_get(mBitBuffer))
					return false;
				mBitsAvailable = 32;
			}
		}

		i32 = result;
		return true;
	}

	/// Reads a single signed value from the specified bin
	bool var_get(int32_t& i32, size_t bin)
	{
		int32_t var;
		if(!uvar_get(var, bin + 1))
			return false;

		uint32_t uvar = static_cast<uint32_t>(var);
		if(uvar & 1)
			i32 = ~(uvar >> 1);
		else
			i32 = (uvar >> 1);
		return true;
	}

	/// Reads the unsigned Golomb-Rice code
	bool ulong_get(uint32_t& ui32)
	{
		int32_t bitcount;
		if(!uvar_get(bitcount, ULONGSIZE))
			return false;

		int32_t i32;
		if(!uvar_get(i32, static_cast<uint32_t>(bitcount)))
			return false;

		ui32 = static_cast<uint32_t>(i32);
		return true;
	}

	bool uint_get(uint32_t& ui32, int version, size_t bin)
	{
		if(version == 0) {
			int32_t i32;
			if(!uvar_get(i32, bin))
				return false;
			ui32 = static_cast<uint32_t>(i32);
			return true;
		}
		else
			return ulong_get(ui32);
	}

	static size_t sizeof_uvar(uint32_t val, size_t nbin)
	{
		return (val >> nbin) + nbin;
	}

	static size_t sizeof_var(int32_t val, size_t nbin)
	{
		return static_cast<size_t>(labs(val) >> nbin) + nbin + 1;
	}

	void Reset()
	{
		mByteBufferPosition = mByteBuffer;
		mBytesAvailable = 0;
		mBitsAvailable = 0;
	}

	bool Refill()
	{
		size_t bytesRead = 0;
		if(!mInputBlock || !mInputBlock(mByteBuffer, mSize, bytesRead) || bytesRead < 4)
			return false;
		mBytesAvailable += bytesRead;
		mByteBufferPosition = mByteBuffer;
		return true;
	}

	bool SetState(uint16_t byteBufferPosition, uint16_t bytesAvailable, uint32_t bitBuffer, uint16_t bitsAvailable)
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
	InputBlock mInputBlock;
	/// Size of \c mByteBuffer in bytes
	size_t mSize;
	/// Byte buffer
	uint8_t *mByteBuffer;
	/// Current position in \c mByteBuffer
	uint8_t *mByteBufferPosition;
	/// Bytes available in \c mByteBuffer
	size_t mBytesAvailable;
	/// Bit buffer
	uint32_t mBitBuffer;
	/// Bits available in \c mBuffer
	size_t mBitsAvailable;

	/// Reads a single \c uint32_t from the byte buffer, refilling if necessary
	bool word_get(uint32_t& ui32)
	{
		if(mBytesAvailable < 4 && !Refill())
			return false;

		ui32 = static_cast<uint32_t>((static_cast<int32_t>(mByteBufferPosition[0]) << 24) | (static_cast<int32_t>(mByteBufferPosition[1]) << 16) | (static_cast<int32_t>(mByteBufferPosition[2]) << 8) | static_cast<int32_t>(mByteBufferPosition[3]));

		mByteBufferPosition += 4;
		mBytesAvailable -= 4;

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
	SFB::ByteStream byteStream(buf, SEEK_HEADER_SIZE);

	SeekTableHeader header;
	byteStream.Read(header.mSignature, 4);
	header.mVersion = byteStream.ReadLE<uint32_t>();
	header.mFileSize = byteStream.ReadLE<uint32_t>();

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
	SFB::ByteStream byteStream(buf, SEEK_TRAILER_SIZE);

	SeekTableTrailer trailer;
	trailer.mSeekTableSize = byteStream.ReadLE<uint32_t>();
	byteStream.Read(trailer.mSignature, 8);

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
	SFB::ByteStream byteStream(buf, SEEK_ENTRY_SIZE);

	SeekTableEntry entry;
	entry.mFrameNumber = byteStream.ReadLE<uint32_t>();
	entry.mByteOffsetInFile = byteStream.ReadLE<uint32_t>();
	entry.mLastBufferReadPosition = byteStream.ReadLE<uint32_t>();
	entry.mBytesAvailable = byteStream.ReadLE<uint16_t>();
	entry.mByteBufferPosition = byteStream.ReadLE<uint16_t>();
	entry.mBitBufferPosition = byteStream.ReadLE<uint16_t>();
	entry.mBitBuffer = byteStream.ReadLE<uint32_t>();
	entry.mBitshift = byteStream.ReadLE<uint16_t>();
	for(auto i = 0; i < 3; ++i)
		entry.mCBuf0[i] = static_cast<int32_t>(byteStream.ReadLE<uint32_t>());
	for(auto i = 0; i < 3; ++i)
		entry.mCBuf1[i] = static_cast<int32_t>(byteStream.ReadLE<uint32_t>());
	for(auto i = 0; i < 4; ++i)
		entry.mOffset0[i] = static_cast<int32_t>(byteStream.ReadLE<uint32_t>());
	for(auto i = 0; i < 4; ++i)
		entry.mOffset1[i] = static_cast<int32_t>(byteStream.ReadLE<uint32_t>());

	return entry;
}

/// Locates the most suitable seek table entry for \c frame
std::vector<SeekTableEntry>::const_iterator FindSeekTableEntry(std::vector<SeekTableEntry>::const_iterator begin, std::vector<SeekTableEntry>::const_iterator end, AVAudioFramePosition frame)
{
	auto it = std::upper_bound(begin, end, frame, [](AVAudioFramePosition value, const SeekTableEntry& entry) {
		return value < entry.mFrameNumber;
	});
	return it == begin ? end : --it;
}

}

@interface SFBShortenDecoder ()
{
@private
	VariableLengthInput _input;
	int _version;
	int32_t _lpcqoffset;
	int _internal_ftype;
	int _nchan;
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
- (BOOL)parseRIFFChunk:(SFB::ByteStream&)chunkData error:(NSError **)error;
- (BOOL)parseFORMChunk:(SFB::ByteStream&)chunkData error:(NSError **)error;
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

	if((_bitsPerSample == 8 && (_internal_ftype != TYPE_U8 && _internal_ftype != TYPE_S8)) || (_bitsPerSample == 16 && (_internal_ftype != TYPE_U16HL && _internal_ftype != TYPE_U16LH && _internal_ftype != TYPE_S16HL && _internal_ftype != TYPE_S16LH))) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth/audio type combination: %u, %u", _bitsPerSample, _internal_ftype);
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
	// Apparently *16HL isn't true for 'AIFF'
//	if(_internal_ftype == TYPE_U16HL || _internal_ftype == TYPE_S16HL)
	if(_bigEndian)
		processingStreamDescription.mFormatFlags	|= kAudioFormatFlagIsBigEndian;
	if(_internal_ftype == TYPE_S8 || _internal_ftype == TYPE_S16HL || _internal_ftype == TYPE_S16LH)
		processingStreamDescription.mFormatFlags	|= kAudioFormatFlagIsSignedInteger;

	processingStreamDescription.mSampleRate			= _sampleRate;
	processingStreamDescription.mChannelsPerFrame	= static_cast<UInt32>(_nchan);
	processingStreamDescription.mBitsPerChannel		= _bitsPerSample;

	processingStreamDescription.mBytesPerPacket		= (_bitsPerSample + 7) / 8;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

	AVAudioChannelLayout *channelLayout = nil;
	switch(_nchan) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 3:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_3_0_A];		break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
		case 5:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_0_A];		break;
		case 6:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_1_A];		break;
		case 7:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_6_1_A];		break;
		case 8:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_7_1_A];		break;
	}

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription{};

	sourceStreamDescription.mFormatID			= kSFBAudioFormatShorten;

	sourceStreamDescription.mSampleRate			= _sampleRate;
	sourceStreamDescription.mChannelsPerFrame	= static_cast<UInt32>(_nchan);
	sourceStreamDescription.mBitsPerChannel		= _bitsPerSample;

	sourceStreamDescription.mFramesPerPacket	= static_cast<UInt32>(_blocksize);

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	_frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:(AVAudioFrameCount)_blocksize];

	// Allocate decoding buffers
	_buffer = AllocateContiguous2DArray<int32_t>(static_cast<size_t>(_nchan), static_cast<size_t>(_blocksize + _nwrap));
	_offset = AllocateContiguous2DArray<int32_t>(static_cast<size_t>(_nchan), static_cast<size_t>(std::max(1, _nmean)));

	for(auto i = 0; i < _nchan; ++i) {
		for(auto j = 0; j < _nwrap; ++j) {
			_buffer[i][j] = 0;
		}
		_buffer[i] += _nwrap;
	}

	if(_maxnlpc > 0)
		_qlpc = new int [static_cast<size_t>(_maxnlpc)];

	// Initialize offset
	int32_t mean = 0;
	switch(_internal_ftype) {
//		case TYPE_AU1:
		case TYPE_S8:
		case TYPE_S16HL:
		case TYPE_S16LH:
//		case TYPE_ULAW:
//		case TYPE_AU2:
//		case TYPE_AU3:
//		case TYPE_ALAW:
			mean = 0;
			break;
		case TYPE_U8:
			mean = 0x80;
			break;
		case TYPE_U16HL:
		case TYPE_U16LH:
			mean = 0x8000;
			break;
		default:
			os_log_error(gSFBAudioDecoderLog, "Unsupported audio type: %u", _internal_ftype);
			return NO;
	}

	for(auto chan = 0; chan < _nchan; ++chan) {
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
	if(_nchan == 2) {
		_buffer[1][-1] = entry->mCBuf1[0];
		_buffer[1][-2] = entry->mCBuf1[1];
		_buffer[1][-3] = entry->mCBuf1[2];
	}

	for(auto i = 0; i < std::max(1, _nmean); ++i) {
		_offset[0][i] = entry->mOffset0[i];
		if(_nchan == 2)
			_offset[1][i] = entry->mOffset1[i];
	}

	_bitshift = entry->mBitshift;

	_framePosition = entry->mFrameNumber;
	_frameBuffer.frameLength = 0;

	AVAudioFrameCount framesToSkip = (AVAudioFrameCount)(frame - entry->mFrameNumber);
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
	if(![_inputSource readUInt8:&version error:nil] || version < MIN_SUPPORTED_VERSION || version > MAX_SUPPORTED_VERSION) {
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
	_nmean = _version < 2 ? DEFAULT_V0NMEAN : DEFAULT_V2NMEAN;

	// Set up variable length reading callback
	__weak SFBInputSource *inputSource = self->_inputSource;
	_input.SetInputCallback(^bool(void *buf, size_t len, size_t &read) {
		NSInteger bytesRead;
		if(![inputSource readBytes:buf length:(NSInteger)len bytesRead:&bytesRead error:nil])
			return false;
		read = static_cast<size_t>(bytesRead);
		return true;
	});

	// Read internal file type
	uint32_t ftype;
	if(!_input.uint_get(ftype, _version, TYPESIZE)) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}
	if(ftype != TYPE_U8 && ftype != TYPE_S8 && ftype != TYPE_U16HL && ftype != TYPE_U16LH && ftype != TYPE_S16HL && ftype != TYPE_S16LH) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported audio type: %u", ftype);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Invalid or unsupported audio type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported audio type.", @"")];
		return NO;
	}
	_internal_ftype = static_cast<int>(ftype);

	// Read number of channels
	uint32_t nchan = 0;
	if(!_input.uint_get(nchan, _version, CHANSIZE) || nchan == 0 || nchan > MAX_CHANNELS) {
		os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported channel count: %u", nchan);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Invalid or unsupported number of channels", @"")
							   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported number of channels.", @"")];
		return NO;
	}
	_nchan = static_cast<int>(nchan);

	// Read blocksize if version > 0
	if(_version > 0) {
		uint32_t blocksize = 0;
		if(!_input.uint_get(blocksize, _version, static_cast<size_t>(log2(DEFAULT_BLOCK_SIZE))) || blocksize == 0 || blocksize > MAX_BLOCKSIZE) {
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
		if(!_input.uint_get(maxnlpc, _version, LPCQSIZE) || maxnlpc > 1024) {
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
		if(!_input.uint_get(nmean, _version, 0) || nmean > 32768) {
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

		uint32_t nskip;
		if(!_input.uint_get(nskip, _version, NSKIPSIZE) /* || nskip > bits_remaining_in_input */) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		for(uint32_t i = 0; i < nskip; ++i) {
			uint32_t dummy;
			if(!_input.uint_get(dummy, _version, XBYTESIZE)) {
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
		_blocksize = DEFAULT_BLOCK_SIZE;
		_maxnlpc = DEFAULT_MAXNLPC;
	}

	_nwrap = std::max(NWRAP, static_cast<int>(_maxnlpc));

	if(_version > 1)
		_lpcqoffset = V2LPCQOFFSET;

	// Parse the WAVE or AIFF header in the verbatim section

	int32_t fn;
	if(!_input.uvar_get(fn, FNSIZE) || fn != FN_VERBATIM) {
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

	int32_t header_size;
	if(!_input.uvar_get(header_size, VERBATIM_CKSIZE_SIZE) || header_size < CANONICAL_HEADER_SIZE || header_size > VERBATIM_CHUNK_MAX) {
		os_log_error(gSFBAudioDecoderLog, "Incorrect header size: %u", header_size);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	int8_t header_bytes [header_size];
	for(int32_t i = 0; i < header_size; ++i) {
		int32_t byte;
		if(!_input.uvar_get(byte, VERBATIM_BYTE_SIZE)) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		header_bytes[i] = static_cast<int8_t>(byte);
	}

	SFB::ByteStream chunkData{header_bytes, static_cast<size_t>(header_size)};
	auto chunkID = chunkData.ReadBE<uint32_t>();

	// Skip chunk size
	chunkData.Skip(4);

	// WAVE
	if(chunkID == 'RIFF') {
		if(![self parseRIFFChunk:chunkData error:error])
			return NO;
	}
	// AIFF
	else if(chunkID == 'FORM') {
		if(![self parseFORMChunk:chunkData error:error])
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

- (BOOL)parseRIFFChunk:(SFB::ByteStream&)chunkData error:(NSError **)error
{
	auto chunkID = chunkData.ReadBE<uint32_t>();
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

	bool sawFormatChunk = false;
	uint32_t dataChunkSize = 0;
	uint16_t blockAlign = 0;

	while((chunkID = chunkData.ReadBE<uint32_t>())) {
		auto len = chunkData.ReadLE<uint32_t>();
		switch(chunkID) {
			case 'fmt ':
			{
				if(len < 16) {
					os_log_error(gSFBAudioDecoderLog, "'fmt ' chunk is too small (%u bytes)", len);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
										   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
					return NO;
				}

				auto format_tag = chunkData.ReadLE<uint16_t>();
				if(format_tag != WAVE_FORMAT_PCM) {
					os_log_error(gSFBAudioDecoderLog, "Unsupported WAVE format tag: %x", format_tag);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInvalidFormat
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Unsupported WAVE format tag", @"")
										   recoverySuggestion:NSLocalizedString(@"The file's WAVE format tag is not supported.", @"")];
					return NO;
				}

				auto channels = chunkData.ReadLE<uint16_t>();
				if(_nchan != channels)
					os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'fmt ' chunk (%u)", _nchan, channels);
				_sampleRate = chunkData.ReadLE<uint32_t>();
				chunkData.Skip(4); // average bytes per second
				blockAlign = chunkData.ReadLE<uint16_t>();
				_bitsPerSample = chunkData.ReadLE<uint16_t>();

				if(len > 16)
					os_log_info(gSFBAudioDecoderLog, "%u bytes in 'fmt ' chunk not parsed", len - 16);

				sawFormatChunk = true;

				break;
			}

			case 'data':
				dataChunkSize = len;
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

- (BOOL)parseFORMChunk:(SFB::ByteStream&)chunkData error:(NSError **)error
{
	auto chunkID = chunkData.ReadBE<uint32_t>();
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

	// Skip unknown chunks, looking for 'COMM'
	while(chunkData.ReadBE<uint32_t>() != 'COMM') {
		auto len = chunkData.ReadBE<uint32_t>();
		// pad byte not included in ckLen
		if(static_cast<int32_t>(len) < 0 || chunkData.Remaining() < 18 + len + (len & 1)) {
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
		chunkData.Skip(len + (len & 1));
	}

	auto len = chunkData.ReadBE<uint32_t>();
	if(static_cast<int32_t>(len) < 18) {
		os_log_error(gSFBAudioDecoderLog, "'COMM' chunk is too small (%u bytes)", len);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	auto channels = chunkData.ReadBE<uint16_t>();
	if(_nchan != channels)
		os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'COMM' chunk (%u)", _nchan, channels);

	_frameLength = chunkData.ReadBE<uint32_t>();

	_bitsPerSample = chunkData.ReadBE<uint16_t>();

	// sample rate is IEEE 754 80-bit extended float (16-bit exponent, 1-bit integer part, 63-bit fraction)
	auto exp = static_cast<int16_t>(chunkData.ReadBE<uint16_t>()) - 16383 - 63;
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

	auto frac = chunkData.ReadBE<uint64_t>();
	if(exp >= 0)
		_sampleRate = static_cast<uint32_t>(frac << exp);
	else
		_sampleRate = static_cast<uint32_t>((frac + (static_cast<uint64_t>(1) << (-exp - 1))) >> -exp);

	if(len > 18)
		os_log_info(gSFBAudioDecoderLog, "%u bytes in 'COMM' chunk not parsed", len - 16);

	return YES;
}

- (BOOL)decodeBlockReturningError:(NSError **)error
{
	int chan = 0;
	for(;;) {
		int32_t cmd;
		if(!_input.uvar_get(cmd, FNSIZE)) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		if(cmd == FN_QUIT) {
			_eos = true;
			return YES;
		}

		switch(cmd) {
			case FN_ZERO:
			case FN_DIFF0:
			case FN_DIFF1:
			case FN_DIFF2:
			case FN_DIFF3:
			case FN_QLPC:
			{
				int32_t coffset, *cbuffer = _buffer[chan];
				int resn = 0, nlpc;

				if(cmd != FN_ZERO) {
					if(!_input.uvar_get(resn, ENERGYSIZE)) {
						if(error)
							*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
															 code:SFBAudioDecoderErrorCodeInvalidFormat
									descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
															  url:_inputSource.url
													failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
											   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
						return NO;
					}
					/* this is a hack as version 0 differed in definition of var_get */
					if(_version == 0)
						resn--;
				}

				/* find mean offset : N.B. this code duplicated */
				if(_nmean == 0)
					coffset = _offset[chan][0];
				else
				{
					int32_t sum = (_version < 2) ? 0 : _nmean / 2;
					for(auto i = 0; i < _nmean; i++) {
						sum += _offset[chan][i];
					}
					if(_version < 2)
						coffset = sum / _nmean;
					else
						coffset = ROUNDEDSHIFTDOWN(sum / _nmean, _bitshift);
				}

				switch(cmd)
				{
					case FN_ZERO:
						for(auto i = 0; i < _blocksize; ++i) {
							cbuffer[i] = 0;
						}
						break;
					case FN_DIFF0:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.var_get(var, static_cast<size_t>(resn))) {
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
					case FN_DIFF1:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.var_get(var, static_cast<size_t>(resn))) {
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
					case FN_DIFF2:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.var_get(var, static_cast<size_t>(resn))) {
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
					case FN_DIFF3:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.var_get(var, static_cast<size_t>(resn))) {
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
					case FN_QLPC:
						if(!_input.uvar_get(nlpc, LPCQSIZE)) {
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
							if(!_input.var_get(_qlpc[i], LPCQUANT)) {
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
							int32_t sum = _lpcqoffset;

							for(auto j = 0; j < nlpc; ++j) {
								sum += _qlpc[j] * cbuffer[i - j - 1];
							}
							int32_t var;
							if(!_input.var_get(var, static_cast<size_t>(resn))) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInvalidFormat
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + (sum >> LPCQUANT);
						}
						if(coffset != 0) {
							for(auto i = 0; i < _blocksize; ++i) {
								cbuffer[i] += coffset;
							}
						}
						break;
				}

				/* store mean value if appropriate : N.B. Duplicated code */
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

				/* do the wrap */
				for(auto i = -_nwrap; i < 0; i++) {
					cbuffer[i] = cbuffer[i + _blocksize];
				}

				if(_bitshift != 0) {
					for(auto i = 0; i < _blocksize; ++i) {
						cbuffer[i] <<= _bitshift;
					}
				}

				if(chan == _nchan - 1) {
					switch(_internal_ftype) {
						case TYPE_U8:
						{
							auto abl = _frameBuffer.audioBufferList;
							for(auto channel = 0; channel < _nchan; ++channel) {
								auto channel_buf = static_cast<uint8_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample)
									channel_buf[sample] = static_cast<uint8_t>(clip(_buffer[channel][sample], 0, UINT8_MAX));
							}
							_frameBuffer.frameLength = (AVAudioFrameCount)_blocksize;
							break;
						}
						case TYPE_S8:
						{
							auto abl = _frameBuffer.audioBufferList;
							for(auto channel = 0; channel < _nchan; ++channel) {
								auto channel_buf = static_cast<int8_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample)
									channel_buf[sample] = static_cast<int8_t>(clip(_buffer[channel][sample], INT8_MIN, INT8_MAX));
							}
							_frameBuffer.frameLength = (AVAudioFrameCount)_blocksize;
							break;
						}
						case TYPE_U16HL:
						case TYPE_U16LH:
						{
							auto abl = _frameBuffer.audioBufferList;
							for(auto channel = 0; channel < _nchan; ++channel) {
								auto channel_buf = static_cast<uint16_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample)
									channel_buf[sample] = static_cast<uint16_t>(clip(_buffer[channel][sample], 0, UINT16_MAX));
							}
							_frameBuffer.frameLength = (AVAudioFrameCount)_blocksize;
							break;
						}
						case TYPE_S16HL:
						case TYPE_S16LH:
						{
							auto abl = _frameBuffer.audioBufferList;
							for(auto channel = 0; channel < _nchan; ++channel) {
								auto channel_buf = static_cast<int16_t *>(abl->mBuffers[channel].mData);
								for(auto sample = 0; sample < _blocksize; ++sample) {
									channel_buf[sample] = static_cast<int16_t>(clip(_buffer[channel][sample], INT16_MIN, INT16_MAX));
								}
							}
							_frameBuffer.frameLength = (AVAudioFrameCount)_blocksize;
							break;
						}
					}

					++_blocksDecoded;
					return YES;
				}
				chan = (chan + 1) % _nchan;
				break;
			}

			case FN_BLOCKSIZE:
			{
				uint32_t uint = 0;
				if(!_input.uint_get(uint, _version, static_cast<size_t>(log2(_blocksize))) || uint == 0 || uint > MAX_BLOCKSIZE || static_cast<int>(uint) > _blocksize) {
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
			case FN_BITSHIFT:
				if(!_input.uvar_get(_bitshift, BITSHIFTSIZE) || _bitshift > 32) {
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
			case FN_VERBATIM:
			{
				int32_t chunk_len;
				if(!_input.uvar_get(chunk_len, VERBATIM_CKSIZE_SIZE) || chunk_len < 0 || chunk_len > VERBATIM_CHUNK_MAX) {
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
					if(!_input.uvar_get(dummy, VERBATIM_BYTE_SIZE)) {
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
	if(![_inputSource getLength:&fileLength error:error] || ![_inputSource seekToOffset:(fileLength - SEEK_TRAILER_SIZE) error:error])
		return NO;

	SeekTableTrailer trailer;
	{
		uint8_t buf [SEEK_TRAILER_SIZE];
		NSInteger bytesRead;
		if(![_inputSource readBytes:buf length:SEEK_TRAILER_SIZE bytesRead:&bytesRead error:error] || bytesRead != SEEK_TRAILER_SIZE)
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
		uint8_t buf [SEEK_HEADER_SIZE];
		NSInteger bytesRead;
		if(![_inputSource readBytes:buf length:SEEK_HEADER_SIZE bytesRead:&bytesRead error:error] || bytesRead != SEEK_HEADER_SIZE)
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

	auto count = (trailer.mSeekTableSize - SEEK_TRAILER_SIZE - SEEK_HEADER_SIZE) / SEEK_ENTRY_SIZE;
	for(uint32_t i = 0; i < count; ++i) {
		uint8_t buf [SEEK_ENTRY_SIZE];
		NSInteger bytesRead;
		if(![_inputSource readBytes:buf length:SEEK_ENTRY_SIZE bytesRead:&bytesRead error:error] || bytesRead != SEEK_ENTRY_SIZE)
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
		uint8_t buf [SEEK_HEADER_SIZE];
		NSInteger bytesRead;
		if(![inputSource readBytes:buf length:SEEK_HEADER_SIZE bytesRead:&bytesRead error:&error] || bytesRead != SEEK_HEADER_SIZE) {
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
		uint8_t buf [SEEK_ENTRY_SIZE];
		NSInteger bytesRead;
		if(![inputSource readBytes:buf length:SEEK_ENTRY_SIZE bytesRead:&bytesRead error:&error] || bytesRead != SEEK_ENTRY_SIZE) {
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
	else if(_nchan != 1 && _nchan != 2) {
		os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid channel count (%d); mono or stereo required", _nchan);
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
