/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// An output source
NS_SWIFT_NAME(OutputSource) @interface SFBOutputSource : NSObject

/// Returns an initialized \c SFBOutputSource object for the given URL or \c nil on failure
/// @param url The URL
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBOutputSource object for the specified URL, or \c nil on failure
+ (nullable instancetype)outputSourceForURL:(NSURL *)url error:(NSError **)error;

/// Returns an initialized \c SFBOutputSource for the given \c NSMutableData object or \c nil on failure
/// @param data An \c NSMutableData object to receive output
/// @return An initialized \c SFBOutputSource object or \c nil on faliure
+ (nullable instancetype)outputSourceWithMutableData:(NSMutableData *)data error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns an initialized \c SFBOutputSource for the given buffer or \c nil on failure
/// @param buffer A buffer to receive output
/// @param capacity The capacity of \c buffer in bytes
/// @return An initialized \c SFBOutputSource object or \c nil on faliure
+ (nullable instancetype)outputSourceWithBuffer:(void *)buffer capacity:(NSInteger)capacity;

//+ (instancetype)new NS_UNAVAILABLE;
//- (instancetype)init NS_UNAVAILABLE;

/// Returns the URL corresponding to this output source or \c nil if none
@property (nonatomic, nullable, readonly) NSURL * url;

/// Opens the output source for writing
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/// Closes the output source
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// Returns \c YES if the output source is open
@property (nonatomic, readonly) BOOL isOpen;

/// Reads bytes from the input
/// @param buffer A buffer to receive data
/// @param length The maximum number of bytes to read
/// @param bytesRead The number of bytes actually read
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if any bytes were read, \c NO otherwise
- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Writes bytes to the output
/// @param buffer A buffer of data to write
/// @param length The maximum number of bytes to write
/// @param bytesWritten The number of bytes actually written
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if any bytes were written, \c NO otherwise
- (BOOL)writeBytes:(const void *)buffer length:(NSInteger)length bytesWritten:(NSInteger *)bytesWritten error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns \c YES if the end of input has been reached
@property (nonatomic, readonly) BOOL atEOF;

/// Returns the current offset in the output, in bytes
- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the length of the output, in bytes
- (BOOL)getLength:(NSInteger *)length error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns \c YES if the output is seekable
@property (nonatomic, readonly) BOOL supportsSeeking;

/// Seeks to the specified byte offset
/// @param offset The desired offset, in bytes
/// @return \c YES on success, \c NO otherwise
- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error;

@end

@interface SFBOutputSource (SFBDataWriting)
- (BOOL)writeData:(NSData *)data error:(NSError **)error;
@end

#pragma mark - Typed and Byte-Ordered Writing

@interface SFBOutputSource (SFBSignedIntegerWriting)
- (BOOL)writeInt8:(int8_t)i8 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeInt16:(int16_t)i16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeInt32:(int32_t)i32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeInt64:(int64_t)i64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

@interface SFBOutputSource (SFBUnsignedIntegerWriting)
- (BOOL)writeUInt8:(uint8_t)ui8 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeUInt16:(uint16_t)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeUInt32:(uint32_t)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeUInt64:(uint64_t)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

@interface SFBOutputSource (SFBBigEndianWriting)
- (BOOL)writeUInt16BigEndian:(uint16_t)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeUInt32BigEndian:(uint32_t)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeUInt64BigEndian:(uint64_t)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

@interface SFBOutputSource (SFBLittleEndianWriting)
- (BOOL)writeUInt16LittleEndian:(uint16_t)ui16 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeUInt32LittleEndian:(uint32_t)ui32 error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (BOOL)writeUInt64LittleEndian:(uint64_t)ui64 error:(NSError **)error NS_REFINED_FOR_SWIFT;
@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBOutputSource and subclasses
extern NSErrorDomain const SFBOutputSourceErrorDomain NS_SWIFT_NAME(OutputSource.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBOutputSource
typedef NS_ERROR_ENUM(SFBOutputSourceErrorDomain, SFBOutputSourceErrorCode) {
	/// File not found
	SFBOutputSourceErrorCodeFileNotFound	= 0,
	/// Input/output error
	SFBOutputSourceErrorCodeInputOutput		= 1
} NS_SWIFT_NAME(OutputSource.ErrorCode);

NS_ASSUME_NONNULL_END
