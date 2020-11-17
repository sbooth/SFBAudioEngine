/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBDSFDecoder.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBCStringForOSType.h"

#define DSF_BLOCK_SIZE_BYTES_PER_CHANNEL 4096

static inline AVAudioPacketCount SFB_min(AVAudioPacketCount a, AVAudioPacketCount b) { return a < b ? a : b; }

// Read a four byte chunk ID as a uint32_t
static BOOL ReadChunkID(SFBInputSource *inputSource, uint32_t *chunkID)
{
	NSCParameterAssert(chunkID != NULL);

	char chunkIDBytes [4];
	NSInteger bytesRead;
	if(![inputSource readBytes:chunkIDBytes length:4 bytesRead:&bytesRead error:nil] || bytesRead != 4) {
		os_log_error(gSFBDSDDecoderLog, "Unable to read chunk ID");
		return NO;
	}

	*chunkID = (uint32_t)((chunkIDBytes[0] << 24u) | (chunkIDBytes[1] << 16u) | (chunkIDBytes[2] << 8u) | chunkIDBytes[3]);
	return YES;
}

static NSError * CreateInvalidDSFFileError(NSURL * url)
{
	return [NSError SFB_errorWithDomain:SFBDSDDecoderErrorDomain
								   code:SFBDSDDecoderErrorCodeInputOutput
		  descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid DSF file.", @"")
									url:url
						  failureReason:NSLocalizedString(@"Not a DSF file", @"")
					 recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
}

// For the size of matrices this class deals with the naive approach is adequate
static void MatrixTransposeNaive(const uint8_t * restrict A, uint8_t * restrict B, NSInteger rows, NSInteger columns)
{
	for(NSInteger i = 0; i < rows; ++i) {
		for(NSInteger j = 0; j < columns; ++j)
			B[j * rows + i] = A[i * columns + j];
	}
}

@interface SFBDSFDecoder ()
{
@private
	AVAudioFramePosition _packetPosition;
	AVAudioFramePosition _packetCount;
	int64_t _audioOffset;
	AVAudioCompressedBuffer *_buffer;
}
- (BOOL)readAndInterleaveDSFBlockReturningError:(NSError **)error;
@end

@implementation SFBDSFDecoder

+ (void)load
{
	[SFBDSDDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"dsf"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/dsf"];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	// Read the 'DSD ' chunk
	uint32_t chunkID;
	if(!ReadChunkID(_inputSource, &chunkID) || chunkID != 'DSD ') {
		os_log_error(gSFBDSDDecoderLog, "Unable to read 'DSD ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	uint64_t chunkSize, fileSize, metadataOffset;
	// Unlike normal IFF, the chunkSize includes the size of the chunk ID and size
	if(![_inputSource readUInt64LittleEndian:&chunkSize error:nil] || chunkSize != 28) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected 'DSD ' chunk size: %llu", chunkSize);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt64LittleEndian:&fileSize error:nil]) {
		os_log_error(gSFBDSDDecoderLog, "Unable to read file size in 'DSD ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt64LittleEndian:&metadataOffset error:nil]) {
		os_log_error(gSFBDSDDecoderLog, "Unable to read metadata offset in 'DSD ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}


	// Read the 'fmt ' chunk
	if(!ReadChunkID(_inputSource, &chunkID) || chunkID != 'fmt ') {
		os_log_error(gSFBDSDDecoderLog, "Unable to read 'fmt ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt64LittleEndian:&chunkSize error:nil]) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected 'fmt ' chunk size: %llu", chunkSize);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	uint32_t formatVersion, formatID, channelType, channelNum, samplingFrequency, bitsPerSample;
	uint64_t sampleCount;
	uint32_t blockSizePerChannel, reserved;

	if(![_inputSource readUInt32LittleEndian:&formatVersion error:nil] || formatVersion != 1) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected format version in 'fmt ': %u", formatVersion);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt32LittleEndian:&formatID error:nil] || formatID != 0) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected format ID in 'fmt ': %{public}.4s", SFBCStringForOSType(formatID));
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt32LittleEndian:&channelType error:nil] || (channelType < 1 || channelType > 7)) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected channel type in 'fmt ': %u", channelType);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt32LittleEndian:&channelNum error:nil] || (channelNum < 1 || channelNum > 6)) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected channel count in 'fmt ': %u", channelNum);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt32LittleEndian:&samplingFrequency error:nil] || (samplingFrequency != SFBDSDSampleRateDSD64 && samplingFrequency != SFBDSDSampleRateDSD128)) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected sample rate in 'fmt ': %u", samplingFrequency);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt32LittleEndian:&bitsPerSample error:nil] || (bitsPerSample != 1 && bitsPerSample != 8)) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected bits per sample in 'fmt ': %u", bitsPerSample);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt64LittleEndian:&sampleCount error:nil]) {
		os_log_error(gSFBDSDDecoderLog, "Unable to read sample count in 'fmt ' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt32LittleEndian:&blockSizePerChannel error:nil] || blockSizePerChannel != DSF_BLOCK_SIZE_BYTES_PER_CHANNEL) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected block size per channel in 'fmt ': %u", blockSizePerChannel);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt32LittleEndian:&reserved error:nil] || reserved != 0) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected non-zero value for reserved in 'fmt ': %u", reserved);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}


	// Read the 'data' chunk
	if(!ReadChunkID(_inputSource, &chunkID) || chunkID != 'data') {
		os_log_error(gSFBDSDDecoderLog, "Unable to read 'data' chunk");
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	if(![_inputSource readUInt64LittleEndian:&chunkSize error:nil]) {
		os_log_error(gSFBDSDDecoderLog, "Unexpected 'data' chunk size: %llu", chunkSize);
		if(error)
			*error = CreateInvalidDSFFileError(_inputSource.url);
		return NO;
	}

	_packetCount = sampleCount / SFB_PCM_FRAMES_PER_DSD_PACKET;
	NSInteger offset;
	if(![_inputSource getOffset:&offset error:nil]) {
		os_log_error(gSFBDSDDecoderLog, "Error getting audio offset");
		return NO;
	}
	_audioOffset = offset;

	// Channel layouts are defined in the DSF file format specification
	AVAudioChannelLayout *channelLayout = nil;
	switch(channelType) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];			break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];		break;
		case 3:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_3_0_A];	break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];	break;
		case 5:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_ITU_2_2];		break;
		case 6:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_0_A];	break;
		case 7:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_1_A];	break;
	}

	AudioStreamBasicDescription processingStreamDescription = {0};

	// The output format is raw DSD
	processingStreamDescription.mFormatID			= SFBAudioFormatIDDirectStreamDigital;
	processingStreamDescription.mFormatFlags		= bitsPerSample == 8 ? kAudioFormatFlagIsBigEndian : 0;

	processingStreamDescription.mSampleRate			= (Float64)samplingFrequency;
	processingStreamDescription.mChannelsPerFrame	= (UInt32)channelNum;
	processingStreamDescription.mBitsPerChannel		= 1;

	processingStreamDescription.mBytesPerPacket		= SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * channelNum;
	processingStreamDescription.mFramesPerPacket	= SFB_PCM_FRAMES_PER_DSD_PACKET;

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDDirectStreamDigital;

	sourceStreamDescription.mSampleRate			= (Float64)samplingFrequency;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)channelNum;
	sourceStreamDescription.mBitsPerChannel		= 1;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	// Metadata chunk is ignored

	_buffer = [[AVAudioCompressedBuffer alloc] initWithFormat:_processingFormat packetCapacity:(DSF_BLOCK_SIZE_BYTES_PER_CHANNEL / SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL) maximumPacketSize:(SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * channelNum)];
	_buffer.packetCount = 0;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_buffer = nil;
	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _buffer != nil;
}

- (AVAudioFramePosition)packetPosition
{
	return _packetPosition;
}

- (AVAudioFramePosition)packetCount
{
	return _packetCount;
}

- (BOOL)decodeIntoBuffer:(AVAudioCompressedBuffer *)buffer packetCount:(AVAudioPacketCount)packetCount error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.packetCount = 0;
	buffer.byteLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBDSDDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(packetCount > buffer.packetCapacity)
		packetCount = buffer.packetCapacity;

	AVAudioPacketCount packetsProcessed = 0;

	uint32_t packetSize = SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * _processingFormat.channelCount;

	for(;;) {
		AVAudioPacketCount packetsRemaining = packetCount - packetsProcessed;
		AVAudioPacketCount packetsToSkip = buffer.packetCount;
		AVAudioPacketCount packetsInBuffer = _buffer.packetCount;
		AVAudioPacketCount packetsToCopy = SFB_min(packetsInBuffer, packetsRemaining);

		// Copy data from the internal buffer to output
		uint32_t copySize = packetsToCopy * packetSize;
		memcpy((uint8_t *)buffer.data + (packetsToSkip * packetSize), _buffer.data, copySize);
		buffer.packetCount += packetsToCopy;
		buffer.byteLength += copySize;

		// Move remaining data in buffer to beginning
		if(packetsToCopy != packetsInBuffer) {
			uint8_t *dst = (uint8_t *)_buffer.data;
			memmove(dst, dst + copySize, (packetsInBuffer - packetsToCopy) * packetSize);
		}

		_buffer.packetCount -= packetsToCopy;
		_buffer.byteLength -= copySize;

		packetsProcessed += packetsToCopy;

		// All requested packets were read
		if(packetsProcessed == packetCount)
			break;

		// Read  the next block
		if(![self readAndInterleaveDSFBlockReturningError:error])
			break;
	}

	_packetPosition += packetsProcessed;

	return YES;
}

- (BOOL)seekToPacket:(AVAudioFramePosition)packet error:(NSError **)error
{
	NSParameterAssert(packet >= 0);

	AVAudioChannelCount channelCount = _processingFormat.channelCount;

	// A DSF version 1 block is 4096 bytes per channel
	// This equates to 4096 packets or 32768 frames per block

	// Seek to the start of the block containing packet
	NSInteger blockNumber = packet / DSF_BLOCK_SIZE_BYTES_PER_CHANNEL;
	NSInteger blockOffset = blockNumber * DSF_BLOCK_SIZE_BYTES_PER_CHANNEL * channelCount;

	if(![_inputSource seekToOffset:(_audioOffset + blockOffset) error:error]) {
		os_log_debug(gSFBDSDDecoderLog, "-seekToPacket:error: failed seeking to input offset: %lld", _audioOffset + blockOffset);
		return NO;
	}

	if(![self readAndInterleaveDSFBlockReturningError:error])
		return NO;

	// Skip ahead in the interleaved audio to the specified packet
	AVAudioPacketCount packetsInBuffer = _buffer.packetCount;
	AVAudioPacketCount packetsToSkip = packet % packetsInBuffer;
	AVAudioPacketCount packetsToMove = packetsInBuffer - packetsToSkip;

	// Move data
	uint32_t packetSize = SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * _processingFormat.channelCount;
	uint8_t *dst = (uint8_t *)_buffer.data;
	const uint8_t *src = (uint8_t *)_buffer.data + (packetsToSkip * packetSize);
	memmove(dst, src, packetsToMove * packetSize);

	_buffer.packetCount = packetsToMove;
	_buffer.byteLength = packetsToMove * packetSize;

	_packetPosition = packet;

	return YES;
}

// Read input, grouped in DSF as 8 one-bit samples per frame (a single channel byte) in a block
// of the specified block size (4096 bytes per channel for DSF version 1) for each channel,
// then interleave the channel bytes into clustered frames.
// The DSF blocks form a matrix with one row per channel and one column per channel byte.
// For stereo, the data is arranged as 4096 L channel bytes followed by 4096 R channel bytes,
// a 2 x 4096 matrix.
// Interleaving is accomplished by matrix transposition.
- (BOOL)readAndInterleaveDSFBlockReturningError:(NSError **)error
{
	uint8_t *buf = (uint8_t *)_buffer.data;
	uint32_t bufsize = _buffer.byteCapacity;

	NSInteger bytesRead;
	if(![_inputSource readBytes:buf length:bufsize bytesRead:&bytesRead error:error] || bytesRead != bufsize) {
		os_log_debug(gSFBDSDDecoderLog, "Error reading audio block: requested %u bytes, got %ld", bufsize, bytesRead);
		return NO;
	}

	// Deinterleave the blocks and interleave the samples into clustered frames
	AVAudioChannelCount channelCount = _processingFormat.channelCount;
	assert(channelCount != 0);
	uint8_t tmp [bufsize];
	MatrixTransposeNaive(buf, tmp, channelCount, DSF_BLOCK_SIZE_BYTES_PER_CHANNEL);
	memcpy(buf, tmp, bufsize);

	_buffer.packetCount = (AVAudioPacketCount)(bytesRead / (SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * channelCount));
	_buffer.byteLength = (uint32_t)bytesRead;

	return YES;
}

@end
