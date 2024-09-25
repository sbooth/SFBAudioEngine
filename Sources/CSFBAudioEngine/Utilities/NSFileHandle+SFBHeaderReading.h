//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Header reading
@interface NSFileHandle (SFBHeaderReading)
/// Reads data from the beginning of the file handle, optionally skipping a leading ID3v2 tag if present
/// - parameter length: The number of bytes to read
/// - parameter skipID3v2Tag: Whether to skip a leading ID3v2 tag if present
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `NSData` object containing `length` bytes following the optional leading ID3v2 tag, `nil` otherwise
- (nullable NSData *)readHeaderOfLength:(NSUInteger)length skipID3v2Tag:(BOOL)skipID3v2Tag error:(NSError **)error;
@end

NS_ASSUME_NONNULL_END
