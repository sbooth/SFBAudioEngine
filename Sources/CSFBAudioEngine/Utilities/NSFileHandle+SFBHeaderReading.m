//
// Copyright (c) 2010-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"

@implementation NSFileHandle (SFBHeaderReading)

- (NSData *)readHeaderOfLength:(NSUInteger)length skipID3v2Tag:(BOOL)skipID3v2Tag error:(NSError **)error {
    NSParameterAssert(length > 0);

    unsigned long long originalOffset;
    if (![self getOffset:&originalOffset error:error])
        return nil;

    if (![self seekToOffset:0 error:error])
        return nil;

    if (skipID3v2Tag) {
        NSInteger offset = 0;

        // Attempt to detect and minimally parse an ID3v2 tag header
        NSData *data = [self readDataUpToLength:SFBID3v2HeaderSize error:error];
        if ([data isID3v2Header])
            offset = [data id3v2TagTotalSize];

        if (![self seekToOffset:offset error:error])
            return nil;
    }

    NSData *data = [self readDataUpToLength:length error:error];
    if (!data)
        return nil;

    if (data.length < length) {
        if (error)
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
        return nil;
    }

    if (![self seekToOffset:originalOffset error:error])
        return nil;

    return data;
}

@end
