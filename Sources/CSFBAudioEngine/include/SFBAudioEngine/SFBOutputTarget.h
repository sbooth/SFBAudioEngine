//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// An output target
NS_SWIFT_NAME(OutputTarget)
@interface SFBOutputTarget : NSObject

/// Returns an initialized `SFBOutputTarget` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBOutputTarget` object for the specified URL, or `nil` on failure
+ (nullable instancetype)outputTargetForURL:(NSURL *)url error:(NSError **)error;

/// Returns an initialized `SFBOutputTarget` writing to an internal data object
+ (instancetype)dataOutputTarget NS_SWIFT_NAME(makeForData());

/// Returns an initialized `SFBOutputTarget` for the given buffer
/// - parameter buffer: A buffer to receive output
/// - parameter capacity: The capacity of `buffer` in bytes
/// - returns: An initialized `SFBOutputTarget` object
+ (instancetype)outputTargetWithBuffer:(void *)buffer capacity:(NSInteger)capacity;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// The URL corresponding to this output target or `nil` if none
@property(nonatomic, nullable, readonly) NSURL *url;

/// The underlying data object for this output target or `nil` if none
@property(nonatomic, nullable, readonly) NSData *data;

/// Opens the output target for writing
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/// Closes the output target
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// `YES` if the output target is open
@property(nonatomic, readonly) BOOL isOpen;

/// Reads bytes from the output
/// - parameter buffer: A buffer to receive data
/// - parameter length: The maximum number of bytes to read
/// - parameter bytesRead: The number of bytes actually read
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if any bytes were read, `NO` otherwise
- (BOOL)readBytes:(void *)buffer
           length:(NSInteger)length
        bytesRead:(NSInteger *)bytesRead
            error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Writes bytes to the output
/// - parameter buffer: A buffer of data to write
/// - parameter length: The maximum number of bytes to write
/// - parameter bytesWritten: The number of bytes actually written
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if any bytes were written, `NO` otherwise
- (BOOL)writeBytes:(const void *)buffer
              length:(NSInteger)length
        bytesWritten:(NSInteger *)bytesWritten
               error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// `YES` if the end of input has been reached
@property(nonatomic, readonly) BOOL atEOF;

/// Returns the current offset in the output, in bytes
- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the length of the output, in bytes
- (BOOL)getLength:(NSInteger *)length error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// `YES` if the output is seekable
@property(nonatomic, readonly) BOOL supportsSeeking;

/// Seeks to the specified byte offset
/// - parameter offset: The desired offset, in bytes
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error;

@end

/// Data writing
@interface SFBOutputTarget (SFBDataWriting)
/// Writes data to the output
/// - parameter data: The data to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeData:(NSData *)data error:(NSError **)error;
@end

// MARK: - Typed and Byte-Ordered Writing

/// Signed integer writing
@interface SFBOutputTarget (SFBSignedIntegerWriting)
/// Writes an 8-bit signed integer to the output
/// - parameter i8: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeInt8:(int8_t)i8 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 16-bit signed integer to the output
/// - parameter i16: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeInt16:(int16_t)i16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 32-bit signed integer to the output
/// - parameter i32: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeInt32:(int32_t)i32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 64-bit signed integer to the output
/// - parameter i64: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeInt64:(int64_t)i64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

/// Unsigned integer writing
@interface SFBOutputTarget (SFBUnsignedIntegerWriting)
/// Writes an 8-bit unsigned integer to the output
/// - parameter ui8: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt8:(uint8_t)ui8 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 16-bit unsigned integer to the output
/// - parameter ui16: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt16:(uint16_t)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 32-bit unsigned integer to the output
/// - parameter ui32: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt32:(uint32_t)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 64-bit unsigned integer to the output
/// - parameter ui64: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt64:(uint64_t)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

/// Big-endian unsigned integer writing
@interface SFBOutputTarget (SFBBigEndianWriting)
/// Writes an 16-bit unsigned integer to the output in big-endian format
/// - parameter ui16: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt16BigEndian:(uint16_t)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 32-bit unsigned integer to the output in big-endian format
/// - parameter ui32: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt32BigEndian:(uint32_t)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 64-bit unsigned integer to the output in big-endian format
/// - parameter ui64: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt64BigEndian:(uint64_t)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

/// Little-endian unsigned integer writing
@interface SFBOutputTarget (SFBLittleEndianWriting)
/// Writes an 16-bit unsigned integer to the output in little-endian format
/// - parameter ui16: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt16LittleEndian:(uint16_t)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 32-bit unsigned integer to the output in little-endian format
/// - parameter ui32: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt32LittleEndian:(uint32_t)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Writes an 64-bit unsigned integer to the output in little-endian format
/// - parameter ui64: The value to write
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)writeUInt64LittleEndian:(uint64_t)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

// MARK: - Error Information

/// The `NSErrorDomain` used by `SFBOutputTarget` and subclasses
extern NSErrorDomain const SFBOutputTargetErrorDomain NS_SWIFT_NAME(OutputTarget.ErrorDomain);

/// Possible `NSError` error codes used by `SFBOutputTarget`
typedef NS_ERROR_ENUM(SFBOutputTargetErrorDomain, SFBOutputTargetErrorCode){
    /// File not found
    SFBOutputTargetErrorCodeFileNotFound = 0,
    /// Input/output error
    SFBOutputTargetErrorCodeInputOutput = 1,
} NS_SWIFT_NAME(OutputTarget.Error);

NS_ASSUME_NONNULL_END
