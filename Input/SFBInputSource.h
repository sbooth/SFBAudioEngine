/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*! @brief The \c NSErrorDomain used by \c SFBInputSource and subclasses */
extern NSErrorDomain const SFBInputSourceErrorDomain NS_SWIFT_NAME(InputSourceErrorDomain);

/*! @brief Possible \c NSError  error codes used by \c SFBInputSource */
typedef NS_ERROR_ENUM(SFBInputSourceErrorDomain, SFBInputSourceErrorCode) {
	SFBInputSourceErrorCodeFileNotFound		= 0,		/*!< File not found */
	SFBInputSourceErrorCodeInputOutput		= 1			/*!< Input/output error */
};

/*! @brief Metadata kind bitmask values used in `copyMetadataFrom:` and `removeMetadataOfKind:` */
typedef NS_OPTIONS(NSUInteger, SFBInputSourceFlags) {
	SFBInputSourceFlagsMemoryMapFiles			= 1 << 0,	/*!< Files should be mapped in memory using \c mmap() */
	SFBInputSourceFlagsLoadFilesInMemory		= 1 << 1	/*!< Files should be fully loaded in memory */
};

//! An input source
NS_SWIFT_NAME(InputSource) @interface SFBInputSource : NSObject

/*!
 * Returns an intiailized  \c SFBInputSource object for the given URL or \c nil on failure
 * @param url The URL
 * @param flags Optional flags affecting how \c url is handled
 * @param error An optional pointer to a \c NSError to receive error information
 * @return An initialized \c SFBInputSource object for the specified URL, or \c nil on failure
 */
+ (nullable instancetype)inputSourceForURL:(NSURL *)url flags:(SFBInputSourceFlags)flags error:(NSError **)error;

/*!
 * Returns an intiailized  \c SFBInputSource for the given byte buffer or \c nil on failure
 * @param bytes A pointer to the desired byte buffer
 * @param length The number of bytes in \c bytes
 * @return An initialized \c SFBInputSource object  or \c nil on faliure
 * @see SFBInputSourceFlags
 */
+ (nullable instancetype)inputSourceWithBytes:(const void *)bytes length:(NSInteger)length error:(NSError **)error;

/*!
 * Returns an intiailized  \c SFBInputSource for the given byte buffer or \c nil on failure
 * @param bytes A pointer to the desired byte buffer
 * @param length The number of bytes in \c bytes
 * @return An initialized \c SFBInputSource object  or \c nil on faliure
 * @see SFBInputSourceFlags
 */
+ (nullable instancetype)inputSourceWithBytesNoCopy:(void *)bytes length:(NSInteger)length error:(NSError **)error;

//+ (instancetype)new NS_UNAVAILABLE;
//- (instancetype)init NS_UNAVAILABLE;

/*! @brief Returns the URL corresponding to this input source or \c nil if none */
@property (nonatomic, nullable, readonly) NSURL * url;

/*!
 * @brief Opens the input source for reading
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/*!
 * Closes the input source
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/*! @brief Returns \c YES if the input source is open */
@property (nonatomic, readonly) BOOL isOpen;

/*!
 * @brief Reads bytes from the input
 * @param buffer A buffer to receive data
 * @param length The maximum number of bytes to read
 * @param bytesRead The number of bytes actually read
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES if any bytes were read, \c NO otherwise
 */
- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error;

/*! @brief Returns \c YES if the end of input has been reached */
@property (nonatomic, readonly) BOOL atEOF;

/*! @brief Returns  the current offset in the input, in bytes */
- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error NS_SWIFT_NAME(offset());

/*! @brief Returns the length of the input, in bytes */
- (BOOL)getLength:(NSInteger *)length error:(NSError **)error NS_SWIFT_NAME(length());

/*! @brief Returns \c YES if the input is seekable */
@property (nonatomic, readonly) BOOL supportsSeeking;

/*!
 * @brief Seeks to the specified byte offset
 * @param offset The desired offset, in bytes
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error;

@end

@interface SFBInputSource (SFBSignedIntegerReading)
- (BOOL)readInt8:(int8_t *)i8 error:(NSError **)error;
- (BOOL)readInt16:(int16_t *)i16 error:(NSError **)error;
- (BOOL)readInt32:(int32_t *)i32 error:(NSError **)error;
- (BOOL)readInt64:(int64_t *)i64 error:(NSError **)error;
@end

@interface SFBInputSource (SFBUnsignedIntegerReading)
- (BOOL)readUInt8:(uint8_t *)ui8 error:(NSError **)error;
- (BOOL)readUInt16:(uint16_t *)ui16 error:(NSError **)error;
- (BOOL)readUInt32:(uint32_t *)ui32 error:(NSError **)error;
- (BOOL)readUInt64:(uint64_t *)ui64 error:(NSError **)error;
@end

@interface SFBInputSource (SFBBigEndianReading)
- (BOOL)readUInt16BigEndian:(uint16_t *)ui16 error:(NSError **)error;
- (BOOL)readUInt32BigEndian:(uint32_t *)ui32 error:(NSError **)error;
- (BOOL)readUInt64BigEndian:(uint64_t *)ui64 error:(NSError **)error;
@end

@interface SFBInputSource (SFBLittleEndianReading)
- (BOOL)readUInt16LittleEndian:(uint16_t *)ui16 error:(NSError **)error;
- (BOOL)readUInt32LittleEndian:(uint32_t *)ui32 error:(NSError **)error;
- (BOOL)readUInt64LittleEndian:(uint64_t *)ui64 error:(NSError **)error;
@end

NS_ASSUME_NONNULL_END
