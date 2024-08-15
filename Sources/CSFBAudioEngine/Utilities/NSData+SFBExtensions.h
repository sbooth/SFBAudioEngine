//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// MARK: - Numeric Values

@interface NSData (SFBNumericValueMethods)
/// Reads an unsigned 32-bit integer value
/// - parameter location: The location in `self` of the value's first byte
/// - returns: An unsigned 32-bit integer value
- (uint32_t)uint32AtLocation:(NSUInteger)location;

/// Reads an unsigned 32-bit big-endian integer value
/// - parameter location: The location in `self` of the value's first byte
/// - returns: An unsigned 32-bit big-endian integer value
- (uint32_t)uint32BigEndianAtLocation:(NSUInteger)location;

/// Reads an unsigned 32-bit little-endian integer value
/// - parameter location: The location in `self` of the value's first byte
/// - returns: An unsigned 32-bit little-endian integer value
- (uint32_t)uint32LittleEndianAtLocation:(NSUInteger)location;
@end

// MARK: - Matching

@interface NSData (SFBMatchMethods)
/// Returns `YES` if self starts with `pattern`
/// - parameter pattern: The search pattern
/// - returns: `YES` if `self` starts with `pattern`, `NO` otherwise
- (BOOL)startsWith:(NSData *)pattern;
/// Returns `YES` if self starts with `patternBytes`
/// - parameter patternBytes: The search pattern
/// - parameter patternLength: The number of bytes in `patternBytes`
/// - returns: `YES` if `self` starts with `pattern`, `NO` otherwise
- (BOOL)startsWithBytes:(const void *)patternBytes length:(NSUInteger)patternLength;

/// Returns `YES` if self contains `pattern` at `location`
/// - parameter pattern: The search pattern
/// - parameter location: The location in `self` to compare
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)contains:(NSData *)pattern atLocation:(NSUInteger)location;
/// Returns `YES` if self contains `patternBytes` at `location`
/// - parameter patternBytes: The search pattern
/// - parameter patternLength: The number of bytes in `patternBytes`
/// - parameter location: The location in `self` to compare
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)containsBytes:(const void *)patternBytes length:(NSUInteger)patternLength atLocation:(NSUInteger)location;
@end

// MARK: - Searching

@interface NSData (SFBSearchMethods)
/// Returns `YES` if self contains `pattern`
/// - parameter pattern: The search pattern
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)contains:(NSData *)pattern;

/// Returns `YES` if self contains `pattern` at or after `startingLocation`
/// - parameter pattern: The search pattern
/// - parameter startingLocation: The location in `self` to begin the search
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)contains:(NSData *)pattern searchingFromLocation:(NSUInteger)startingLocation;

/// Returns `YES` if self contains `patternBytes` at or after `startingLocation`
/// - parameter patternBytes: The search pattern
/// - parameter patternLength: The number of bytes in `patternBytes`
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)containsBytes:(const void *)patternBytes length:(NSUInteger)patternLength;

/// Returns `YES` if self contains `patternBytes` at or after `startingLocation`
/// - parameter patternBytes: The search pattern
/// - parameter patternLength: The number of bytes in `patternBytes`
/// - parameter startingLocation: The location in `self` to begin the search
/// - returns: `YES` if `self` contains `pattern`, `NO` otherwise
- (BOOL)containsBytes:(const void *)patternBytes length:(NSUInteger)patternLength searchingFromLocation:(NSUInteger)startingLocation;


/// Searches for and returns the starting location of `pattern`
/// - parameter pattern: The search pattern
/// - returns: The starting location of `pattern` or `NSNotFound` if not found
- (NSUInteger)find:(NSData *)pattern;

/// Searches for and returns the starting offset of `pattern` at or after `startingLocation`
/// - parameter pattern: The search pattern
/// - parameter startingLocation: The location in `self` to begin the search
/// - returns: The starting offset of `pattern` relative to `startingLocation` or `NSNotFound` if not found
- (NSUInteger)find:(NSData *)pattern startingLocation:(NSUInteger)startingLocation;

/// Searches for and returns the starting offset of `patternBytes` at or after `startingLocation`
/// - parameter patternBytes: The search pattern
/// - parameter patternLength: The number of bytes in `patternBytes`
/// - returns: The starting offset of `patternBytes` or `NSNotFound` if not found
- (NSUInteger)findBytes:(const void *)patternBytes length:(NSUInteger)patternLength;

/// Searches for and returns the starting offset of `patternBytes` at or after `startingLocation`
/// - parameter patternBytes: The search pattern
/// - parameter patternLength: The number of bytes in `patternBytes`
/// - parameter startingLocation: The location in `self` to begin the search
/// - returns: The starting offset of `patternBytes` relative to `startingLocation` or `NSNotFound` if not found
- (NSUInteger)findBytes:(const void *)patternBytes length:(NSUInteger)patternLength startingLocation:(NSUInteger)startingLocation;
@end

// MARK: - ID3v2

@interface NSData (SFBID3v2Methods)
/// Returns `YES` if `self` starts with an ID3v2 tag header
- (BOOL)startsWithID3v2Header;
@end

NS_ASSUME_NONNULL_END
