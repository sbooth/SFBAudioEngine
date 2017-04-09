/*
 * Copyright (c) 2012 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

/*! @file CFErrorUtilities.h @brief Utility functions simplifying the boilerplate creation of CFErrors */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*!
	 * @brief Create a \c CFErrorRef
	 * @note The returned error must be released by the caller
	 * @param domain The \c CFErrorRef error domain
	 * @param code The \c CFErrorRef error code
	 * @param description The value for userinfo dictionary \c kCFErrorLocalizedDescriptionKey
	 * @param failureReason The value for userinfo dictionary \c kCFErrorLocalizedFailureReasonKey
	 * @param recoverySuggestion The value for userinfo dictionary \c kCFErrorLocalizedRecoverySuggestionKey
	 * @return A \c CFErrorRef or \c nullptr on failure
	 */
	CFErrorRef CreateError(CFStringRef domain, CFIndex code, CFStringRef description, CFStringRef failureReason, CFStringRef recoverySuggestion);

	/*!
	 * @brief Create a \c CFErrorRef
	 * @note The returned error must be released by the caller
	 * @param domain The \c CFErrorRef error domain
	 * @param code The \c CFErrorRef error code
	 * @param descriptionFormatStringForURL The value for userinfo dictionary \c kCFErrorLocalizedDescriptionKey.  The display name of \c url will be substituted for the first occurrence of \c %@ in this string.
	 * @param url The URL
	 * @param failureReason The value for userinfo dictionary \c kCFErrorLocalizedFailureReasonKey
	 * @param recoverySuggestion The value for userinfo dictionary \c kCFErrorLocalizedRecoverySuggestionKey
	 * @return A \c CFErrorRef or \c nullptr on failure
	 */
	CFErrorRef CreateErrorForURL(CFStringRef domain, CFIndex code, CFStringRef descriptionFormatStringForURL, CFURLRef url, CFStringRef failureReason, CFStringRef recoverySuggestion);

}
