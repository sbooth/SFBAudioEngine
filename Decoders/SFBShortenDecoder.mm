/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <algorithm>

#import "SFBShortenDecoder.h"

#import "AVAudioChannelLayout+SFBChannelLabels.h"
#import "NSError+SFBURLPresentation.h"


//#define MAGIC                 "ajkg"
//#define FORMAT_VERSION        2
#define MIN_SUPPORTED_VERSION 1
#define MAX_SUPPORTED_VERSION 3
//#define MAX_VERSION           7

//#define UNDEFINED_UINT     -1
#define DEFAULT_BLOCK_SIZE  256
#define DEFAULT_V0NMEAN     0
#define DEFAULT_V2NMEAN     4
#define DEFAULT_MAXNLPC     0
#define DEFAULT_NCHAN       1
#define DEFAULT_NSKIP       0
#define DEFAULT_NDISCARD    0
#define NBITPERLONG         32
#define DEFAULT_MINSNR      256
#define DEFAULT_MAXRESNSTR  "32.0"
#define DEFAULT_QUANTERROR  0
#define MINBITRATE          2.5

#define MAX_LPC_ORDER 64
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
#define TYPE_AU1            0  /* original lossless ulaw                    */
#define TYPE_S8             1  /* signed 8 bit characters                   */
#define TYPE_U8             2  /* unsigned 8 bit characters                 */
#define TYPE_S16HL          3  /* signed 16 bit shorts: high-low            */
#define TYPE_U16HL          4  /* unsigned 16 bit shorts: high-low          */
#define TYPE_S16LH          5  /* signed 16 bit shorts: low-high            */
#define TYPE_U16LH          6  /* unsigned 16 bit shorts: low-high          */
#define TYPE_ULAW           7  /* lossy ulaw: internal conversion to linear */
#define TYPE_AU2            8  /* new ulaw with zero mapping                */
#define TYPE_AU3            9  /* lossless alaw                             */
#define TYPE_ALAW          10  /* lossy alaw: internal conversion to linear */
#define TYPE_RIFF_WAVE     11  /* Microsoft .WAV files                      */
#define TYPE_AIFF          12  /* Apple .AIFF files                         */
#define TYPE_EOF           13
#define TYPE_GENERIC_ULAW 128
#define TYPE_GENERIC_ALAW 129

#define POSITIVE_ULAW_ZERO 0xff
#define NEGATIVE_ULAW_ZERO 0x7f

#define SEEK_TABLE_REVISION 1

#define SEEK_HEADER_SIZE  12
#define SEEK_TRAILER_SIZE 12
#define SEEK_ENTRY_SIZE   80

//#define V2LPCQOFFSET (1 << LPCQUANT);

#define MAX_CHANNELS 8
#define MAX_BLOCKSIZE 65535

#define CANONICAL_HEADER_SIZE 44

#define WAVE_FORMAT_PCM 0x0001

namespace {
	static const uint32_t sMaskTable [] = {
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

	/// Variable-length input using Golomb-Rice coding
	class VariableLengthInput {
	public:
		/// Creates a new \c VariableLengthInput object with an internal buffer of the specified size
		VariableLengthInput(size_t size = 512)
			: mInputBlock(nil), mSize(size), mBytesAvailable(0), mBuffer(0), mBitsAvailable(0)
		{
			mBuf = new uint8_t [(size_t)mSize];
			mPos = mBuf;
		}

		~VariableLengthInput()
		{
			delete [] mBuf;
		}

		/// Input callback type
		using InputBlock = bool(^)(void *buf, size_t len, size_t& read);

		/// Sets the input callback
		void SetInputCallback(InputBlock block)
		{
			mInputBlock = block;
		}

		/// Reads a single \c int32_t from the specified bin
		bool uvar_get(int32_t& i32, size_t bin)
		{
			if(mBitsAvailable == 0) {
				if(!word_get(mBuffer))
					return false;
				mBitsAvailable = 32;
			}

			int32_t result;
			for(result = 0; !(mBuffer & (1L << --mBitsAvailable)); ++result) {
				if(mBitsAvailable == 0) {
					if(!word_get(mBuffer))
						return false;
					mBitsAvailable = 32;
				}
			}

			while(bin != 0) {
				if(mBitsAvailable >= bin) {
					result = (result << bin) | (int32_t)((mBuffer >> (mBitsAvailable - bin)) & sMaskTable[bin]);
					mBitsAvailable -= bin;
					bin = 0;
				}
				else {
					result = (result << mBitsAvailable) | (int32_t)(mBuffer & sMaskTable[mBitsAvailable]);
					bin -= mBitsAvailable;
					if(!word_get(mBuffer))
						return false;
					mBitsAvailable = 32;
				}
			}

			i32 = result;
			return true;
		}

		/// Reads the unsigned Golomb-Rice code
		bool ulong_get(uint32_t& ui32)
		{
			int32_t bitcount;
			if(!uvar_get(bitcount, ULONGSIZE))
				return false;

			int32_t i32;
			if(!uvar_get(i32, (uint32_t)bitcount))
				return false;

			ui32 = (uint32_t)i32;
			return true;
		}

	private:
		/// Input callback
		InputBlock mInputBlock;
		/// Size of \c mBuf in bytes
		size_t mSize;
		/// Byte buffer
		uint8_t *mBuf;
		/// Current position in mBuf
		uint8_t *mPos;
		/// Bytes available in \c mPos
		size_t mBytesAvailable;
		/// Bit buffer
		uint32_t mBuffer;
		/// Bits available in \c mBuffer
		size_t mBitsAvailable;

		/// Reads a single \c uint32_t from the byte buffer, refilling if necessary
		bool word_get(uint32_t& ui32)
		{
			if(mBytesAvailable < 4) {
				size_t bytesRead = 0;
				if(!mInputBlock || !mInputBlock(mBuf, mSize, bytesRead) || bytesRead < 4)
					return false;
				mBytesAvailable += bytesRead;
				mPos = mBuf;
			}

			ui32 = (uint32_t)((((int32_t)mPos[0]) << 24) | (((int32_t)mPos[1]) << 16) | (((int32_t)mPos[2]) << 8) | ((int32_t)mPos[3]));

			mPos += 4;
			mBytesAvailable -= 4;

			return true;
		}

	};

	class ByteStream {
	public:
		ByteStream(const void *buf, size_t len)
			: mBuf(buf), mLen(len), mPos(0)
		{}

		bool ReadLE16(uint16_t& ui16)
		{
			if((mPos + 2) > mLen)
				return false;
			ui16 = OSReadLittleInt16(mBuf, mPos);
			mPos += 2;
			return true;
		}

		uint16_t ReadLE16()
		{
			uint16_t result = 0;
			if(!ReadLE16(result))
				mPos = mLen;
			return result;
		}

		bool ReadLE32(uint32_t& ui32)
		{
			if((mPos + 4) > mLen)
				return false;
			ui32 = OSReadLittleInt32(mBuf, mPos);
			mPos += 4;
			return true;
		}

		uint32_t ReadLE32()
		{
			uint32_t result = 0;
			if(!ReadLE32(result))
				mPos = mLen;
			return result;
		}

		bool ReadLE64(uint64_t& ui64)
		{
			if((mPos + 8) > mLen)
				return false;
			ui64 = OSReadLittleInt64(mBuf, mPos);
			mPos += 8;
			return true;
		}

		uint64_t ReadLE64()
		{
			uint64_t result = 0;
			if(!ReadLE64(result))
				mPos = mLen;
			return result;
		}

		bool ReadBE16(uint16_t& ui16)
		{
			if((mPos + 2) > mLen)
				return false;
			ui16 = OSReadBigInt16(mBuf, mPos);
			mPos += 2;
			return true;
		}

		uint16_t ReadBE16()
		{
			uint16_t result = 0;
			if(!ReadBE16(result))
				mPos = mLen;
			return result;
		}

		bool ReadBE32(uint32_t& ui32)
		{
			if((mPos + 4) > mLen)
				return false;
			ui32 = OSReadBigInt32(mBuf, mPos);
			mPos += 4;
			return true;
		}

		uint32_t ReadBE32()
		{
			uint32_t result = 0;
			if(!ReadBE32(result))
				mPos = mLen;
			return result;
		}

		bool ReadBE64(uint64_t& ui64)
		{
			if((mPos + 8) > mLen)
				return false;
			ui64 = OSReadBigInt64(mBuf, mPos);
			mPos += 8;
			return true;
		}

		uint64_t ReadBE64()
		{
			uint64_t result = 0;
			if(!ReadBE64(result))
				mPos = mLen;
			return result;
		}

		bool Skip(size_t count)
		{
			if((mPos + count) > mLen)
				return false;
			mPos += count;
			return true;
		}

		inline size_t Remaining() const
		{
			return mLen - mPos;
		}

	private:
		const void *mBuf;
		size_t mLen;
		size_t mPos;
	};
}

@interface SFBShortenDecoder ()
{
@private
	VariableLengthInput _input;
	int _version;
	int _internal_ftype;
	int _nchan;
	int _nmean;
	int _blocksize;
	int _nwrap;

//	int32_t _lpcqoffset;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
}
- (BOOL)parseShortenHeaderReturningError:(NSError **)error;
- (BOOL)parseRIFFChunk:(ByteStream&)chunkData error:(NSError **)error;
- (BOOL)parseFORMChunk:(ByteStream&)chunkData error:(NSError **)error;
- (BOOL)uint_get:(nonnull uint32_t *)result size:(size_t)size;
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

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error] || ![self parseShortenHeaderReturningError:error])
		return NO;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return NO;
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

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	return YES;
}

- (BOOL)parseShortenHeaderReturningError:(NSError **)error
{
	// Read magic number
	uint32_t magic;
	if(![_inputSource readUInt32BigEndian:&magic error:nil] || magic != 'ajkg') {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
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
											 code:SFBAudioDecoderErrorCodeInputOutput
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
	_input.SetInputCallback(^bool(void *buf, size_t len, size_t &read) {
		NSInteger bytesRead;
		if(![self->_inputSource readBytes:buf length:(NSInteger)len bytesRead:&bytesRead error:nil])
			return false;
		read = (size_t)bytesRead;
		return true;
	});

	// Read internal file type
	uint32_t ftype;
	if(![self uint_get:&ftype size:TYPESIZE]) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}
	_internal_ftype = (int)ftype;

	// Read number of channels
	uint32_t nchan;
	if(![self uint_get:&nchan size:CHANSIZE] || nchan == 0 || nchan > MAX_CHANNELS) {
		os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported channel count: %u", nchan);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Invalid or unsupported number of channels", @"")
							   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported number of channels.", @"")];
		return NO;
	}
	_nchan = (int)nchan;

	uint32_t maxnlpc = 0;

	// Read blocksize if version > 0
	if(_version > 0) {
		uint32_t blocksize;
		if(![self uint_get:&blocksize size:(size_t)log2(DEFAULT_BLOCK_SIZE)] || blocksize == 0 || blocksize > MAX_BLOCKSIZE) {
			os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported block size: %u", blocksize);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Invalid or unsupported block size", @"")
								   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported block size.", @"")];
			return NO;
		}
		_blocksize = (int)blocksize;

		if(![self uint_get:&maxnlpc size:LPCQSIZE] || maxnlpc > 1024) {
			os_log_error(gSFBAudioDecoderLog, "Invalid maxnlpc: %u", maxnlpc);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		uint32_t nmean;
		if(![self uint_get:&nmean size:0] || nmean > 32768) {
			os_log_error(gSFBAudioDecoderLog, "Invalid nmean: %u", nmean);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
		_nmean = (int)nmean;

		uint32_t nskip;
		if(![self uint_get:&nskip size:NSKIPSIZE] /* || nskip > bits_remaining_in_input */) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		for(uint32_t i = 0; i < nskip; ++i) {
			uint32_t dummy;
			if(![self uint_get:&dummy size:XBYTESIZE]) {
				if(error)
					*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
													 code:SFBAudioDecoderErrorCodeInputOutput
							descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
													  url:_inputSource.url
											failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
									   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
				return NO;
			}
		}
	}
	else
		_blocksize = DEFAULT_BLOCK_SIZE;

	_nwrap = std::max(NWRAP, (int)maxnlpc);

	// Parse the WAVE or AIFF header in the verbatim section

	int32_t fn;
	if(!_input.uvar_get(fn, FNSIZE) || fn != FN_VERBATIM) {
		os_log_error(gSFBAudioDecoderLog, "Missing initial verbatim section");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
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
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	NSMutableData *header = [[NSMutableData alloc] initWithLength:(NSUInteger)header_size];
	int8_t *header_bytes = (int8_t *)[header mutableBytes];
	for(int32_t i = 0; i < header_size; ++i) {
		int32_t byte;
		if(!_input.uvar_get(byte, VERBATIM_BYTE_SIZE)) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		header_bytes[i] = (int8_t)byte;
	}

	ByteStream chunkData{header_bytes, header.length};
	auto chunkID = chunkData.ReadBE32();

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
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported data format", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's data format is not supported.", @"")];
		return NO;
	}

	return YES;
}

- (BOOL)parseRIFFChunk:(ByteStream&)chunkData error:(NSError **)error
{
	auto chunkID = chunkData.ReadBE32();
	if(chunkID != 'WAVE') {
		os_log_error(gSFBAudioDecoderLog, "Missing 'WAVE' in 'RIFF' chunk");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// Skip unknown chunks, looking for 'fmt '
	while((chunkID = chunkData.ReadBE32()) != 'fmt ') {
		auto len = chunkData.ReadLE32();
		chunkData.Skip(len);
		if(chunkData.Remaining() < 16) {
			os_log_error(gSFBAudioDecoderLog, "Missing 'fmt ' chunk");
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
	}

	auto len = chunkData.ReadLE32();
	if(len < 16) {
		os_log_error(gSFBAudioDecoderLog, "'fmt ' chunk is too small (%u bytes)", len);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	auto format_tag = chunkData.ReadLE16();
	if(format_tag != WAVE_FORMAT_PCM) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported WAVE format tag: %x", format_tag);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported WAVE format tag", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's WAVE format tag is not supported.", @"")];
		return NO;
	}

	auto channels = chunkData.ReadLE16();
	if(_nchan != channels)
		os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'fmt ' chunk (%u)", _nchan, channels);
	auto sampleRate = chunkData.ReadLE32();
	chunkData.Skip(4); // average bytes per second
	chunkData.Skip(2); // block align
	auto bitsPerSample = chunkData.ReadLE16();

	if(bitsPerSample != 16 && bitsPerSample != 8) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth: %u", bitsPerSample);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported bit depth", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's bit depth is not supported.", @"")];
		return NO;
	}

	if(len > 16)
		os_log_info(gSFBAudioDecoderLog, "%u bytes in 'fmt ' chunk not parsed", len - 16);

	// Set up the processing format
	AudioStreamBasicDescription processingStreamDescription;

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;

	processingStreamDescription.mSampleRate			= sampleRate;
	processingStreamDescription.mChannelsPerFrame	= (UInt32)_nchan;
	processingStreamDescription.mBitsPerChannel		= bitsPerSample;

	processingStreamDescription.mBytesPerPacket		= (bitsPerSample + 7) / 8;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket * processingStreamDescription.mFramesPerPacket;

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
	AudioStreamBasicDescription sourceStreamDescription;

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDShorten;

	sourceStreamDescription.mSampleRate			= sampleRate;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)_nchan;
	sourceStreamDescription.mBitsPerChannel		= bitsPerSample;

//		sourceStreamDescription.mFramesPerPacket	= _streamInfo.max_blocksize;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	return YES;
}

- (BOOL)parseFORMChunk:(ByteStream&)chunkData error:(NSError **)error
{
	auto chunkID = chunkData.ReadBE32();
	if(chunkID != 'AIFF' && chunkID != 'AIFC') {
		os_log_error(gSFBAudioDecoderLog, "Missing 'AIFF' or 'AIFC' in 'FORM' chunk");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// Skip unknown chunks, looking for 'COMM'
	while((chunkID = chunkData.ReadBE32()) != 'COMM') {
		auto len = chunkData.ReadBE32();
		if((int32_t)len < 0 || chunkData.Remaining() < 18 + len + (len & 1)) {
			os_log_error(gSFBAudioDecoderLog, "Missing 'COMM' chunk");
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
		chunkData.Skip(len + (len & 1));
	}

	auto len = chunkData.ReadBE32();
	if((int32_t)len < 18) {
		os_log_error(gSFBAudioDecoderLog, "'COMM' chunk is too small (%u bytes)", len);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	chunkData.Skip(6);

	auto bitsPerSample = chunkData.ReadBE16();
	if(bitsPerSample != 16 && bitsPerSample != 8) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth: %u", bitsPerSample);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported bit depth", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's bit depth is not supported.", @"")];
		return NO;
	}

	auto exp = (int16_t)chunkData.ReadBE16() - 16383 - 63;
	if(exp < -63 || exp > 63) {
		os_log_error(gSFBAudioDecoderLog, "exp out of range: %d", exp);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	uint32_t sampleRate;

	auto val = chunkData.ReadBE64();
	if(exp >= 0)
		sampleRate = (uint32_t)(val << exp);
	else
		sampleRate = (uint32_t)((val + (1 << (-exp - 1))) >> -exp);

	if(len > 18)
		os_log_info(gSFBAudioDecoderLog, "%u bytes in 'COMM' chunk not parsed", len - 16);

	// Set up the processing format
	AudioStreamBasicDescription processingStreamDescription;

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;

	processingStreamDescription.mSampleRate			= sampleRate;
	processingStreamDescription.mChannelsPerFrame	= (UInt32)_nchan;
	processingStreamDescription.mBitsPerChannel		= bitsPerSample;

	processingStreamDescription.mBytesPerPacket		= (bitsPerSample + 7) / 8;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket * processingStreamDescription.mFramesPerPacket;

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
	AudioStreamBasicDescription sourceStreamDescription;

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDShorten;

	sourceStreamDescription.mSampleRate			= sampleRate;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)_nchan;
	sourceStreamDescription.mBitsPerChannel		= bitsPerSample;

//		sourceStreamDescription.mFramesPerPacket	= _streamInfo.max_blocksize;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	return YES;
}

- (BOOL)uint_get:(uint32_t *)result size:(size_t)size
{
	if(_version == 0) {
		int32_t i32;
		if(!_input.uvar_get(i32, size))
			return NO;
		*result = (uint32_t)i32;
		return YES;
	}
	else
		return _input.ulong_get(*result);
}

@end
