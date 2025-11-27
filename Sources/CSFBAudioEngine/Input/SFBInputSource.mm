//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <system_error>

#import "SFBInputSource+Internal.h"

#import "BufferInput.hpp"
#import "DataInput.hpp"
#import "FileContentsInput.hpp"
#import "FileInput.hpp"
#import "MemoryMappedFileInput.hpp"

#import "NSData+SFBExtensions.h"

namespace {

NSError * NSErrorFromInputSourceException(const std::exception *e) noexcept
{
	NSCParameterAssert(e != nullptr);

	// TODO: Set NSURLErrorKey?

	if(const auto se = dynamic_cast<const std::system_error *>(e); se)
		return [NSError errorWithDomain:NSPOSIXErrorDomain code:se->code().value() userInfo:@{ NSDebugDescriptionErrorKey: @(se->code().message().c_str()) }];

	if(const auto ia = dynamic_cast<const std::invalid_argument *>(e); ia)
		return [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:@{ NSDebugDescriptionErrorKey: @(ia->what()) }];

	if(const auto oor = dynamic_cast<const std::out_of_range *>(e); oor)
		return [NSError errorWithDomain:NSPOSIXErrorDomain code:EDOM userInfo:@{ NSDebugDescriptionErrorKey: @(oor->what()) }];

	return [NSError errorWithDomain:SFBInputSourceErrorDomain code:SFBInputSourceErrorCodeInputOutput userInfo:@{ NSDebugDescriptionErrorKey: @(e->what()) }];
}

} /* namespace */

// NSError domain for InputSource and subclasses
NSErrorDomain const SFBInputSourceErrorDomain = @"org.sbooth.AudioEngine.InputSource";

@implementation SFBInputSource

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBInputSourceErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
			switch(err.code) {
				case SFBInputSourceErrorCodeFileNotFound:
					return NSLocalizedString(@"The requested file was not found.", @"");
				case SFBInputSourceErrorCodeInputOutput:
					return NSLocalizedString(@"An input/output error occurred.", @"");
				case SFBInputSourceErrorCodeNotSeekable:
					return NSLocalizedString(@"The input does not support seeking.", @"");
			}
		}
		return nil;
	}];
}

+ (instancetype)inputSourceForURL:(NSURL *)url error:(NSError **)error
{
	return [SFBInputSource inputSourceForURL:url flags:0 error:error];
}

+ (instancetype)inputSourceForURL:(NSURL *)url flags:(SFBInputSourceFlags)flags error:(NSError **)error
{
	NSParameterAssert(url != nil);
	NSParameterAssert(url.isFileURL);

	try {
		SFBInputSource *inputSource = [[SFBInputSource alloc] init];
		if(inputSource) {
			if(flags & SFBInputSourceFlagsMemoryMapFiles)
				inputSource->_input = std::make_unique<SFB::MemoryMappedFileInput>((__bridge CFURLRef)url);
			else if(flags & SFBInputSourceFlagsLoadFilesInMemory)
				inputSource->_input = std::make_unique<SFB::FileContentsInput>((__bridge CFURLRef)url);
			else
				inputSource->_input = std::make_unique<SFB::FileInput>((__bridge CFURLRef)url);
		}
		return inputSource;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return nil;
	}
}

+ (instancetype)inputSourceWithData:(NSData *)data
{
	NSParameterAssert(data != nil);

	try {
		SFBInputSource *inputSource = [[SFBInputSource alloc] init];
		if(inputSource)
			inputSource->_input = std::make_unique<SFB::DataInput>((__bridge CFDataRef)data);
		return inputSource;
	}
	catch(const std::exception& e) {
		return nil;
	}
}

+ (instancetype)inputSourceWithBytes:(const void *)bytes length:(NSInteger)length
{
	NSParameterAssert(bytes != nullptr);
	NSParameterAssert(length >= 0);

	try {
		SFBInputSource *inputSource = [[SFBInputSource alloc] init];
		if(inputSource)
			inputSource->_input = std::make_unique<SFB::BufferInput>(bytes, length);
		return inputSource;
	}
	catch(const std::exception& e) {
		return nil;
	}
}

+ (instancetype)inputSourceWithBytesNoCopy:(void *)bytes length:(NSInteger)length freeWhenDone:(BOOL)freeWhenDone
{
	NSParameterAssert(bytes != nullptr);
	NSParameterAssert(length >= 0);

	try {
		SFBInputSource *inputSource = [[SFBInputSource alloc] init];
		if(inputSource)
			inputSource->_input = std::make_unique<SFB::BufferInput>(bytes, length, freeWhenDone ? SFB::BufferInput::BufferAdoption::noCopyAndFree : SFB::BufferInput::BufferAdoption::noCopy);
		return inputSource;
	}
	catch(const std::exception& e) {
		return nil;
	}
}

- (void)dealloc
{
	_input.reset();
}

- (NSURL *)url
{
	return (__bridge NSURL *)_input->GetURL();
}

- (BOOL)openReturningError:(NSError **)error
{
	try {
		_input->Open();
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)closeReturningError:(NSError **)error
{
	try {
		_input->Close();
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)isOpen
{
	return _input->IsOpen();
}

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error
{
	NSParameterAssert(bytesRead != nullptr);

	try {
		*bytesRead = _input->Read(buffer, length);
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)atEOF
{
	try {
		return _input->AtEOF();
	}
	catch(const std::exception& e) {
		// FIXME: Is `NO` the best error return?
		return NO;
	}
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error
{
	NSParameterAssert(offset != nullptr);

	try {
		*offset = _input->Offset();
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)getLength:(NSInteger *)length error:(NSError **)error
{
	NSParameterAssert(length != nullptr);

	try {
		*length = _input->Length();
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)supportsSeeking
{
	return _input->SupportsSeeking();
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error
{
	try {
		_input->SeekToOffset(offset);
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (NSString *)description
{
	return (__bridge_transfer NSString *)_input->CopyDescription();
}

@end

@implementation SFBInputSource (SFBSignedIntegerReading)
- (BOOL)readInt8:(int8_t *)i8 error:(NSError **)error			{ return [self readUInt8:(uint8_t *)i8 error:error]; }
- (BOOL)readInt16:(int16_t *)i16 error:(NSError **)error		{ return [self readUInt16:(uint16_t *)i16 error:error]; }
- (BOOL)readInt32:(int32_t *)i32 error:(NSError **)error		{ return [self readUInt32:(uint32_t *)i32 error:error]; }
- (BOOL)readInt64:(int64_t *)i64 error:(NSError **)error		{ return [self readUInt64:(uint64_t *)i64 error:error]; }
@end

@implementation SFBInputSource (SFBUnsignedIntegerReading)
- (BOOL)readUInt8:(uint8_t *)ui8 error:(NSError **)error
{
	NSParameterAssert(ui8 != nil);
	try {
		*ui8 = _input->ReadValue<uint8_t>();
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)readUInt16:(uint16_t *)ui16 error:(NSError **)error
{
	NSParameterAssert(ui16 != nil);
	try {
		*ui16 = _input->ReadUnsigned<uint16_t>();
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)readUInt32:(uint32_t *)ui32 error:(NSError **)error
{
	NSParameterAssert(ui32 != nil);
	try {
		*ui32 = _input->ReadUnsigned<uint32_t>();
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)readUInt64:(uint64_t *)ui64 error:(NSError **)error
{
	NSParameterAssert(ui64 != nil);
	try {
		*ui64 = _input->ReadUnsigned<uint64_t>();
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

@end

@implementation SFBInputSource (SFBBigEndianReading)

- (BOOL)readUInt16BigEndian:(uint16_t *)ui16 error:(NSError **)error
{
	NSParameterAssert(ui16 != nil);
	try {
		*ui16 = _input->ReadUnsigned<uint16_t>(SFB::InputSource::ByteOrder::big);
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)readUInt32BigEndian:(uint32_t *)ui32 error:(NSError **)error
{
	NSParameterAssert(ui32 != nil);
	try {
		*ui32 = _input->ReadUnsigned<uint32_t>(SFB::InputSource::ByteOrder::big);
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)readUInt64BigEndian:(uint64_t *)ui64 error:(NSError **)error
{
	NSParameterAssert(ui64 != nil);
	try {
		*ui64 = _input->ReadUnsigned<uint64_t>(SFB::InputSource::ByteOrder::big);
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

@end

@implementation SFBInputSource (SFBLittleEndianReading)

- (BOOL)readUInt16LittleEndian:(uint16_t *)ui16 error:(NSError **)error
{
	NSParameterAssert(ui16 != nil);
	try {
		*ui16 = _input->ReadUnsigned<uint16_t>(SFB::InputSource::ByteOrder::little);
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)readUInt32LittleEndian:(uint32_t *)ui32 error:(NSError **)error
{
	NSParameterAssert(ui32 != nil);
	try {
		*ui32 = _input->ReadUnsigned<uint32_t>(SFB::InputSource::ByteOrder::little);
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

- (BOOL)readUInt64LittleEndian:(uint64_t *)ui64 error:(NSError **)error
{
	NSParameterAssert(ui64 != nil);
	try {
		*ui64 = _input->ReadUnsigned<uint64_t>(SFB::InputSource::ByteOrder::little);
		return YES;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return NO;
	}
}

@end

@implementation SFBInputSource (SFBDataReading)

- (NSData *)readDataOfLength:(NSUInteger)length error:(NSError **)error
{
	try {
		return (__bridge_transfer NSData *)_input->CopyData(length);
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return nil;
	}
}

@end

@implementation SFBInputSource (SFBHeaderReading)

- (NSData *)readHeaderOfLength:(NSUInteger)length skipID3v2Tag:(BOOL)skipID3v2Tag error:(NSError **)error
{
	NSParameterAssert(length > 0);

	if(!_input->SupportsSeeking()) {
		if(error)
			*error = [NSError errorWithDomain:SFBInputSourceErrorDomain code:SFBInputSourceErrorCodeNotSeekable userInfo:nil];
		return nil;
	}

	try {
		const auto originalOffset = _input->Offset();
		_input->SeekToOffset(0);

		if(skipID3v2Tag) {
			int64_t offset = 0;

			// Attempt to detect and minimally parse an ID3v2 tag header
			NSData *data = (__bridge_transfer NSData *)_input->CopyData(SFBID3v2HeaderSize);
			if([data isID3v2Header])
				offset = [data id3v2TagTotalSize];

			_input->SeekToOffset(offset);
		}

		NSData *data = (__bridge_transfer NSData *)_input->CopyData(length);
		if(data.length < length) {
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:@{ NSURLErrorKey: self.url }];
			return nil;
		}

		_input->SeekToOffset(originalOffset);

		return data;
	}
	catch(const std::exception& e) {
		if(error)
			*error = NSErrorFromInputSourceException(&e);
		return nil;
	}
}

@end
