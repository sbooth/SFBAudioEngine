/*
 * Copyright (c) 2006 - 2016 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "CoreAudioOutput.h"
#include "AudioPlayer.h"
#include "AudioFormat.h"
#include "Logger.h"
#include "CreateStringForOSType.h"

namespace {

	// ========================================
	// AUGraph input callback
	OSStatus myAURenderCallback(void							*inRefCon,
								AudioUnitRenderActionFlags		*ioActionFlags,
								const AudioTimeStamp			*inTimeStamp,
								UInt32							inBusNumber,
								UInt32							inNumberFrames,
								AudioBufferList					*ioData)
	{
		assert(nullptr != inRefCon);

		auto output = static_cast<SFB::Audio::CoreAudioOutput *>(inRefCon);
		return output->Render(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
	}

}

SFB::Audio::CoreAudioOutput::CoreAudioOutput()
	: mAUGraph(nullptr), mMixerNode(-1), mOutputNode(-1), mDefaultMaximumFramesPerSlice(0)
{}

SFB::Audio::CoreAudioOutput::~CoreAudioOutput()
{}

#pragma mark Player Parameters

bool SFB::Audio::CoreAudioOutput::GetVolume(Float32& volume) const
{
	return GetVolumeForChannel(0, volume);
}

bool SFB::Audio::CoreAudioOutput::SetVolume(Float32 volume)
{
	return SetVolumeForChannel(0, volume);
}

bool SFB::Audio::CoreAudioOutput::GetVolumeForChannel(UInt32 channel, Float32& volume) const
{
	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitGetParameter(au, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, &volume);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetVolumeForChannel(UInt32 channel, Float32 volume)
{
	if(0 > volume || 1 < volume)
		return false;

	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetParameter(au, kHALOutputParam_Volume, kAudioUnitScope_Global, channel, volume, 0);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetParameter (kHALOutputParam_Volume, kAudioUnitScope_Global, " << channel << ") failed: " << result);
		return false;
	}

	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Volume for channel " << channel << " set to " << volume);

	return true;
}

bool SFB::Audio::CoreAudioOutput::GetPreGain(Float32& preGain) const
{
	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitGetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, &preGain);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetPreGain(Float32 preGain)
{
	if(0 > preGain || 1 < preGain)
		return false;

	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, preGain, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Pregain set to " << preGain);

	return true;
}

bool SFB::Audio::CoreAudioOutput::IsPerformingSampleRateConversion() const
{
	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	Float64 sampleRate;
	UInt32 dataSize = sizeof(sampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Global, 0, &sampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate) failed: " << result);
		return false;
	}

	return (sampleRate != mFormat.mSampleRate);
}

bool SFB::Audio::CoreAudioOutput::GetSampleRateConverterComplexity(UInt32& complexity) const
{
	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	UInt32 dataSize = sizeof(complexity);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRateConverterComplexity, kAudioUnitScope_Global, 0, &complexity, &dataSize);
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_SampleRateConverterComplexity) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetSampleRateConverterComplexity(UInt32 complexity)
{
	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Setting sample rate converter quality to " << complexity);

	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	result = AudioUnitSetProperty(au, kAudioUnitProperty_SampleRateConverterComplexity, kAudioUnitScope_Global, 0, &complexity, (UInt32)sizeof(complexity));
	if(noErr != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (kAudioUnitProperty_SampleRateConverterComplexity) failed: " << result);
		return false;
	}

	return true;
}

#pragma mark DSP Effects

bool SFB::Audio::CoreAudioOutput::AddEffect(OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit)
{
    return AddEffect(kAudioUnitType_Effect, subType, manufacturer, flags, mask, effectUnit);
}

bool SFB::Audio::CoreAudioOutput::AddEffect(OSType componentType, OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit1)
{
	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Adding DSP: '" << SFB::StringForOSType(componentType) << "''" << SFB::StringForOSType(subType) << "' '" << SFB::StringForOSType(manufacturer) << "'");

	// Get the source node for the graph's output node
	UInt32 numInteractions = 0;
	auto result = AUGraphCountNodeInteractions(mAUGraph, mOutputNode, &numInteractions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphCountNodeInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction interactions [numInteractions];

	result = AUGraphGetNodeInteractions(mAUGraph, mOutputNode, &numInteractions, interactions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetNodeInteractions failed: " << result);
		return false;
	}

	AUNode sourceNode = -1;
	for(UInt32 interactionIndex = 0; interactionIndex < numInteractions; ++interactionIndex) {
		AUNodeInteraction interaction = interactions[interactionIndex];

		if(kAUNodeInteraction_Connection == interaction.nodeInteractionType && mOutputNode == interaction.nodeInteraction.connection.destNode) {
			sourceNode = interaction.nodeInteraction.connection.sourceNode;
			break;
		}
	}

	// Unable to determine the preceding node, so bail
	if(-1 == sourceNode) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "Unable to determine input node");
		return false;
	}

	// Create the effect node and set its format
	AudioComponentDescription componentDescription = {
		.componentType = componentType,
		.componentSubType = subType,
		.componentManufacturer = manufacturer,
		.componentFlags = flags,
		.componentFlagsMask = mask
	};

	AUNode effectNode = -1;
	result = AUGraphAddNode(mAUGraph, &componentDescription, &effectNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphAddNode failed: " << result);
		return false;
	}

	AudioUnit effectUnit = nullptr;
	result = AUGraphNodeInfo(mAUGraph, effectNode, nullptr, &effectUnit);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphRemoveNode failed: " << result);

		return false;
	}

#if TARGET_OS_IPHONE
	// All AudioUnits on iOS except RemoteIO require kAudioUnitProperty_MaximumFramesPerSlice to be 4096
	// See http://developer.apple.com/library/ios/#documentation/AudioUnit/Reference/AudioUnitPropertiesReference/Reference/reference.html#//apple_ref/c/econst/kAudioUnitProperty_MaximumFramesPerSlice
	UInt32 framesPerSlice = 4096;
	result = AudioUnitSetProperty(effectUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &framesPerSlice, (UInt32)sizeof(framesPerSlice));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphRemoveNode failed: " << result);

		return false;
	}
#endif

//	result = AudioUnitSetProperty(effectUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &mFormat, sizeof(mFormat));
//	if(noErr != result) {
////		ERR("AudioUnitSetProperty(kAudioUnitProperty_StreamFormat) failed: %i", result);
//
//		// If the property couldn't be set (the AU may not support this format), remove the new node
//		result = AUGraphRemoveNode(mAUGraph, effectNode);
//		if(noErr != result)
//			;//			ERR("AUGraphRemoveNode failed: %i", result);
//
//		return false;
//	}
//
//	result = AudioUnitSetProperty(effectUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &mFormat, sizeof(mFormat));
//	if(noErr != result) {
////		ERR("AudioUnitSetProperty(kAudioUnitProperty_StreamFormat) failed: %i", result);
//
//		// If the property couldn't be set (the AU may not support this format), remove the new node
//		result = AUGraphRemoveNode(mAUGraph, effectNode);
//		if(noErr != result)
//			;			//ERR("AUGraphRemoveNode failed: %i", result);
//
//		return false;
//	}

	// Insert the effect at the end of the graph, before the output node
	result = AUGraphDisconnectNodeInput(mAUGraph, mOutputNode, 0);

	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphDisconnectNodeInput failed: " << result);

		result = AUGraphRemoveNode(mAUGraph, effectNode);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphRemoveNode failed: " << result);

		return false;
	}

	// Reconnect the nodes
	result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, effectNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphConnectNodeInput(mAUGraph, effectNode, 0, mOutputNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphUpdate(mAUGraph, nullptr);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphUpdate failed: " << result);

		// If the update failed, restore the previous node state
		result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, mOutputNode, 0);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphConnectNodeInput failed: " << result);
			return false;
		}
	}

	if(nullptr != effectUnit1)
		*effectUnit1 = effectUnit;

	return true;
}

bool SFB::Audio::CoreAudioOutput::RemoveEffect(AudioUnit effectUnit)
{
	if(nullptr == effectUnit)
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Removing DSP effect: " << effectUnit);

	UInt32 nodeCount = 0;
	auto result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	AUNode effectNode = -1;
	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = -1;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		// This is the unit to remove
		if(effectUnit == au) {
			effectNode = node;
			break;
		}
	}

	if(-1 == effectNode) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "Unable to find the AUNode for the specified AudioUnit");
		return false;
	}

	// Get the current input and output nodes for the node to delete
	UInt32 numInteractions = 0;
	result = AUGraphCountNodeInteractions(mAUGraph, effectNode, &numInteractions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphCountNodeInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction interactions [numInteractions];

	result = AUGraphGetNodeInteractions(mAUGraph, effectNode, &numInteractions, interactions);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetNodeInteractions failed: " << result);

		return false;
	}

	AUNode sourceNode = -1, destNode = -1;
	for(UInt32 interactionIndex = 0; interactionIndex < numInteractions; ++interactionIndex) {
		AUNodeInteraction interaction = interactions[interactionIndex];

		if(kAUNodeInteraction_Connection == interaction.nodeInteractionType) {
			if(effectNode == interaction.nodeInteraction.connection.destNode)
				sourceNode = interaction.nodeInteraction.connection.sourceNode;
			else if(effectNode == interaction.nodeInteraction.connection.sourceNode)
				destNode = interaction.nodeInteraction.connection.destNode;
		}
	}

	if(-1 == sourceNode || -1 == destNode) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "Unable to find the source or destination nodes");
		return false;
	}

	result = AUGraphDisconnectNodeInput(mAUGraph, effectNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphDisconnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphDisconnectNodeInput(mAUGraph, destNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphDisconnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphRemoveNode(mAUGraph, effectNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphRemoveNode failed: " << result);
		return false;
	}

	// Reconnect the nodes
	result = AUGraphConnectNodeInput(mAUGraph, sourceNode, 0, destNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphConnectNodeInput failed: " << result);
		return false;
	}

	result = AUGraphUpdate(mAUGraph, nullptr);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphUpdate failed: " << result);
		return false;
	}

	return true;
}

#if !TARGET_OS_IPHONE

#pragma mark Hog Mode

bool SFB::Audio::CoreAudioOutput::DeviceIsHogged() const
{
	// Is it hogged by us?
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	auto result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	return (hogPID == getpid() ? true : false);
}

bool SFB::Audio::CoreAudioOutput::StartHoggingDevice()
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Taking hog mode for device 0x" << std::hex << deviceID);

	// Is it hogged already?
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	auto result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// The device is already hogged
	if(hogPID != (pid_t)-1) {
		LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Device is already hogged by pid: " << hogPID);
		return false;
	}

	bool restartIO = _IsRunning();
	if(restartIO)
		_Stop();

	hogPID = getpid();

	result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(hogPID), &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// If IO was enabled before, re-enable it
	if(restartIO && !_IsRunning())
		_Start();

	return true;
}

bool SFB::Audio::CoreAudioOutput::StopHoggingDevice()
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Releasing hog mode for device 0x" << std::hex << deviceID);

	// Is it hogged by us?
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	auto result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	// If we don't own hog mode we can't release it
	if(hogPID != getpid())
		return false;

	bool restartIO = _IsRunning();
	if(restartIO)
		_Stop();

	// Release hog mode.
	hogPID = (pid_t)-1;

	result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(hogPID), &hogPID);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: " << result);
		return false;
	}

	if(restartIO && !_IsRunning())
		_Start();

	return true;
}

#pragma mark Device Parameters

bool SFB::Audio::CoreAudioOutput::GetDeviceMasterVolume(Float32& volume) const
{
	return GetDeviceVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool SFB::Audio::CoreAudioOutput::SetDeviceMasterVolume(Float32 volume)
{
	return SetDeviceVolumeForChannel(kAudioObjectPropertyElementMaster, volume);
}

bool SFB::Audio::CoreAudioOutput::GetDeviceVolumeForChannel(UInt32 channel, Float32& volume) const
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalar,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= channel
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	UInt32 dataSize = sizeof(volume);
	auto result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &volume);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetDeviceVolumeForChannel(UInt32 channel, Float32 volume)
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Setting output device 0x" << std::hex << deviceID << " channel " << channel << " volume to " << volume);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalar,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= channel
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, " << channel << ") is false");
		return false;
	}

	auto result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(volume), &volume);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, " << channel << ") failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::GetDeviceChannelCount(UInt32& channelCount) const
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyStreamConfiguration,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectHasProperty (kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput) is false");
		return false;
	}

	UInt32 dataSize;
	auto result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, nullptr, &dataSize);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput) failed: " << result);
		return false;
	}

	AudioBufferList *bufferList = (AudioBufferList *)malloc(dataSize);

	if(nullptr == bufferList) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "Unable to allocate << " << dataSize << " bytes");
		return false;
	}

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, bufferList);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput) failed: " << result);
		free(bufferList), bufferList = nullptr;
		return false;
	}

	channelCount = 0;
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		channelCount += bufferList->mBuffers[bufferIndex].mNumberChannels;

	free(bufferList), bufferList = nullptr;
	return true;
}

bool SFB::Audio::CoreAudioOutput::GetDevicePreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyPreferredChannelsForStereo,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelsForStereo, kAudioObjectPropertyScopeOutput) failed is false");
		return false;
	}

	UInt32 preferredChannels [2];
	UInt32 dataSize = sizeof(preferredChannels);
	auto result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &preferredChannels);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo, kAudioObjectPropertyScopeOutput) failed: " << result);
		return false;
	}

	preferredStereoChannels.first = preferredChannels[0];
	preferredStereoChannels.second = preferredChannels[1];

	return true;
}

bool SFB::Audio::CoreAudioOutput::GetDeviceAvailableNominalSampleRates(std::vector<AudioValueRange>& nominalSampleRates) const
{
	nominalSampleRates.clear();

	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyAvailableNominalSampleRates,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	if(!AudioObjectHasProperty(deviceID, &propertyAddress)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectHasProperty (kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeOutput) failed is false");
		return false;
	}

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, nullptr, &dataSize);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeOutput) failed is false");
		return false;
	}

	size_t numberNominalSampleRates = dataSize / sizeof(AudioValueRange);
	nominalSampleRates.resize(numberNominalSampleRates);

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &nominalSampleRates[0]);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeOutput) failed is false");
		return false;
	}

	return true;
}

#pragma mark Device Management

bool SFB::Audio::CoreAudioOutput::GetDeviceID(AudioDeviceID& deviceID) const
{
	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	UInt32 dataSize = sizeof(deviceID);

	result = AudioUnitGetProperty(au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceID, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetDeviceID(AudioDeviceID deviceID)
{
	if(kAudioDeviceUnknown == deviceID)
		return false;

	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	// Update our output AU to use the specified device
	result = AudioUnitSetProperty(au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceID, (UInt32)sizeof(deviceID));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (kAudioOutputUnitProperty_CurrentDevice) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::GetAvailableDataSources(std::vector<UInt32>& dataSources) const
{
	dataSources.clear();

	AudioDeviceID deviceID = kAudioDeviceUnknown;
	if(!GetDeviceID(deviceID) || kAudioDeviceUnknown == deviceID)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSources,
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(deviceID,
													 &propertyAddress,
													 0,
													 nullptr,
													 &dataSize);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyDataSources) failed: " << result);
		return false;
	}

	auto dataSourceCount = dataSize / sizeof(UInt32);
	dataSources.resize(dataSourceCount);

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &dataSources[0]);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyDataSources) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::GetActiveDataSources(std::vector<UInt32>& dataSources) const
{
	dataSources.clear();

	AudioDeviceID deviceID = kAudioDeviceUnknown;
	if(!GetDeviceID(deviceID) || kAudioDeviceUnknown == deviceID)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSource,
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(deviceID,
													 &propertyAddress,
													 0,
													 nullptr,
													 &dataSize);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyDataSources) failed: " << result);
		return false;
	}

	auto dataSourceCount = dataSize / sizeof(UInt32);
	dataSources.resize(dataSourceCount);

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &dataSources[0]);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyDataSources) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetActiveDataSources(const std::vector<UInt32>& dataSources)
{
	if(dataSources.empty())
		return false;

	AudioDeviceID deviceID = kAudioDeviceUnknown;
	if(!GetDeviceID(deviceID) || kAudioDeviceUnknown == deviceID)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSource,
		.mScope		= kAudioDevicePropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, (UInt32)(dataSources.size() * sizeof(UInt32)), &dataSources[0]);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectSetPropertyData (kAudioDevicePropertyDataSource) failed: " << result);
		return false;
	}

	return true;
}

#pragma mark Stream Management

bool SFB::Audio::CoreAudioOutput::GetOutputStreams(std::vector<AudioStreamID>& streams) const
{
	streams.clear();

	AudioDeviceID deviceID = kAudioDeviceUnknown;
	if(!GetDeviceID(deviceID) || kAudioDeviceUnknown == deviceID)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyStreams,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize;
	OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, nullptr, &dataSize);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreams) failed: " << result);
		return false;
	}

	auto streamCount = dataSize / sizeof(AudioStreamID);
	streams.resize(streamCount);

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &streams[0]);
	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyStreams) failed: " << result);
		return false;
	}

	return true;
}

//bool SFB::Audio::CoreAudioOutput::GetOutputStreamVirtualFormat(AudioStreamID streamID, AudioStreamBasicDescription& virtualFormat) const
//{
//	std::vector<AudioStreamID> streams;
//	if(!GetOutputStreams(streams))
//		return false;
//
//	if(std::end(streams) == std::find(std::begin(streams), std::end(streams), streamID)) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "Unknown AudioStreamID: " << std::hex << streamID);
//		return false;
//	}
//
//	AudioObjectPropertyAddress propertyAddress = {
//		.mSelector	= kAudioStreamPropertyVirtualFormat,
//		.mScope		= kAudioObjectPropertyScopeGlobal,
//		.mElement	= kAudioObjectPropertyElementMaster
//	};
//
//	UInt32 dataSize = sizeof(virtualFormat);
//
//	OSStatus result = AudioObjectGetPropertyData(streamID,
//												 &propertyAddress,
//												 0,
//												 nullptr,
//												 &dataSize,
//												 &virtualFormat);
//
//	if(kAudioHardwareNoError != result) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: " << result);
//		return false;
//	}
//
//	return true;
//}
//
//bool SFB::Audio::CoreAudioOutput::SetOutputStreamVirtualFormat(AudioStreamID streamID, const AudioStreamBasicDescription& virtualFormat)
//{
//	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Setting stream 0x" << std::hex << streamID << " virtual format to: " << virtualFormat);
//
//	std::vector<AudioStreamID> streams;
//	if(!GetOutputStreams(streams))
//		return false;
//
//	if(std::end(streams) == std::find(std::begin(streams), std::end(streams), streamID)) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "Unknown AudioStreamID: " << std::hex << streamID);
//		return false;
//	}
//
//	AudioObjectPropertyAddress propertyAddress = {
//		.mSelector	= kAudioStreamPropertyVirtualFormat,
//		.mScope		= kAudioObjectPropertyScopeGlobal,
//		.mElement	= kAudioObjectPropertyElementMaster
//	};
//
//	OSStatus result = AudioObjectSetPropertyData(streamID,
//												 &propertyAddress,
//												 0,
//												 nullptr,
//												 sizeof(virtualFormat),
//												 &virtualFormat);
//
//	if(kAudioHardwareNoError != result) {
//		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectSetPropertyData (kAudioStreamPropertyVirtualFormat) failed: " << result);
//		return false;
//	}
//
//	return true;
//}

bool SFB::Audio::CoreAudioOutput::GetOutputStreamPhysicalFormat(AudioStreamID streamID, AudioStreamBasicDescription& physicalFormat) const
{
	std::vector<AudioStreamID> streams;
	if(!GetOutputStreams(streams))
		return false;

	if(std::end(streams) == std::find(std::begin(streams), std::end(streams), streamID)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "Unknown AudioStreamID: " << std::hex << streamID);
		return false;
	}

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioStreamPropertyPhysicalFormat,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = sizeof(physicalFormat);

	OSStatus result = AudioObjectGetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 nullptr,
												 &dataSize,
												 &physicalFormat);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetOutputStreamPhysicalFormat(AudioStreamID streamID, const AudioStreamBasicDescription& physicalFormat)
{
	LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Setting stream 0x" << std::hex << streamID << " physical format to: " << physicalFormat);

	std::vector<AudioStreamID> streams;
	if(!GetOutputStreams(streams))
		return false;

	if(std::end(streams) == std::find(std::begin(streams), std::end(streams), streamID)) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "Unknown AudioStreamID: " << std::hex << streamID);
		return false;
	}

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioStreamPropertyPhysicalFormat,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(streamID,
												 &propertyAddress,
												 0,
												 nullptr,
												 sizeof(physicalFormat),
												 &physicalFormat);

	if(kAudioHardwareNoError != result) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectSetPropertyData (kAudioStreamPropertyPhysicalFormat) failed: " << result);
		return false;
	}

	return true;
}

#endif

#pragma mark Device Management

bool SFB::Audio::CoreAudioOutput::_GetDeviceSampleRate(Float64& sampleRate) const
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyNominalSampleRate,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = sizeof(sampleRate);
	auto result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &sampleRate);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::_SetDeviceSampleRate(Float64 sampleRate)
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	// Determine if this will actually be a change
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyNominalSampleRate,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float64 currentSampleRate;
	UInt32 dataSize = sizeof(currentSampleRate);

	auto result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &currentSampleRate);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	// Nothing to do
	if(currentSampleRate == sampleRate)
		return true;

	// Set the sample rate
	dataSize = sizeof(sampleRate);
	result = AudioObjectSetPropertyData(deviceID, &propertyAddress, 0, nullptr, sizeof(sampleRate), &sampleRate);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: " << result);
		return false;
	}

	return true;
}

size_t SFB::Audio::CoreAudioOutput::_GetPreferredBufferSize() const
{
	AudioUnit au = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return 0;
	}

	UInt32 maxFramesPerSlice = 0;
	UInt32 dataSize = sizeof(maxFramesPerSlice);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFramesPerSlice, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);
		return 0;
	}

	return maxFramesPerSlice;
}

#pragma mark -

bool SFB::Audio::CoreAudioOutput::_Open()
{
	auto result = NewAUGraph(&mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "NewAUGraph failed: " << result);
		return false;
	}

	// The graph will look like:
	// MultiChannelMixer -> Output
	AudioComponentDescription desc;

	// Set up the mixer node
	desc.componentType			= kAudioUnitType_Mixer;
	desc.componentSubType		= kAudioUnitSubType_MultiChannelMixer;
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlags			= kAudioComponentFlag_SandboxSafe;
	desc.componentFlagsMask		= 0;

	result = AUGraphAddNode(mAUGraph, &desc, &mMixerNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphAddNode failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	// Set up the output node
	desc.componentType			= kAudioUnitType_Output;
#if TARGET_OS_IPHONE
	desc.componentSubType		= kAudioUnitSubType_RemoteIO;
	desc.componentFlags			= 0;
#else
	desc.componentSubType		= kAudioUnitSubType_HALOutput;
	desc.componentFlags			= kAudioComponentFlag_SandboxSafe;
#endif
	desc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	desc.componentFlagsMask		= 0;

	result = AUGraphAddNode(mAUGraph, &desc, &mOutputNode);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphAddNode failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	result = AUGraphConnectNodeInput(mAUGraph, mMixerNode, 0, mOutputNode, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphConnectNodeInput failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	// Install the input callback
	AURenderCallbackStruct cbs = { myAURenderCallback, this };
	result = AUGraphSetNodeInputCallback(mAUGraph, mMixerNode, 0, &cbs);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphSetNodeInputCallback failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	// Open the graph
	result = AUGraphOpen(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphOpen failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	// Set the mixer's volume on the input and output
	AudioUnit au = nullptr;
	result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, 1.f, 0);
	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Input) failed: " << result);

	result = AudioUnitSetParameter(au, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, 1.f, 0);
	if(noErr != result)
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetParameter (kMultiChannelMixerParam_Volume, kAudioUnitScope_Output) failed: " << result);

#if TARGET_OS_IPHONE
	// All AudioUnits on iOS except RemoteIO require kAudioUnitProperty_MaximumFramesPerSlice to be 4096
	// See http://developer.apple.com/library/ios/#documentation/AudioUnit/Reference/AudioUnitPropertiesReference/Reference/reference.html#//apple_ref/c/econst/kAudioUnitProperty_MaximumFramesPerSlice
	result = AUGraphNodeInfo(mAUGraph, mMixerNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	UInt32 framesPerSlice = 4096;
	result = AudioUnitSetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &framesPerSlice, (UInt32)sizeof(framesPerSlice));
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
#else
	// Save the default value of kAudioUnitProperty_MaximumFramesPerSlice for use when performing sample rate conversion
	result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	UInt32 dataSize = sizeof(mDefaultMaximumFramesPerSlice);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &mDefaultMaximumFramesPerSlice, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}
#endif

	// Initialize the graph
	result = AUGraphInitialize(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphInitialize failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	// Store the graph's format
	result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	UInt32 propertySize = sizeof(mFormat);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &mFormat, &propertySize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input) failed: " << result);

		result = DisposeAUGraph(mAUGraph);
		if(noErr != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);

		mAUGraph = nullptr;
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::_Close()
{
	Boolean graphIsRunning = false;
	auto result = AUGraphIsRunning(mAUGraph, &graphIsRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphIsRunning failed: " << result);
		return false;
	}

	if(graphIsRunning) {
		result = AUGraphStop(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphStop failed: " << result);
			return false;
		}
	}

	Boolean graphIsInitialized = false;
	result = AUGraphIsInitialized(mAUGraph, &graphIsInitialized);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphIsInitialized failed: " << result);
		return false;
	}

	if(graphIsInitialized) {
		result = AUGraphUninitialize(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphUninitialize failed: " << result);
			return false;
		}
	}

	result = AUGraphClose(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphClose failed: " << result);
		return false;
	}

	result = DisposeAUGraph(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "DisposeAUGraph failed: " << result);
		return false;
	}

	mAUGraph = nullptr;
	mMixerNode = -1;
	mOutputNode = -1;

	return true;
}

bool SFB::Audio::CoreAudioOutput::_Start()
{
	auto result = AUGraphStart(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphStart failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::_Stop()
{
	auto result = AUGraphStop(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphStop failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::_RequestStop()
{
	return _Stop();
}

bool SFB::Audio::CoreAudioOutput::_IsOpen() const
{
	return nullptr != mAUGraph;
}

bool SFB::Audio::CoreAudioOutput::_IsRunning() const
{
	Boolean isRunning = false;
	auto result = AUGraphIsRunning(mAUGraph, &isRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphIsRunning failed: " << result);
		return false;
	}

	return isRunning;
}

bool SFB::Audio::CoreAudioOutput::_Reset()
{
	UInt32 nodeCount = 0;
	auto result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphIsRunning failed: " << result);
		return false;
	}

	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, i, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		result = AudioUnitReset(au, kAudioUnitScope_Global, 0);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitReset failed: " << result);
			return false;
		}
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::_SupportsFormat(const AudioFormat& format) const
{
	return format.IsPCM() || format.IsDoP();
}

bool SFB::Audio::CoreAudioOutput::_SetupForDecoder(const Decoder& decoder)
{
	const AudioFormat& decoderFormat = decoder.GetFormat();
	if(!_SupportsFormat(decoderFormat)) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "Core Audio unsupported format: " << decoderFormat);
		return false;
	}

	// ========================================
	// If the graph is running, stop it
	Boolean graphIsRunning = FALSE;
	auto result = AUGraphIsRunning(mAUGraph, &graphIsRunning);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphIsRunning failed: " << result);
		return false;
	}

	if(graphIsRunning) {
		result = AUGraphStop(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphStop failed: " << result);
			return false;
		}
	}

	// ========================================
	// If the graph is initialized, uninitialize it
	Boolean graphIsInitialized = FALSE;
	result = AUGraphIsInitialized(mAUGraph, &graphIsInitialized);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphIsInitialized failed: " << result);
		return false;
	}

	if(graphIsInitialized) {
		result = AUGraphUninitialize(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphUninitialize failed: " << result);
			return false;
		}
	}

	// ========================================
	// Save the interaction information and then clear all the connections
	UInt32 interactionCount = 0;
	result = AUGraphGetNumberOfInteractions(mAUGraph, &interactionCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetNumberOfInteractions failed: " << result);
		return false;
	}

	AUNodeInteraction interactions [interactionCount];

	for(UInt32 i = 0; i < interactionCount; ++i) {
		result = AUGraphGetInteractionInfo(mAUGraph, i, &interactions[i]);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetInteractionInfo failed: " << result);
			return false;
		}
	}

	result = AUGraphClearConnections(mAUGraph);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphClearConnections failed: " << result);
		return false;
	}

	AudioFormat format = mFormat;

	// Even if the format is DoP, treat it as PCM from the AUGraph's perspective
	format.mFormatID			= kAudioFormatLinearPCM;
	format.mChannelsPerFrame	= decoderFormat.mChannelsPerFrame;
	format.mSampleRate			= decoderFormat.mSampleRate;

	// ========================================
	// Attempt to set the new stream format
	if(!SetPropertyOnAUGraphNodes(kAudioUnitProperty_StreamFormat, &format, sizeof(format))) {
		// If the new format could not be set, restore the old format to ensure a working graph

		// DoP masquerades as PCM
		bool wasDoP = mFormat.IsDoP();
		if(wasDoP)
			mFormat.mFormatID = kAudioFormatLinearPCM;

		if(!SetPropertyOnAUGraphNodes(kAudioUnitProperty_StreamFormat, &mFormat, sizeof(mFormat))) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "Unable to restore AUGraph format: " << result);
		}

		if(wasDoP)
			mFormat.mFormatID = kAudioFormatDoP;

		// Do not free connections here, so graph can be rebuilt
	}
	else {
		// Store the correct format ID
		format.mFormatID = decoderFormat.mFormatID;
		mFormat = format;
	}

	// ========================================
	// Restore the graph's connections and input callbacks
	for(UInt32 i = 0; i < interactionCount; ++i) {
		switch(interactions[i].nodeInteractionType) {

				// Reestablish the connection
			case kAUNodeInteraction_Connection:
			{
				result = AUGraphConnectNodeInput(mAUGraph,
												 interactions[i].nodeInteraction.connection.sourceNode,
												 interactions[i].nodeInteraction.connection.sourceOutputNumber,
												 interactions[i].nodeInteraction.connection.destNode,
												 interactions[i].nodeInteraction.connection.destInputNumber);

				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphConnectNodeInput failed: " << result);
					return false;
				}

				break;
			}

				// Reestablish the input callback
			case kAUNodeInteraction_InputCallback:
			{
				result = AUGraphSetNodeInputCallback(mAUGraph,
													 interactions[i].nodeInteraction.inputCallback.destNode,
													 interactions[i].nodeInteraction.inputCallback.destInputNumber,
													 &interactions[i].nodeInteraction.inputCallback.cback);

				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphSetNodeInputCallback failed: " << result);
					return false;
				}

				break;
			}
		}
	}

#if !TARGET_OS_IPHONE
	// ========================================
	// Output units perform sample rate conversion if the input sample rate is not equal to
	// the output sample rate. For high sample rates, the sample rate conversion can require
	// more rendered frames than are available by default in kAudioUnitProperty_MaximumFramesPerSlice (512)
	// For example, 192 KHz audio converted to 44.1 HHz requires approximately (192 / 44.1) * 512 = 2229 frames
	// So if the input and output sample rates on the output device don't match, adjust
	// kAudioUnitProperty_MaximumFramesPerSlice to ensure enough audio data is passed per render cycle
	// See http://lists.apple.com/archives/coreaudio-api/2009/Oct/msg00150.html
	AudioUnit au = nullptr;
	result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &au);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	Float64 inputSampleRate = 0;
	UInt32 dataSize = sizeof(inputSampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input, 0, &inputSampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	Float64 outputSampleRate = 0;
	dataSize = sizeof(outputSampleRate);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &outputSampleRate, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_SampleRate, kAudioUnitScope_Output) failed: " << result);
		return false;
	}

	UInt32 newMaxFrames = mDefaultMaximumFramesPerSlice;

	// If the output unit's input and output sample rates don't match, calculate a working maximum number of frames per slice
	if(inputSampleRate != outputSampleRate) {
		LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Input sample rate (" << inputSampleRate << ") and output sample rate (" << outputSampleRate << ") don't match");

		Float64 ratio = inputSampleRate / outputSampleRate;
		Float64 multiplier = std::max(1.0, ratio);

		// Round up to the nearest 16 frames
		newMaxFrames = (UInt32)ceil(mDefaultMaximumFramesPerSlice * multiplier);
		newMaxFrames += 16;
		newMaxFrames &= 0xFFFFFFF0;
	}

	UInt32 currentMaxFrames = 0;
	dataSize = sizeof(currentMaxFrames);
	result = AudioUnitGetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &currentMaxFrames, &dataSize);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global) failed: " << result);
		return false;
	}

	// Adjust the maximum frames per slice if necessary
	if(newMaxFrames != currentMaxFrames) {
		LOGGER_INFO("org.sbooth.AudioEngine.Output.CoreAudio", "Adjusting kAudioUnitProperty_MaximumFramesPerSlice to " << newMaxFrames);

		if(!SetPropertyOnAUGraphNodes(kAudioUnitProperty_MaximumFramesPerSlice, &newMaxFrames, sizeof(newMaxFrames))) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "SetPropertyOnAUGraphNodes (kAudioUnitProperty_MaximumFramesPerSlice) failed");
			return false;
		}
	}
#endif

	// If the graph was initialized, reinitialize it
	if(graphIsInitialized) {
		result = AUGraphInitialize(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphInitialize failed: " << result);
			return false;
		}
	}

	// If the graph was running, restart it
	if(graphIsRunning) {
		result = AUGraphStart(mAUGraph);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphStart failed: " << result);
			return false;
		}
	}

	// Attempt to set the output audio unit's channel map
	const ChannelLayout& decoderChannelLayout = decoder.GetChannelLayout();
	if(!SetOutputUnitChannelMap(decoderChannelLayout))
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "Unable to set output unit channel map");

	// The decoder's channel layout becomes our channel layout
	mChannelLayout = decoderChannelLayout;

	return true;
}

#if !TARGET_OS_IPHONE

bool SFB::Audio::CoreAudioOutput::_CreateDeviceUID(CFStringRef& deviceUID) const
{
	AudioDeviceID deviceID;
	if(!GetDeviceID(deviceID))
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDeviceUID,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = sizeof(deviceUID);
	auto result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &deviceUID);
	if(kAudioHardwareNoError != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::_SetDeviceUID(CFStringRef deviceUID)
{
	AudioDeviceID deviceID = kAudioDeviceUnknown;

	// If nullptr was passed as the device UID, use the default output device
	if(nullptr == deviceUID) {
		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioHardwarePropertyDefaultOutputDevice,
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster
		};

		UInt32 specifierSize = sizeof(deviceID);

		auto result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &specifierSize, &deviceID);
		if(kAudioHardwareNoError != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: " << result);
			return false;
		}
	}
	else {
		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioHardwarePropertyDeviceForUID,
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster
		};

		AudioValueTranslation translation = {
			&deviceUID, sizeof(deviceUID),
			&deviceID, sizeof(deviceID)
		};

		UInt32 specifierSize = sizeof(translation);

		auto result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &specifierSize, &translation);
		if(kAudioHardwareNoError != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: " << result);
			return false;
		}
	}

	// The device isn't connected or doesn't exist
	if(kAudioDeviceUnknown == deviceID)
		return false;

	return SetDeviceID(deviceID);
}

#endif

#pragma mark AUGraph Utilities

bool SFB::Audio::CoreAudioOutput::GetAUGraphLatency(Float64& latency) const
{
	latency = 0;

	UInt32 nodeCount = 0;
	auto result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		Float64 auLatency = 0;
		UInt32 dataSize = sizeof(auLatency);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &auLatency, &dataSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_Latency, kAudioUnitScope_Global) failed: " << result);
			return false;
		}

		latency += auLatency;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::GetAUGraphTailTime(Float64& tailTime) const
{
	tailTime = 0;

	UInt32 nodeCount = 0;
	auto result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	for(UInt32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
		AUNode node = 0;
		result = AUGraphGetIndNode(mAUGraph, nodeIndex, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
			return false;
		}

		Float64 auTailTime = 0;
		UInt32 dataSize = sizeof(auTailTime);
		result = AudioUnitGetProperty(au, kAudioUnitProperty_TailTime, kAudioUnitScope_Global, 0, &auTailTime, &dataSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_TailTime, kAudioUnitScope_Global) failed: " << result);
			return false;
		}

		tailTime += auTailTime;
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize)
{
	if(nullptr == propertyData || 0 >= propertyDataSize)
		return false;

	UInt32 nodeCount = 0;
	auto result = AUGraphGetNodeCount(mAUGraph, &nodeCount);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetNodeCount failed: " << result);
		return false;
	}

	// Iterate through the nodes and attempt to set the property
	for(UInt32 i = 0; i < nodeCount; ++i) {
		AUNode node;
		result = AUGraphGetIndNode(mAUGraph, i, &node);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetIndNode failed: " << result);
			return false;
		}

		AudioUnit au = nullptr;
		result = AUGraphNodeInfo(mAUGraph, node, nullptr, &au);

		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphGetNodeCount failed: " << result);
			return false;
		}

		if(mOutputNode == node) {
			// For AUHAL as the output node, you can't set the device side, so just set the client side
			result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Input, 0, propertyData, propertyDataSize);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
				return false;
			}
		}
		else {
			UInt32 elementCount = 0;
			UInt32 dataSize = sizeof(elementCount);
			result = AudioUnitGetProperty(au, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &elementCount, &dataSize);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_ElementCount, kAudioUnitScope_Input) failed: " << result);
				return false;
			}

			for(UInt32 j = 0; j < elementCount; ++j) {
//				Boolean writable;
//				result = AudioUnitGetPropertyInfo(au, propertyID, kAudioUnitScope_Input, j, &dataSize, &writable);
//				if(noErr != result && kAudioUnitErr_InvalidProperty != result) {
//					LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetPropertyInfo (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
//					return false;
//				}
//
//				if(kAudioUnitErr_InvalidProperty == result || !writable)
//					continue;

				result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Input, j, propertyData, propertyDataSize);
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Input) failed: " << result);
					return false;
				}
			}

			elementCount = 0;
			dataSize = sizeof(elementCount);
			result = AudioUnitGetProperty(au, kAudioUnitProperty_ElementCount, kAudioUnitScope_Output, 0, &elementCount, &dataSize);
			if(noErr != result) {
				LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_ElementCount, kAudioUnitScope_Output) failed: " << result);
				return false;
			}

			for(UInt32 j = 0; j < elementCount; ++j) {
//				Boolean writable;
//				result = AudioUnitGetPropertyInfo(au, propertyID, kAudioUnitScope_Output, j, &dataSize, &writable);
//				if(noErr != result && kAudioUnitErr_InvalidProperty != result) {
//					LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetPropertyInfo (" << propertyID << ", kAudioUnitScope_Output) failed: " << result);
//					return false;
//				}
//
//				if(kAudioUnitErr_InvalidProperty == result || !writable)
//					continue;

				result = AudioUnitSetProperty(au, propertyID, kAudioUnitScope_Output, j, propertyData, propertyDataSize);
				if(noErr != result) {
					LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (" << propertyID << ", kAudioUnitScope_Output) failed: " << result);
					return false;
				}
			}
		}
	}

	return true;
}

bool SFB::Audio::CoreAudioOutput::SetOutputUnitChannelMap(const ChannelLayout& channelLayout)
{
#if !TARGET_OS_IPHONE
	AudioUnit outputUnit = nullptr;
	auto result = AUGraphNodeInfo(mAUGraph, mOutputNode, nullptr, &outputUnit);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AUGraphNodeInfo failed: " << result);
		return false;
	}

	// Clear the existing channel map
	result = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 0, nullptr, 0);
	if(noErr != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input) failed: " << result);
		return false;
	}

	if(!channelLayout)
		return true;

	// Stereo
	if(channelLayout == ChannelLayout::Stereo) {
		UInt32 preferredChannelsForStereo [2];
		UInt32 preferredChannelsForStereoSize = sizeof(preferredChannelsForStereo);
		result = AudioUnitGetProperty(outputUnit, kAudioDevicePropertyPreferredChannelsForStereo, kAudioUnitScope_Output, 0, preferredChannelsForStereo, &preferredChannelsForStereoSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioDevicePropertyPreferredChannelsForStereo) failed: " << result);
			return false;
		}

		// Build a channel map using the preferred stereo channels
		AudioStreamBasicDescription outputFormat;
		UInt32 propertySize = sizeof(outputFormat);
		result = AudioUnitGetProperty(outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outputFormat, &propertySize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output) failed: " << result);
			return false;
		}

		SInt32 channelMap [ outputFormat.mChannelsPerFrame ];
		for(UInt32 i = 0; i <  outputFormat.mChannelsPerFrame; ++i)
			channelMap[i] = -1;

		// TODO: Verify the following statement to be true
		// preferredChannelsForStereo uses 1-based indices
		channelMap[preferredChannelsForStereo[0] - 1] = 0;
		channelMap[preferredChannelsForStereo[1] - 1] = 1;

		LOGGER_DEBUG("org.sbooth.AudioEngine.Output.CoreAudio", "Using stereo channel map: ");
		for(UInt32 i = 0; i < outputFormat.mChannelsPerFrame; ++i)
			LOGGER_DEBUG("org.sbooth.AudioEngine.Output.CoreAudio", "  " << i << " -> " << channelMap[i]);

		// Set the channel map
		result = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 0, channelMap, (UInt32)sizeof(channelMap));
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input) failed: " << result);
			return false;
		}
	}
	// Multichannel or other non-stereo audio
	else {
		// Use the device's preferred channel layout
		UInt32 devicePreferredChannelLayoutSize = 0;
		result = AudioUnitGetPropertyInfo(outputUnit, kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output, 0, &devicePreferredChannelLayoutSize, nullptr);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetPropertyInfo (kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output) failed: " << result);
			return false;
		}

		AudioChannelLayout *devicePreferredChannelLayout = (AudioChannelLayout *)malloc(devicePreferredChannelLayoutSize);

		result = AudioUnitGetProperty(outputUnit, kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output, 0, devicePreferredChannelLayout, &devicePreferredChannelLayoutSize);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitGetProperty (kAudioDevicePropertyPreferredChannelLayout, kAudioUnitScope_Output) failed: " << result);

			if(devicePreferredChannelLayout)
				free(devicePreferredChannelLayout), devicePreferredChannelLayout = nullptr;

			return false;
		}

		UInt32 channelCount = 0;
		UInt32 dataSize = sizeof(channelCount);
		result = AudioFormatGetProperty(kAudioFormatProperty_NumberOfChannelsForLayout, devicePreferredChannelLayoutSize, devicePreferredChannelLayout, &dataSize, &channelCount);
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioFormatGetProperty (kAudioFormatProperty_NumberOfChannelsForLayout) failed: " << result);

			if(devicePreferredChannelLayout)
				free(devicePreferredChannelLayout), devicePreferredChannelLayout = nullptr;

			return false;
		}

		// Create the channel map
		SInt32 channelMap [ channelCount ];
		dataSize = (UInt32)sizeof(channelMap);

		const AudioChannelLayout *channelLayouts [] = {
			channelLayout,
			devicePreferredChannelLayout
		};

		result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(channelLayouts), channelLayouts, &dataSize, channelMap);

		if(devicePreferredChannelLayout)
			free(devicePreferredChannelLayout), devicePreferredChannelLayout = nullptr;

		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);
			return false;
		}

		LOGGER_DEBUG("org.sbooth.AudioEngine.Output.CoreAudio", "Using multichannel channel map: ");
		for(UInt32 i = 0; i < channelCount; ++i)
			LOGGER_DEBUG("org.sbooth.AudioEngine.Output.CoreAudio", "  " << i << " -> " << channelMap[i]);

		// Set the channel map
		result = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 0, channelMap, (UInt32)sizeof(channelMap));
		if(noErr != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.CoreAudio", "AudioUnitSetProperty (kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input) failed: " << result);
			return false;
		}
	}
#endif

	return true;
}

#pragma mark Callbacks

OSStatus SFB::Audio::CoreAudioOutput::Render(AudioUnitRenderActionFlags		*ioActionFlags,
											 const AudioTimeStamp			*inTimeStamp,
											 UInt32							inBusNumber,
											 UInt32							inNumberFrames,
											 AudioBufferList				*ioData)
{
#pragma unused(ioActionFlags)
#pragma unused(inTimeStamp)
#pragma unused(inBusNumber)

	mPlayer->ProvideAudio(ioData, inNumberFrames);
	return noErr;
}
