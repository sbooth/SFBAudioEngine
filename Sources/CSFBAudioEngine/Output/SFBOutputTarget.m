//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBBufferOutputTarget.h"
#import "SFBFileOutputTarget.h"
#import "SFBMutableDataOutputTarget.h"
#import "SFBOutputTarget+Internal.h"

// NSError domain for OutputTarget and subclasses
NSErrorDomain const SFBOutputTargetErrorDomain = @"org.sbooth.AudioEngine.OutputTarget";

os_log_t gSFBOutputTargetLog = NULL;

static void SFBCreateOutputTargetLog(void) __attribute__((constructor));
static void SFBCreateOutputTargetLog(void) {
    gSFBOutputTargetLog = os_log_create("org.sbooth.AudioEngine", "OutputTarget");
}

@implementation SFBOutputTarget

+ (void)load {
    [NSError setUserInfoValueProviderForDomain:SFBOutputTargetErrorDomain
                                      provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
                                          switch (err.code) {
                                          case SFBOutputTargetErrorCodeFileNotFound:
                                              if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                                  return NSLocalizedString(@"The requested file was not found.", @"");
                                              }
                                              break;

                                          case SFBOutputTargetErrorCodeInputOutput:
                                              if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                                  return NSLocalizedString(@"An input/output error occurred.", @"");
                                              }
                                              break;
                                          }

                                          return nil;
                                      }];
}

+ (instancetype)outputTargetForURL:(NSURL *)url error:(NSError **)error {
    NSParameterAssert(url != nil);

    if (url.isFileURL) {
        return [[SFBFileOutputTarget alloc] initWithURL:url];
    }

    if (error) {
        *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:@{NSURLErrorKey : url}];
    }
    return nil;
}

+ (instancetype)dataOutputTarget {
    return [[SFBMutableDataOutputTarget alloc] init];
}

+ (instancetype)outputTargetWithBuffer:(void *)buffer capacity:(NSInteger)capacity {
    NSParameterAssert(buffer != NULL);
    NSParameterAssert(capacity >= 0);
    return [[SFBBufferOutputTarget alloc] initWithBuffer:buffer capacity:(size_t)capacity];
}

- (instancetype)initWithURL:(NSURL *)url {
    if ((self = [super init])) {
        _url = url;
    }
    return self;
}

- (void)dealloc {
    if (self.isOpen) {
        [self closeReturningError:nil];
    }
}

- (NSData *)data {
    return nil;
}

- (BOOL)openReturningError:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)closeReturningError:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)isOpen {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)writeBytes:(const void *)buffer
              length:(NSInteger)length
        bytesWritten:(NSInteger *)bytesWritten
               error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)getLength:(NSInteger *)length error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)supportsSeeking {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (NSError *)posixErrorWithCode:(NSInteger)code {
    NSDictionary *userInfo = nil;
    if (_url) {
        userInfo = [NSDictionary dictionaryWithObject:_url forKey:NSURLErrorKey];
    }
    return [NSError errorWithDomain:NSPOSIXErrorDomain code:code userInfo:userInfo];
}

- (NSString *)description {
    if (_url) {
        return [NSString stringWithFormat:@"<%@ %p: \"%@\">", [self class], (__bridge void *)self,
                                          [[NSFileManager defaultManager] displayNameAtPath:_url.path]];
    }
    return [NSString stringWithFormat:@"<%@ %p>", [self class], (__bridge void *)self];
}

@end

@implementation SFBOutputTarget (SFBDataWriting)
- (BOOL)writeData:(NSData *)data error:(NSError **)error {
    NSParameterAssert(data != nil);
    NSInteger bytesWritten;
    return [self writeBytes:data.bytes length:(NSInteger)data.length bytesWritten:&bytesWritten error:error] &&
           bytesWritten == (NSInteger)data.length;
}
@end

@implementation SFBOutputTarget (SFBSignedIntegerWriting)
- (BOOL)writeInt8:(int8_t)i8 error:(NSError **)error {
    return [self writeUInt8:(uint8_t)i8 error:error];
}
- (BOOL)writeInt16:(int16_t)i16 error:(NSError **)error {
    return [self writeUInt16:(uint16_t)i16 error:error];
}
- (BOOL)writeInt32:(int32_t)i32 error:(NSError **)error {
    return [self writeUInt32:(uint32_t)i32 error:error];
}
- (BOOL)writeInt64:(int64_t)i64 error:(NSError **)error {
    return [self writeUInt64:(uint64_t)i64 error:error];
}
@end

@implementation SFBOutputTarget (SFBUnsignedIntegerWriting)
- (BOOL)writeUInt8:(uint8_t)ui8 error:(NSError **)error {
    NSInteger bytesWritten;
    return [self writeBytes:&ui8 length:sizeof(uint8_t) bytesWritten:&bytesWritten error:error] &&
           bytesWritten == sizeof(uint8_t);
}

- (BOOL)writeUInt16:(uint16_t)ui16 error:(NSError **)error {
    NSInteger bytesWritten;
    return [self writeBytes:&ui16 length:sizeof(uint16_t) bytesWritten:&bytesWritten error:error] &&
           bytesWritten == sizeof(uint16_t);
}

- (BOOL)writeUInt32:(uint32_t)ui32 error:(NSError **)error {
    NSInteger bytesWritten;
    return [self writeBytes:&ui32 length:sizeof(uint32_t) bytesWritten:&bytesWritten error:error] &&
           bytesWritten == sizeof(uint32_t);
}

- (BOOL)writeUInt64:(uint64_t)ui64 error:(NSError **)error {
    NSInteger bytesWritten;
    return [self writeBytes:&ui64 length:sizeof(uint64_t) bytesWritten:&bytesWritten error:error] &&
           bytesWritten == sizeof(uint64_t);
}

@end

@implementation SFBOutputTarget (SFBBigEndianWriting)
- (BOOL)writeUInt16BigEndian:(uint16_t)ui16 error:(NSError **)error {
    return [self writeUInt16:OSSwapHostToBigInt16(ui16) error:error];
}
- (BOOL)writeUInt32BigEndian:(uint32_t)ui32 error:(NSError **)error {
    return [self writeUInt32:OSSwapHostToBigInt32(ui32) error:error];
}
- (BOOL)writeUInt64BigEndian:(uint64_t)ui64 error:(NSError **)error {
    return [self writeUInt64:OSSwapHostToBigInt64(ui64) error:error];
}
@end

@implementation SFBOutputTarget (SFBLittleEndianWriting)
- (BOOL)writeUInt16LittleEndian:(uint16_t)ui16 error:(NSError **)error {
    return [self writeUInt16:OSSwapHostToLittleInt16(ui16) error:error];
}
- (BOOL)writeUInt32LittleEndian:(uint32_t)ui32 error:(NSError **)error {
    return [self writeUInt32:OSSwapHostToLittleInt32(ui32) error:error];
}
- (BOOL)writeUInt64LittleEndian:(uint64_t)ui64 error:(NSError **)error {
    return [self writeUInt64:OSSwapHostToLittleInt64(ui64) error:error];
}
@end
