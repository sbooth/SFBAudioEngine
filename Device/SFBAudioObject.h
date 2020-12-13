/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

NS_ASSUME_NONNULL_BEGIN

/// Possible scopes for \c SFBAudioObject properties
/// @note These are interchangeable with \c AudioObjectPropertyScope
typedef NS_ENUM(AudioObjectPropertyScope, SFBAudioObjectPropertyScope) {
	/// Global scope
	SFBAudioObjectPropertyScopeGlobal         = kAudioObjectPropertyScopeGlobal,
	/// Input scope
	SFBAudioObjectPropertyScopeInput          = kAudioObjectPropertyScopeInput,
	/// Output scope
	SFBAudioObjectPropertyScopeOutput         = kAudioObjectPropertyScopeOutput,
	/// Playthrough scope
	SFBAudioObjectPropertyScopePlayThrough    = kAudioObjectPropertyScopePlayThrough
} /*NS_SWIFT_NAME(AudioObjectPropertyScope)*/;

/// An audio object
NS_SWIFT_NAME(AudioObject) @interface SFBAudioObject : NSObject

/// The system audio object
+ (SFBAudioObject *)systemObject;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioObject object with the specified audio object ID
/// @note This returns a specialized subclass of \c SFBAudioObject when possible
/// @param objectID The desired audio object ID
/// @return An initialized \c SFBAudioObject object or \c nil if \c objectID is invalid or unknown
- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)objectID NS_DESIGNATED_INITIALIZER;

/// Returns the audio object's ID
@property (nonatomic, readonly) AudioObjectID objectID;

#pragma mark - Audio Object Properties

/// Returns \c YES if the underlying audio object has the specified property
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property;
/// Returns \c YES if the underlying audio object has the specified property in a scope
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope;
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Returns \c YES if the underlying audio object has the specified property and it is settable
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property;
/// Returns \c YES if the underlying audio object has the specified property in a scope and it is settable
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope;
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope and it is settable
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @return The property value
- (nullable NSString *)stringForProperty:(AudioObjectPropertySelector)property;
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSString *)stringForProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope;
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSString *)stringForProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(AudioObjectPropertySelector)property;
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope;
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(AudioObjectPropertySelector)property;
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope;
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(AudioObjectPropertySelector)property;
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope;
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Performs a block when the specified property changes
/// @note This observes \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenPropertyChanges:(AudioObjectPropertySelector)property performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the specified property in a scope changes
/// @note This observes \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param scope The desired scope
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(AudioObjectPropertySelector)property changesinScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(AudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(AudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block;

@end

@interface SFBAudioObject (SFBAudioObjectProperties)

/// Returns the audio object's base class or \c 0 on error
@property (nonatomic, readonly) AudioClassID baseClassID;
/// Returns the audio object's class or \c 0 on error
@property (nonatomic, readonly) AudioClassID classID;
/// Returns the audio object's owning object
@property (nonatomic, readonly) SFBAudioObject * owner;
/// Returns the audio object's name
@property (nonatomic, nullable, readonly) NSString *name;
/// Returns the audio object's model name
@property (nonatomic, nullable, readonly) NSString *modelName;
/// Returns the audio object's manufacturer
@property (nonatomic, nullable, readonly) NSString *manufacturer;

/// Returns the name of the specified element
- (NSString *)nameOfElement:(AudioObjectPropertyElement)element NS_SWIFT_NAME(nameOfElement(_:));
/// Returns the name of the specified element in the specified scope
- (NSString *)nameOfElement:(AudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(nameOfElement(_:scope:));
/// Returns the category name of the specified element
- (NSString *)categoryNameOfElement:(AudioObjectPropertyElement)element NS_SWIFT_NAME(categoryNameOfElement(_:));
/// Returns the category name of the specifiec element in the specified scope
- (NSString *)categoryNameOfElement:(AudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(categoryNameOfElement(_:scope:));
/// Returns the number name of the specifiec element
- (NSString *)numberNameOfElement:(AudioObjectPropertyElement)element NS_SWIFT_NAME(numberNameOfElement(_:));
/// Returns the number name of the specifiec element in the specified scope
- (NSString *)numberNameOfElement:(AudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(numberNameOfElement(_:scope:));

/// Returns the audio objects owned by this object
@property (nonatomic, nullable, readonly) NSArray<SFBAudioObject *> *ownedObjects;

/// Returns the audio object's serial number
@property (nonatomic, nullable, readonly) NSString *serialNumber;
/// Returns the audio object's firmware version
@property (nonatomic, nullable, readonly) NSString *firmwareVersion;

@end

NS_ASSUME_NONNULL_END
