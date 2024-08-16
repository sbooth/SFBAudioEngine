//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// MARK: - Matching

@interface NSData (SFBMatchMethods)
/// Returns `YES` if self starts with `pattern`
/// - parameter pattern: The search pattern
/// - returns: `YES` if `self` starts with `pattern`, `NO` otherwise
- (BOOL)startsWith:(NSData *)pattern;

/// Returns `YES` if self starts with `bytes`
/// - parameter bytes: The search pattern
/// - parameter length: The number of bytes in `bytes`
/// - returns: `YES` if `self` starts with `pattern`, `NO` otherwise
- (BOOL)startsWithBytes:(const void *)bytes length:(NSUInteger)length;


/// Returns `YES` if self contains `pattern` at `location`
/// - parameter pattern: The search pattern
/// - parameter location: The location in `self` to compare
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)matches:(NSData *)pattern atLocation:(NSUInteger)location;

/// Returns `YES` if self contains `bytes` at `location`
/// - parameter bytes: The search pattern
/// - parameter length: The number of bytes in `bytes`
/// - parameter location: The location in `self` to compare
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)matchesBytes:(const void *)bytes length:(NSUInteger)length atLocation:(NSUInteger)location;
@end

// MARK: - Searching

@interface NSData (SFBSearchMethods)
/// Returns `YES` if self contains `pattern`
/// - parameter pattern: The search pattern
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)contains:(NSData *)pattern;

/// Returns `YES` if self contains `pattern` at or after `location`
/// - parameter pattern: The search pattern
/// - parameter location: The location in `self` to begin the search
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)contains:(NSData *)pattern searchingFromLocation:(NSUInteger)location;

/// Returns `YES` if self contains `bytes`
/// - parameter bytes: The search pattern
/// - parameter length: The number of bytes in `bytes`
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)containsBytes:(const void *)bytes length:(NSUInteger)length;

/// Returns `YES` if self contains `bytes` at or after `location`
/// - parameter bytes: The search pattern
/// - parameter length: The number of bytes in `bytes`
/// - parameter location: The location in `self` to begin the search
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)containsBytes:(const void *)bytes length:(NSUInteger)length searchingFromLocation:(NSUInteger)location;


/// Searches for and returns the starting location of `pattern`
/// - parameter pattern: The search pattern
/// - returns: The starting location of `pattern` or `NSNotFound` if not found
- (NSUInteger)find:(NSData *)pattern;

/// Searches for and returns the starting offset of `pattern` at or after `location`
/// - parameter pattern: The search pattern
/// - parameter location: The location in `self` to begin the search
/// - returns: The starting offset of `pattern` relative to `location` or `NSNotFound` if not found
- (NSUInteger)find:(NSData *)pattern searchingFromLocation:(NSUInteger)location;

/// Searches for and returns the starting offset of `bytes`
/// - parameter bytes: The search pattern
/// - parameter length: The number of bytes in `bytes`
/// - returns: The starting offset of `bytes` or `NSNotFound` if not found
- (NSUInteger)findBytes:(const void *)bytes length:(NSUInteger)length;

/// Searches for and returns the starting offset of `bytes` at or after `location`
/// - parameter bytes: The search pattern
/// - parameter length: The number of bytes in `bytes`
/// - parameter location: The location in `self` to begin the search
/// - returns: The starting offset of `bytes` relative to `location` or `NSNotFound` if not found
- (NSUInteger)findBytes:(const void *)bytes length:(NSUInteger)length searchingFromLocation:(NSUInteger)location;
@end

// MARK: - ID3v2

@interface NSData (SFBID3v2Methods)
/// Returns `YES` if `self` starts with an ID3v2 tag header
- (BOOL)startsWithID3v2Header;
@end

NS_ASSUME_NONNULL_END
