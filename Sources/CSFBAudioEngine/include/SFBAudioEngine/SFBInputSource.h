//
// Copyright (c) 2010-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Bitmask values used in `+inputSourceForURL:flags:error:`
typedef NS_OPTIONS(NSUInteger, SFBInputSourceFlags) {
	/// Files should be mapped in memory using `mmap()`
	SFBInputSourceFlagsMemoryMapFiles			= 1 << 0,
	/// Files should be fully loaded in memory
	SFBInputSourceFlagsLoadFilesInMemory		= 1 << 1,
} NS_SWIFT_NAME(InputSource.Flags);

/// An input source
NS_SWIFT_NAME(InputSource) @interface SFBInputSource : NSObject

/// Returns an initialized `SFBInputSource` object for the given URL or `nil` on failure
/// - important: Only file URLs are supported
/// - parameter url: The URL
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBInputSource` object for the specified URL, or `nil` on failure
+ (nullable instancetype)inputSourceForURL:(NSURL *)url error:(NSError **)error;

/// Returns an initialized `SFBInputSource` object for the given URL or `nil` on failure
/// - important: Only file URLs are supported
/// - parameter url: The URL
/// - parameter flags: Optional flags affecting how `url` is handled
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBInputSource` object for the specified URL, or `nil` on failure
+ (nullable instancetype)inputSourceForURL:(NSURL *)url flags:(SFBInputSourceFlags)flags error:(NSError **)error;

/// Returns an initialized `SFBInputSource` for the given `NSData` object
/// - parameter data: The desired data
/// - returns: An initialized `SFBInputSource` object
+ (instancetype)inputSourceWithData:(NSData *)data;

/// Returns an initialized `SFBInputSource` for the given byte buffer or `nil` on failure
/// - parameter bytes: A pointer to the desired byte buffer
/// - parameter length: The number of bytes in `bytes`
/// - returns: An initialized `SFBInputSource` object or `nil` on faliure
+ (nullable instancetype)inputSourceWithBytes:(const void *)bytes length:(NSInteger)length;

/// Returns an initialized `SFBInputSource` for the given byte buffer or `nil` on failure
/// - important: If `freeWhenDone` is `YES`, `bytes` must point to a buffer allocated with `malloc`
/// - parameter bytes: A pointer to the desired byte buffer
/// - parameter length: The number of bytes in `bytes`
/// - parameter freeWhenDone: If `YES` the returned object takes ownership of `bytes` and frees it on deallocation
/// - returns: An initialized `SFBInputSource` object or `nil` on faliure
+ (nullable instancetype)inputSourceWithBytesNoCopy:(void *)bytes length:(NSInteger)length freeWhenDone:(BOOL)freeWhenDone;

//+ (instancetype)new NS_UNAVAILABLE;
//- (instancetype)init NS_UNAVAILABLE;

/// Returns the URL corresponding to this input source or `nil` if none
@property (nonatomic, nullable, readonly) NSURL * url;

/// Opens the input source for reading
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/// Closes the input source
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// Returns `YES` if the input source is open
@property (nonatomic, readonly) BOOL isOpen;

/// Reads bytes from the input
/// - parameter buffer: A buffer to receive data
/// - parameter length: The maximum number of bytes to read
/// - parameter bytesRead: The number of bytes actually read
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if any bytes were read, `NO` otherwise
- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns `YES` if the end of input has been reached
@property (nonatomic, readonly) BOOL atEOF;

/// Returns the current offset in the input, in bytes
- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the length of the input, in bytes
- (BOOL)getLength:(NSInteger *)length error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns `YES` if the input is seekable
@property (nonatomic, readonly) BOOL supportsSeeking;

/// Seeks to the specified byte offset
/// - parameter offset: The desired offset, in bytes
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error;

@end

#pragma mark - Typed and Byte-Ordered Reading

/// Signed integer reading
@interface SFBInputSource (SFBSignedIntegerReading)
/// Reads an 8-bit signed integer from the input
/// - parameter i8: A pointer to an `int8_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readInt8:(int8_t *)i8 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads a 16-bit signed integer from the input
/// - parameter i16: A pointer to an `int16_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readInt16:(int16_t *)i16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads an 32-bit signed integer from the input
/// - parameter i32: A pointer to an `int32_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readInt32:(int32_t *)i32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads an 64-bit signed integer from the input
/// - parameter i64: A pointer to an `int64_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readInt64:(int64_t *)i64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

/// Unsigned integer reading
@interface SFBInputSource (SFBUnsignedIntegerReading)
/// Reads an 8-bit unsigned integer from the input
/// - parameter ui8: A pointer to an `uint8_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt8:(uint8_t *)ui8 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads a 16-bit unsigned integer from the input
/// - parameter ui16: A pointer to an `uint16_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt16:(uint16_t *)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads an 32-bit unsigned integer from the input
/// - parameter ui32: A pointer to an `uint32_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt32:(uint32_t *)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads an 64-bit unsigned integer from the input
/// - parameter ui64: A pointer to an `uint64_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt64:(uint64_t *)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

/// Big-endian unsigned integer reading
@interface SFBInputSource (SFBBigEndianReading)
/// Reads a 16-bit unsigned integer from the input in big-endian format
/// - parameter ui16: A pointer to an `uint16_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt16BigEndian:(uint16_t *)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads an 32-bit unsigned integer from the input in big-endian format
/// - parameter ui32: A pointer to an `uint32_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt32BigEndian:(uint32_t *)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads an 64-bit unsigned integer from the input in big-endian format
/// - parameter ui64: A pointer to an `uint64_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt64BigEndian:(uint64_t *)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

/// Little-endian unsigned integer reading
@interface SFBInputSource (SFBLittleEndianReading)
/// Reads a 16-bit unsigned integer from the input in little-endian format
/// - parameter ui16: A pointer to an `uint16_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt16LittleEndian:(uint16_t *)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads an 32-bit unsigned integer from the input in little-endian format
/// - parameter ui32: A pointer to an `uint32_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt32LittleEndian:(uint32_t *)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
/// Reads an 64-bit unsigned integer from the input in little-endian format
/// - parameter ui64: A pointer to an `uint64_t` to receive the value
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)readUInt64LittleEndian:(uint64_t *)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

/// Data reading
@interface SFBInputSource (SFBDataReading)
/// Reads data from the input
/// - parameter length: The maximum number of bytes to read
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `NSData` object if any bytes were read, `nil` otherwise
- (nullable NSData *)readDataOfLength:(NSUInteger)length error:(NSError **)error NS_SWIFT_NAME(read(length:));
@end

/// Header reading
@interface SFBInputSource (SFBHeaderReading)
/// Reads data from the beginning of the input, optionally skipping a leading ID3v2 tag if present
/// - important: If the input source does not support seeking this method returns an error
/// - parameter length: The number of bytes to read
/// - parameter skipID3v2Tag: Whether to skip a leading ID3v2 tag if present
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `NSData` object containing `length` bytes following the optional leading ID3v2 tag, `nil` otherwise
- (nullable NSData *)readHeaderOfLength:(NSUInteger)length skipID3v2Tag:(BOOL)skipID3v2Tag error:(NSError **)error;
@end

#pragma mark - Error Information

/// The `NSErrorDomain` used by `SFBInputSource` and subclasses
extern NSErrorDomain const SFBInputSourceErrorDomain NS_SWIFT_NAME(InputSource.ErrorDomain);

/// Possible `NSError` error codes used by `SFBInputSource`
typedef NS_ERROR_ENUM(SFBInputSourceErrorDomain, SFBInputSourceErrorCode) {
	/// File not found
	SFBInputSourceErrorCodeFileNotFound		= 0,
	/// Input/output error
	SFBInputSourceErrorCodeInputOutput		= 1,
	/// Input not seekable
	SFBInputSourceErrorCodeNotSeekable		= 2,
} NS_SWIFT_NAME(InputSource.ErrorCode);

NS_ASSUME_NONNULL_END
