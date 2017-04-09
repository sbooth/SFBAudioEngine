/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <CoreFoundation/CoreFoundation.h>
#if !TARGET_OS_IPHONE
# include <CoreServices/CoreServices.h>
#endif
#include <iomanip>

#include "Logger.h"
#include "CFWrapper.h"

#define BUFFER_LENGTH 512

int SFB::Logger::currentLogLevel = err;

namespace {

	/*! @brief Get the string representation of an \c AudioChannelLayoutTag */
	const char * GetChannelLayoutTagName(AudioChannelLayoutTag layoutTag)
	{
		switch(layoutTag) {
			case kAudioChannelLayoutTag_Mono:					return "kAudioChannelLayoutTag_Mono";
			case kAudioChannelLayoutTag_Stereo:					return "kAudioChannelLayoutTag_Stereo";
			case kAudioChannelLayoutTag_StereoHeadphones:		return "kAudioChannelLayoutTag_StereoHeadphones";
			case kAudioChannelLayoutTag_MatrixStereo:			return "kAudioChannelLayoutTag_MatrixStereo";
			case kAudioChannelLayoutTag_MidSide:				return "kAudioChannelLayoutTag_MidSide";
			case kAudioChannelLayoutTag_XY:						return "kAudioChannelLayoutTag_XY";
			case kAudioChannelLayoutTag_Binaural:				return "kAudioChannelLayoutTag_Binaural";
			case kAudioChannelLayoutTag_Ambisonic_B_Format:		return "kAudioChannelLayoutTag_Ambisonic_B_Format";
			case kAudioChannelLayoutTag_Quadraphonic:			return "kAudioChannelLayoutTag_Quadraphonic";
			case kAudioChannelLayoutTag_Pentagonal:				return "kAudioChannelLayoutTag_Pentagonal";
			case kAudioChannelLayoutTag_Hexagonal:				return "kAudioChannelLayoutTag_Hexagonal";
			case kAudioChannelLayoutTag_Octagonal:				return "kAudioChannelLayoutTag_Octagonal";
			case kAudioChannelLayoutTag_Cube:					return "kAudioChannelLayoutTag_Cube";
			case kAudioChannelLayoutTag_MPEG_3_0_A:				return "kAudioChannelLayoutTag_MPEG_3_0_A";
			case kAudioChannelLayoutTag_MPEG_3_0_B:				return "kAudioChannelLayoutTag_MPEG_3_0_B";
			case kAudioChannelLayoutTag_MPEG_4_0_A:				return "kAudioChannelLayoutTag_MPEG_4_0_A";
			case kAudioChannelLayoutTag_MPEG_4_0_B:				return "kAudioChannelLayoutTag_MPEG_4_0_B";
			case kAudioChannelLayoutTag_MPEG_5_0_A:				return "kAudioChannelLayoutTag_MPEG_5_0_A";
			case kAudioChannelLayoutTag_MPEG_5_0_B:				return "kAudioChannelLayoutTag_MPEG_5_0_B";
			case kAudioChannelLayoutTag_MPEG_5_0_C:				return "kAudioChannelLayoutTag_MPEG_5_0_C";
			case kAudioChannelLayoutTag_MPEG_5_0_D:				return "kAudioChannelLayoutTag_MPEG_5_0_D";
			case kAudioChannelLayoutTag_MPEG_5_1_A:				return "kAudioChannelLayoutTag_MPEG_5_1_A";
			case kAudioChannelLayoutTag_MPEG_5_1_B:				return "kAudioChannelLayoutTag_MPEG_5_1_B";
			case kAudioChannelLayoutTag_MPEG_5_1_C:				return "kAudioChannelLayoutTag_MPEG_5_1_C";
			case kAudioChannelLayoutTag_MPEG_5_1_D:				return "kAudioChannelLayoutTag_MPEG_5_1_D";
			case kAudioChannelLayoutTag_MPEG_6_1_A:				return "kAudioChannelLayoutTag_MPEG_6_1_A";
			case kAudioChannelLayoutTag_MPEG_7_1_A:				return "kAudioChannelLayoutTag_MPEG_7_1_A";
			case kAudioChannelLayoutTag_MPEG_7_1_B:				return "kAudioChannelLayoutTag_MPEG_7_1_B";
			case kAudioChannelLayoutTag_MPEG_7_1_C:				return "kAudioChannelLayoutTag_MPEG_7_1_C";
			case kAudioChannelLayoutTag_Emagic_Default_7_1:		return "kAudioChannelLayoutTag_Emagic_Default_7_1";
			case kAudioChannelLayoutTag_SMPTE_DTV:				return "kAudioChannelLayoutTag_SMPTE_DTV";
			case kAudioChannelLayoutTag_ITU_2_1:				return "kAudioChannelLayoutTag_ITU_2_1";
			case kAudioChannelLayoutTag_ITU_2_2:				return "kAudioChannelLayoutTag_ITU_2_2";
			case kAudioChannelLayoutTag_DVD_4:					return "kAudioChannelLayoutTag_DVD_4";
			case kAudioChannelLayoutTag_DVD_5:					return "kAudioChannelLayoutTag_DVD_5";
			case kAudioChannelLayoutTag_DVD_6:					return "kAudioChannelLayoutTag_DVD_6";
			case kAudioChannelLayoutTag_DVD_10:					return "kAudioChannelLayoutTag_DVD_10";
			case kAudioChannelLayoutTag_DVD_11:					return "kAudioChannelLayoutTag_DVD_11";
			case kAudioChannelLayoutTag_DVD_18:					return "kAudioChannelLayoutTag_DVD_18";
			case kAudioChannelLayoutTag_AudioUnit_6_0:			return "kAudioChannelLayoutTag_AudioUnit_6_0";
			case kAudioChannelLayoutTag_AudioUnit_7_0:			return "kAudioChannelLayoutTag_AudioUnit_7_0";
			case kAudioChannelLayoutTag_AudioUnit_7_0_Front:	return "kAudioChannelLayoutTag_AudioUnit_7_0_Front";
			case kAudioChannelLayoutTag_AAC_6_0:				return "kAudioChannelLayoutTag_AAC_6_0";
			case kAudioChannelLayoutTag_AAC_6_1:				return "kAudioChannelLayoutTag_AAC_6_1";
			case kAudioChannelLayoutTag_AAC_7_0:				return "kAudioChannelLayoutTag_AAC_7_0";
			case kAudioChannelLayoutTag_AAC_Octagonal:			return "kAudioChannelLayoutTag_AAC_Octagonal";
			case kAudioChannelLayoutTag_TMH_10_2_std:			return "kAudioChannelLayoutTag_TMH_10_2_std";
			case kAudioChannelLayoutTag_TMH_10_2_full:			return "kAudioChannelLayoutTag_TMH_10_2_full";
			case kAudioChannelLayoutTag_AC3_1_0_1:				return "kAudioChannelLayoutTag_AC3_1_0_1";
			case kAudioChannelLayoutTag_AC3_3_0:				return "kAudioChannelLayoutTag_AC3_3_0";
			case kAudioChannelLayoutTag_AC3_3_1:				return "kAudioChannelLayoutTag_AC3_3_1";
			case kAudioChannelLayoutTag_AC3_3_0_1:				return "kAudioChannelLayoutTag_AC3_3_0_1";
			case kAudioChannelLayoutTag_AC3_2_1_1:				return "kAudioChannelLayoutTag_AC3_2_1_1";
			case kAudioChannelLayoutTag_AC3_3_1_1:				return "kAudioChannelLayoutTag_AC3_3_1_1";
			case kAudioChannelLayoutTag_DiscreteInOrder:		return "kAudioChannelLayoutTag_DiscreteInOrder";
			case kAudioChannelLayoutTag_Unknown:				return "kAudioChannelLayoutTag_Unknown";

			default:											return nullptr;
		}
	}

	/*! @brief Get the string representation of an \c AudioChannelLabel */
	const char * GetChannelLabelName(AudioChannelLabel label)
	{
		switch(label) {
			case kAudioChannelLabel_Unknown:					return "kAudioChannelLabel_Unknown";
			case kAudioChannelLabel_Unused:						return "kAudioChannelLabel_Unused";
			case kAudioChannelLabel_UseCoordinates:				return "kAudioChannelLabel_UseCoordinates";
			case kAudioChannelLabel_Left:						return "kAudioChannelLabel_Left";
			case kAudioChannelLabel_Right:						return "kAudioChannelLabel_Right";
			case kAudioChannelLabel_Center:						return "kAudioChannelLabel_Center";
			case kAudioChannelLabel_LFEScreen:					return "kAudioChannelLabel_LFEScreen";
			case kAudioChannelLabel_LeftSurround:				return "kAudioChannelLabel_LeftSurround";
			case kAudioChannelLabel_RightSurround:				return "kAudioChannelLabel_RightSurround";
			case kAudioChannelLabel_LeftCenter:					return "kAudioChannelLabel_LeftCenter";
			case kAudioChannelLabel_RightCenter:				return "kAudioChannelLabel_RightCenter";
			case kAudioChannelLabel_CenterSurround:				return "kAudioChannelLabel_CenterSurround";
			case kAudioChannelLabel_LeftSurroundDirect:			return "kAudioChannelLabel_LeftSurroundDirect";
			case kAudioChannelLabel_RightSurroundDirect:		return "kAudioChannelLabel_RightSurroundDirect";
			case kAudioChannelLabel_TopCenterSurround:			return "kAudioChannelLabel_TopCenterSurround";
			case kAudioChannelLabel_VerticalHeightLeft:			return "kAudioChannelLabel_VerticalHeightLeft";
			case kAudioChannelLabel_VerticalHeightCenter:		return "kAudioChannelLabel_VerticalHeightCenter";
			case kAudioChannelLabel_VerticalHeightRight:		return "kAudioChannelLabel_VerticalHeightRight";
			case kAudioChannelLabel_TopBackLeft:				return "kAudioChannelLabel_TopBackLeft";
			case kAudioChannelLabel_TopBackCenter:				return "kAudioChannelLabel_TopBackCenter";
			case kAudioChannelLabel_TopBackRight:				return "kAudioChannelLabel_TopBackRight";
			case kAudioChannelLabel_RearSurroundLeft:			return "kAudioChannelLabel_RearSurroundLeft";
			case kAudioChannelLabel_RearSurroundRight:			return "kAudioChannelLabel_RearSurroundRight";
			case kAudioChannelLabel_LeftWide:					return "kAudioChannelLabel_LeftWide";
			case kAudioChannelLabel_RightWide:					return "kAudioChannelLabel_RightWide";
			case kAudioChannelLabel_LFE2:						return "kAudioChannelLabel_LFE2";
			case kAudioChannelLabel_LeftTotal:					return "kAudioChannelLabel_LeftTotal";
			case kAudioChannelLabel_RightTotal:					return "kAudioChannelLabel_RightTotal";
			case kAudioChannelLabel_HearingImpaired:			return "kAudioChannelLabel_HearingImpaired";
			case kAudioChannelLabel_Narration:					return "kAudioChannelLabel_Narration";
			case kAudioChannelLabel_Mono:						return "kAudioChannelLabel_Mono";
			case kAudioChannelLabel_DialogCentricMix:			return "kAudioChannelLabel_DialogCentricMix";
			case kAudioChannelLabel_CenterSurroundDirect:		return "kAudioChannelLabel_CenterSurroundDirect";
			case kAudioChannelLabel_Haptic:						return "kAudioChannelLabel_Haptic";
			case kAudioChannelLabel_Ambisonic_W:				return "kAudioChannelLabel_Ambisonic_W";
			case kAudioChannelLabel_Ambisonic_X:				return "kAudioChannelLabel_Ambisonic_X";
			case kAudioChannelLabel_Ambisonic_Y:				return "kAudioChannelLabel_Ambisonic_Y";
			case kAudioChannelLabel_Ambisonic_Z:				return "kAudioChannelLabel_Ambisonic_Z";
			case kAudioChannelLabel_MS_Mid:						return "kAudioChannelLabel_MS_Mid";
			case kAudioChannelLabel_MS_Side:					return "kAudioChannelLabel_MS_Side";
			case kAudioChannelLabel_XY_X:						return "kAudioChannelLabel_XY_X";
			case kAudioChannelLabel_XY_Y:						return "kAudioChannelLabel_XY_Y";
			case kAudioChannelLabel_HeadphonesLeft:				return "kAudioChannelLabel_HeadphonesLeft";
			case kAudioChannelLabel_HeadphonesRight:			return "kAudioChannelLabel_HeadphonesRight";
			case kAudioChannelLabel_ClickTrack:					return "kAudioChannelLabel_ClickTrack";
			case kAudioChannelLabel_ForeignLanguage:			return "kAudioChannelLabel_ForeignLanguage";
			case kAudioChannelLabel_Discrete:					return "kAudioChannelLabel_Discrete";
			case kAudioChannelLabel_Discrete_0:					return "kAudioChannelLabel_Discrete_0";
			case kAudioChannelLabel_Discrete_1:					return "kAudioChannelLabel_Discrete_1";
			case kAudioChannelLabel_Discrete_2:					return "kAudioChannelLabel_Discrete_2";
			case kAudioChannelLabel_Discrete_3:					return "kAudioChannelLabel_Discrete_3";
			case kAudioChannelLabel_Discrete_4:					return "kAudioChannelLabel_Discrete_4";
			case kAudioChannelLabel_Discrete_5:					return "kAudioChannelLabel_Discrete_5";
			case kAudioChannelLabel_Discrete_6:					return "kAudioChannelLabel_Discrete_6";
			case kAudioChannelLabel_Discrete_7:					return "kAudioChannelLabel_Discrete_7";
			case kAudioChannelLabel_Discrete_8:					return "kAudioChannelLabel_Discrete_8";
			case kAudioChannelLabel_Discrete_9:					return "kAudioChannelLabel_Discrete_9";
			case kAudioChannelLabel_Discrete_10:				return "kAudioChannelLabel_Discrete_10";
			case kAudioChannelLabel_Discrete_11:				return "kAudioChannelLabel_Discrete_11";
			case kAudioChannelLabel_Discrete_12:				return "kAudioChannelLabel_Discrete_12";
			case kAudioChannelLabel_Discrete_13:				return "kAudioChannelLabel_Discrete_13";
			case kAudioChannelLabel_Discrete_14:				return "kAudioChannelLabel_Discrete_14";
			case kAudioChannelLabel_Discrete_15:				return "kAudioChannelLabel_Discrete_15";
			case kAudioChannelLabel_Discrete_65535:				return "kAudioChannelLabel_Discrete_65535";

			default:											return nullptr;
		}
	}

}

void SFB::Logger::Log(levels level, const char *facility, const char *message, const char *function, const char *file, int line)
{
	if(currentLogLevel < level)
		return;

	aslmsg msg = asl_new(ASL_TYPE_MSG);

	if(facility)
		asl_set(msg, ASL_KEY_FACILITY, facility);

	if(function)
		asl_set(msg, "Function", function);

	if(file)
		asl_set(msg, "File", file);

	if(-1 != line) {
		char buf [32];
		if(snprintf(buf, sizeof(buf), "%d", line))
			asl_set(msg, "Line", buf);
	}

	asl_log(nullptr, msg, level, "%s", message);

	asl_free(msg);
}

std::ostream& operator<<(std::ostream& out, CFStringRef s)
{
	if(nullptr == s) {
		out << "(null)";
		return out;
	}

	char buf [BUFFER_LENGTH];

	CFIndex totalCharacters = CFStringGetLength(s);
	CFIndex currentCharacter = 0;
	CFIndex charactersConverted = 0;
	CFIndex bytesWritten;

	while(currentCharacter < totalCharacters) {
		charactersConverted = CFStringGetBytes(s, CFRangeMake(currentCharacter, totalCharacters), kCFStringEncodingUTF8, 0, false, (UInt8 *)buf, BUFFER_LENGTH, &bytesWritten);
		currentCharacter += charactersConverted;
		out.write(buf, bytesWritten);
	};

	return out;
}

std::ostream& operator<<(std::ostream& out, CFNumberRef n)
{
	if(nullptr == n)
		out << "(null)";
	else if(n == kCFNumberPositiveInfinity)
		out << "+Inf";
	else if(n == kCFNumberNegativeInfinity)
		out << "-Inf";
	else if(n == kCFNumberNaN)
		out << "NaN";
	else if(CFNumberIsFloatType(n)) {
		double val;
		if(CFNumberGetValue(n, kCFNumberDoubleType, &val))
			out << val;
	}
	else {
		long long val;
		if(CFNumberGetValue(n, kCFNumberLongLongType, &val))
			out << val;
	}

	return out;
}

std::ostream& operator<<(std::ostream& out, CFURLRef u)
{
	if(nullptr == u) {
		out << "(null)";
		return out;
	}

	CFStringRef s = CFURLGetString(u);
#if !TARGET_OS_IPHONE
	if(CFStringHasPrefix(s, CFSTR("file:"))) {
		CFStringRef displayName = nullptr;
		OSStatus result = LSCopyDisplayNameForURL(u, &displayName);

		if(noErr == result && nullptr != displayName) {
			out << displayName;
			CFRelease(displayName), displayName = nullptr;
		}
	}
	else
#endif
		out << s;

	return out;
}

std::ostream& operator<<(std::ostream& out, CFErrorRef e)
{
	if(nullptr == e) {
		out << "(null)";
		return out;
	}

	SFB::CFString r(CFErrorCopyDescription(e));
	if(r)
		out << r;

	return out;
}

std::ostream& operator<<(std::ostream& out, CFUUIDRef u)
{
	if(nullptr == u) {
		out << "(null)";
		return out;
	}

	SFB::CFString r(CFUUIDCreateString(kCFAllocatorDefault, u));
	if(r)
		out << r;

	return out;
}

std::ostream& operator<<(std::ostream& out, CFUUIDBytes b)
{
	SFB::CFUUID u(CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, b));
	if(u)
		out << u;

	return out;
}

// Most of this is stolen from Apple's CAStreamBasicDescription::Print()
std::ostream& operator<<(std::ostream& out, const AudioStreamBasicDescription& format)
{
	unsigned char formatID [5];
	*(UInt32 *)formatID = OSSwapHostToBigInt32(format.mFormatID);
	formatID[4] = '\0';

	// General description
	out << format.mChannelsPerFrame << " ch, " << format.mSampleRate << " Hz, '" << formatID << "' (0x" << std::hex << std::setw(8) << std::setfill('0') << format.mFormatFlags << std::dec << ") ";

	if(kAudioFormatLinearPCM == format.mFormatID) {
		// Bit depth
		UInt32 fractionalBits = ((0x3f << 7)/*kLinearPCMFormatFlagsSampleFractionMask*/ & format.mFormatFlags) >> 7/*kLinearPCMFormatFlagsSampleFractionShift*/;
		if(0 < fractionalBits)
			out << (format.mBitsPerChannel - fractionalBits) << "." << fractionalBits;
		else
			out << format.mBitsPerChannel;

		out << "-bit";

		// Endianness
		bool isInterleaved = !(kAudioFormatFlagIsNonInterleaved & format.mFormatFlags);
		UInt32 interleavedChannelCount = (isInterleaved ? format.mChannelsPerFrame : 1);
		UInt32 sampleSize = (0 < format.mBytesPerFrame && 0 < interleavedChannelCount ? format.mBytesPerFrame / interleavedChannelCount : 0);
		if(1 < sampleSize)
			out << ((kLinearPCMFormatFlagIsBigEndian & format.mFormatFlags) ? " big-endian" : " little-endian");

		// Sign
		bool isInteger = !(kLinearPCMFormatFlagIsFloat & format.mFormatFlags);
		if(isInteger)
			out << ((kLinearPCMFormatFlagIsSignedInteger & format.mFormatFlags) ? " signed" : " unsigned");

		// Integer or floating
		out << (isInteger ? " integer" : " float");

		// Packedness
		if(0 < sampleSize && ((sampleSize << 3) != format.mBitsPerChannel))
			out << ((kLinearPCMFormatFlagIsPacked & format.mFormatFlags) ? ", packed in " : ", unpacked in ") << sampleSize << " bytes";

		// Alignment
		if((0 < sampleSize && ((sampleSize << 3) != format.mBitsPerChannel)) || (0 != (format.mBitsPerChannel & 7)))
			out << ((kLinearPCMFormatFlagIsAlignedHigh & format.mFormatFlags) ? " high-aligned" : " low-aligned");

		if(!isInterleaved)
			out << ", deinterleaved";
	}
	else if(kAudioFormatAppleLossless == format.mFormatID) {
		UInt32 sourceBitDepth = 0;
		switch(format.mFormatFlags) {
			case kAppleLosslessFormatFlag_16BitSourceData:		sourceBitDepth = 16;	break;
    		case kAppleLosslessFormatFlag_20BitSourceData:		sourceBitDepth = 20;	break;
    		case kAppleLosslessFormatFlag_24BitSourceData:		sourceBitDepth = 24;	break;
    		case kAppleLosslessFormatFlag_32BitSourceData:		sourceBitDepth = 32;	break;
		}

		if(0 != sourceBitDepth)
			out << "from " << sourceBitDepth << "-bit source, ";
		else
			out << "from UNKNOWN source bit depth, ";

		out << format.mFramesPerPacket << " frames/packet";
	}
	else
		out << format.mBitsPerChannel << " bits/channel, " << format.mBytesPerPacket << " bytes/packet, " << format.mFramesPerPacket << " frames/packet, " << format.mBytesPerFrame << " bytes/frame";

	return out;
}

std::ostream& operator<<(std::ostream& out, const AudioChannelLayout *layout)
{
	if(nullptr == layout) {
		out << "(null)";
		return out;
	}

	if(kAudioChannelLayoutTag_UseChannelBitmap == layout->mChannelLayoutTag)
		out << "Channel bitmap: 0x" << std::hex << std::setw(8) << std::setfill('0') << layout->mChannelBitmap << std::dec;
	else if(kAudioChannelLayoutTag_UseChannelDescriptions == layout->mChannelLayoutTag){
		out << layout->mNumberChannelDescriptions << " channel descriptions: " << std::endl;

		const AudioChannelDescription *desc = layout->mChannelDescriptions;
		for(UInt32 i = 0; i < layout->mNumberChannelDescriptions; ++i, ++desc) {
			if(kAudioChannelLabel_UseCoordinates == desc->mChannelLabel)
				out << "\t" << i << ". Coordinates = (" << desc->mCoordinates[0] << ", " << desc->mCoordinates[1] << ", " << desc->mCoordinates[2] << "), flags = 0x" << std::hex << std::setw(8) << std::setfill('0') << desc->mChannelFlags << std::dec;
			else
				out << "\t" << i << ". Label = " << GetChannelLabelName(desc->mChannelLabel) << " (0x" << std::hex << std::setw(8) << std::setfill('0') << desc->mChannelLabel << std::dec << ")";
			if(i < layout->mNumberChannelDescriptions - 1)
				out << std::endl;
		}
	}
	else
		out << GetChannelLayoutTagName(layout->mChannelLayoutTag) << " (0x" << std::hex << std::setw(8) << std::setfill('0') << layout->mChannelLayoutTag << std::dec << ")";


	return out;
}
