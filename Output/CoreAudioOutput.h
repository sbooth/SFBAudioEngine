/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudioTypes.h>

#include "AudioOutput.h"

/*! @file CoreAudioOutput.h @brief Core %Audio output functionality */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief Output subclass supporting Apple's Core %Audio */
		class CoreAudioOutput : public Output
		{
		public:

			CoreAudioOutput();
			virtual ~CoreAudioOutput();


			// ========================================
			/*! @name Device Parameters */
			//@{

			/*!
			 * @brief Get the device volume
			 *
			 * This corresponds to the property \c kHALOutputParam_Volume on element \c 0
			 * @note The volume is linear across the interval [0, 1]
			 * @param volume A \c Float32 to receive the volume
			 * @return \c true on success, \c false otherwise
			 */
			bool GetVolume(Float32& volume) const;

			/*!
			 * @brief Set the device volume
			 *
			 * This corresponds to the property \c kHALOutputParam_Volume on element \c 0
			 * @note The volume is linear and the value will be clamped to the interval [0, 1]
			 * @param volume The desired volume
			 * @return \c true on success, \c false otherwise
			 */
			bool SetVolume(Float32 volume);


			/*!
			 * @brief Get the volume for the specified channel
			 *
			 * This corresponds to the property \c kHALOutputParam_Volume on element \c channel
			 * @note The volume is linear across the interval [0, 1]
			 * @param channel The desired channel
			 * @param volume A \c Float32 to receive the channel's volume
			 * @return \c true on success, \c false otherwise
			 */
			bool GetVolumeForChannel(UInt32 channel, Float32& volume) const;

			/*!
			 * @brief Set the volume for the specified channel
			 *
			 * This corresponds to the property \c kHALOutputParam_Volume on element \c channel
			 * @note The volume is linear and the value will be clamped to the interval [0, 1]
			 * @param channel The desired channel
			 * @param volume The desired volume
			 * @return \c true on success, \c false otherwise
			 */
			bool SetVolumeForChannel(UInt32 channel, Float32 volume);


			/*!
			 * @brief Get the audio processing graph pre-gain
			 *
			 * This corresponds to the property \c kMultiChannelMixerParam_Volume
			 * @note Pre-gain is linear across the interval [0, 1]
			 * @param preGain A \c Float32 to receive the pre-gain
			 * @return \c true on success, \c false otherwise
			 */
			bool GetPreGain(Float32& preGain) const;

			/*!
			 * @brief Set the audio processing graph pre-gain
			 *
			 * This corresponds to the property \c kMultiChannelMixerParam_Volume
			 * @note The pre-gain is linear and the value will be clamped to the interval [0, 1]
			 * @param preGain The desired pre-gain
			 * @return \c true on success, \c false otherwise
			 */
			bool SetPreGain(Float32 preGain);


			/*! @brief Query whether the output is performing sample rate conversion */
			bool IsPerformingSampleRateConversion() const;


			/*!
			 * @brief Get the sample rate converter's complexity
			 *
			 * This corresponds to the property \c kAudioUnitProperty_SampleRateConverterComplexity
			 * @param complexity A \c UInt32 to receive the SRC complexity
			 * @return \c true on success, \c false otherwise
			 * @see kAudioUnitProperty_SampleRateConverterComplexity
			 */
			bool GetSampleRateConverterComplexity(UInt32& complexity) const;

			/*!
			 * @brief Set the sample rate converter's complexity
			 *
			 * This corresponds to the property \c kAudioUnitProperty_SampleRateConverterComplexity
			 * @param complexity The desired SRC complexity
			 * @return \c true on success, \c false otherwise
			 * @see kAudioUnitProperty_SampleRateConverterComplexity
			 */
			bool SetSampleRateConverterComplexity(UInt32 complexity);


			/*!
			 * @brief Get the sample rate converter's quality
			 *
			 * This corresponds to the property \c kAudioUnitProperty_RenderQuality
			 * @param quality A \c UInt32 to receive the SRC quality
			 * @return \c true on success, \c false otherwise
			 * @see kAudioUnitProperty_RenderQuality
			 */
			bool GetSampleRateConverterQuality(UInt32& quality) const;

			/*!
			 * @brief Set the sample rate converter's quality
			 *
			 * This corresponds to the property \c kAudioUnitProperty_RenderQuality
			 * @param quality The desired SRC quality
			 * @return \c true on success, \c false otherwise
			 * @see kAudioUnitProperty_RenderQuality
			 */
			bool SetSampleRateConverterQuality(UInt32 quality);

			//@}


			// ========================================
			/*! @name DSP Effects */
			//@{

            /*!
			 * @brief Add a DSP effect to the audio processing graph with the component type kAudioUnitType_Effect
			 * @param subType The \c AudioComponent subtype
			 * @param manufacturer The \c AudioComponent manufacturer
			 * @param flags The \c AudioComponent flags
			 * @param mask The \c AudioComponent mask
			 * @param effectUnit An optional pointer to an \c AudioUnit to receive the effect
			 * @return \c true on success, \c false otherwise
			 * @see AudioComponentDescription
			 */
			bool AddEffect(OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit = nullptr);

			/*!
			 * @brief Add a DSP effect to the audio processing graph
             * @param componentType The \c AudioComponent type, normally \c kAudioUnitType_Effect
			 * @param subType The \c AudioComponent subtype
			 * @param manufacturer The \c AudioComponent manufacturer
			 * @param flags The \c AudioComponent flags
			 * @param mask The \c AudioComponent mask
			 * @param effectUnit An optional pointer to an \c AudioUnit to receive the effect
			 * @return \c true on success, \c false otherwise
			 * @see AudioComponentDescription
			 */
			bool AddEffect(OSType componentType, OSType subType, OSType manufacturer, UInt32 flags, UInt32 mask, AudioUnit *effectUnit = nullptr);

			/*!
			 * @brief Remove the specified DSP effect
			 * @param effectUnit The \c AudioUnit to remove from the processing graph
			 * @return \c true on success, \c false otherwise
			 */
			bool RemoveEffect(AudioUnit effectUnit);

			//@}


#if !TARGET_OS_IPHONE
			// ========================================
			/*! @name Hog Mode */
			//@{

			/*! @brief Query whether the output device hogged */
			bool DeviceIsHogged() const;

			/*!
			 * @brief Start hogging the output device
			 *
			 * This will attempt to set the property \c kAudioDevicePropertyHogMode
			 * @return \c true on success, \c false otherwise
			 */
			bool StartHoggingDevice();

			/*!
			 * @brief Stop hogging the output device
			 *
			 * This will attempt to clear the property \c kAudioDevicePropertyHogMode
			 * @return \c true on success, \c false otherwise
			 */
			bool StopHoggingDevice();

			//@}


			// ========================================
			/*! @name Device parameters */
			//@{

			/*! @brief Query whether the output device muted */
			bool DeviceIsMuted() const;

			/*!
			 * @brief Mute the output device
			 *
			 * This will attempt to set the property \c kAudioDevicePropertyMute
			 * @return \c true on success, \c false otherwise
			 */
			bool MuteDevice();

			/*!
			 * @brief Unmute the output device
			 *
			 * This will attempt to clear the property \c kAudioDevicePropertyMute
			 * @return \c true on success, \c false otherwise
			 */
			bool UnmuteDevice();


			/*!
			 * @brief Get the device's master volume
			 *
			 * This corresponds to the property \c kAudioDevicePropertyVolumeScalar on element \c kAudioObjectPropertyElementMaster
			 * @param volume A \c Float32 to receive the master volume
			 * @return \c true on success, \c false otherwise
			 */
			bool GetDeviceMasterVolume(Float32& volume) const;

			/*!
			 * @brief Set the device's master volume
			 *
			 * This corresponds to the property \c kAudioDevicePropertyVolumeScalar on element \c kAudioObjectPropertyElementMaster
			 * @param volume The desired master volume
			 * @return \c true on success, \c false otherwise
			 * @see kAudioDevicePropertyVolumeScalar
			 */
			bool SetDeviceMasterVolume(Float32 volume);


			/*!
			 * @brief Get the device's volume for the specified channel
			 *
			 * This corresponds to the property \c kAudioDevicePropertyVolumeScalar on element \c channel
			 * @param channel The desired channel
			 * @param volume A \c Float32 to receive the volume
			 * @return \c true on success, \c false otherwise
			 */
			bool GetDeviceVolumeForChannel(UInt32 channel, Float32& volume) const;

			/*!
			 * @brief Set the device's volume for the specified channel
			 *
			 * This corresponds to the property \c kAudioDevicePropertyVolumeScalar on element \c channel
			 * @param channel The desired channel
			 * @param volume The desired volume
			 * @return \c true on success, \c false otherwise
			 */
			bool SetDeviceVolumeForChannel(UInt32 channel, Float32 volume);


			/*!
			 * @brief Get the number of output channels on the device
			 * @param channelCount A \c UInt32 to receive the channel count
			 * @return \c true on success, \c false otherwise
			 */
			bool GetDeviceChannelCount(UInt32& channelCount) const;

			/*!
			 * @brief Get the device's preferred stereo channel
			 * @param preferredStereoChannels A \c std::pair to receive the channels
			 * @return \c true on success, \c false otherwise
			 */
			bool GetDevicePreferredStereoChannels(std::pair<UInt32, UInt32>& preferredStereoChannels) const;


			/*!
			 * @brief Get the device's nominal sample rates
			 * @param nominalSampleRates A \c std::vector to receive the sample rates
			 * @return \c true on success, \c false otherwise
			 */
			bool GetDeviceAvailableNominalSampleRates(std::vector<AudioValueRange>& nominalSampleRates) const;

			//@}


			// ========================================
			/*! @name Device Management */
			//@{

			/*!
			 * @brief Get the device ID of the output device
			 * @param deviceID An \c AudioDeviceID to receive the device ID
			 * @return \c true on success, \c false otherwise
			 * @see CreateDeviceUID()
			 */
			bool GetDeviceID(AudioDeviceID& deviceID) const;

			/*!
			 * @brief Set the output device to the device matching the provided ID
			 * @param deviceID The ID of the desired device
			 * @return \c true on success, \c false otherwise
			 * @see SetDeviceUID()
			 */
			bool SetDeviceID(AudioDeviceID deviceID);


			/*!
			 * @brief Get the available data sources for the current device
			 *
			 * This corresponds to the property \c kAudioDevicePropertyDataSources
			 * @param dataSources A \c std::vector to receive the data sources
			 * @return \c true on success, \c false otherwise
			 * @see kAudioDevicePropertyDataSources
			 */
			bool GetAvailableDataSources(std::vector<UInt32>& dataSources) const;


			/*!
			 * @brief Get the active data sources for the current device
			 *
			 * This corresponds to the property \c kAudioDevicePropertyDataSource
			 * @param dataSources A \c std::vector to receive the data sources
			 * @return \c true on success, \c false otherwise
			 * @see kAudioDevicePropertyDataSource
			 */
			bool GetActiveDataSources(std::vector<UInt32>& dataSources) const;

			/*!
			 * @brief Set the active data sources for the current device
			 *
			 * This corresponds to the property \c kAudioDevicePropertyDataSource
			 * @param dataSources A \c std::vector containing the desired data sources
			 * @return \c true on success, \c false otherwise
			 * @see kAudioDevicePropertyDataSource
			 */
			bool SetActiveDataSources(const std::vector<UInt32>& dataSources);

			//@}


			// ========================================
			/*! Stream Management */
			//@{

			/*!
			 * @brief Get the output streams for the current device
			 *
			 * This corresponds to the property \c kAudioDevicePropertyStreams
			 * @param streams A \c std::vector to receive the output streams
			 * @return \c true on success, \c false otherwise
			 * @see kAudioDevicePropertyStreams
			 */
			bool GetOutputStreams(std::vector<AudioStreamID>& streams) const;


//			bool GetOutputStreamVirtualFormat(AudioStreamID streamID, AudioStreamBasicDescription& virtualFormat) const;
//			bool SetOutputStreamVirtualFormat(AudioStreamID streamID, const AudioStreamBasicDescription& virtualFormat);


			/*!
			 * @brief Get the physical format for the specified output stream on the current device
			 *
			 * This corresponds to the property \c kAudioStreamPropertyPhysicalFormat
			 * @param streamID The output stream ID
			 * @param physicalFormat An \c AudioStreamBasicDescription to receive the stream's physical format
			 * @return \c true on success, \c false otherwise
			 * @see kAudioStreamPropertyPhysicalFormat
			 */
			bool GetOutputStreamPhysicalFormat(AudioStreamID streamID, AudioStreamBasicDescription& physicalFormat) const;

			/*!
			 * @brief Set the physical format for the specified output stream on the current device
			 *
			 * This corresponds to the property \c kAudioStreamPropertyPhysicalFormat
			 * @param streamID The output stream ID
			 * @param physicalFormat The desired physical format
			 * @see kAudioStreamPropertyPhysicalFormat
			 */
			bool SetOutputStreamPhysicalFormat(AudioStreamID streamID, const AudioStreamBasicDescription& physicalFormat);

			//@}

#endif


			// ========================================
			/*! Advanced AUGraph Functionality */
			//@{

			/*!
			 * @brief Get the latency of the AUGraph in seconds
			 *
			 * This corresponds to the sum of the property \c kAudioUnitProperty_Latency over the
			 * nodes of the AUGraph
			 * @param latency A \c Float64 to receive the AUGraph's latency
			 * @return \c true on success, \c false otherwise
			 */
			bool GetAUGraphLatency(Float64& latency) const;

			/*!
			 * @brief Get the tail time of the AUGraph in seconds
			 *
			 * This corresponds to the sum of the property \c kAudioUnitProperty_TailTime over the
			 * nodes of the AUGraph
			 * @param tailTime A \c Float64 to receive the AUGraph's tail time
			 * @return \c true on success, \c false otherwise
			 */
			bool GetAUGraphTailTime(Float64& tailTime) const;


			/*!
			 * @brief Get the \c AUGraph used internally for audio processing
			 *
			 * The default AUGraph has two nodes: a mixer (\c kAudioUnitSubType_MultiChannelMixer)
			 * connected to the output (\c kAudioUnitSubType_HALOutput on macOS, \c kAudioUnitSubType_RemoteIO on iOS)
			 *
			 * @warning The graph may be manipulated but changing the graph's mixer or output node is not supported
			 *
			 * @param graph An \c AUGraph to receive the graph used for audio processing
			 * @return \c true on success, \c false otherwise
			 */
			bool GetAUGraph(AUGraph& graph) const;


			/*!
			 * @brief Get the mixer \c AUNode for the internal \c AUGraph
			 *
			 * @param node An \c AUNode to receive the mixer node
			 * @return \c true on success, \c false otherwise
			 */
			bool GetAUGraphMixerNode(AUNode& node) const;

			/*!
			 * @brief Get the output \c AUNode for the internal \c AUGraph
			 *
			 * @param node An \c AUNode to receive the output node
			 * @return \c true on success, \c false otherwise
			 */
			bool GetAUGraphOutputNode(AUNode& node) const;


			/*!
			 * @brief Get the mixer \c AudioUnit used by the internal \c AUGraph
			 *
			 * @param au An \c AudioUnit to receive the mixer unit
			 * @return \c true on success, \c false otherwise
			 */
			bool GetAUGraphMixer(AudioUnit& au) const;

			/*!
			 * @brief Get the output \c AudioUnit used by the internal \c AUGraph
			 *
			 * @param au An \c AudioUnit to receive the output unit
			 * @return \c true on success, \c false otherwise
			 */
			bool GetAUGraphOutput(AudioUnit& au) const;

			//@}

		private:

			virtual bool _Open();
			virtual bool _Close();

			virtual bool _Start();
			virtual bool _Stop();
			virtual bool _RequestStop();

			virtual bool _IsOpen() const;
			virtual bool _IsRunning() const;

			virtual bool _Reset();

			virtual bool _SupportsFormat(const AudioFormat& format) const;

			virtual bool _SetupForDecoder(const Decoder& decoder);

#if !TARGET_OS_IPHONE
			virtual bool _CreateDeviceUID(CFStringRef& deviceUID) const;
			virtual bool _SetDeviceUID(CFStringRef deviceUID);

			virtual bool _GetDeviceSampleRate(Float64& sampleRate) const;
			virtual bool _SetDeviceSampleRate(Float64 sampleRate);
#endif

			virtual size_t _GetPreferredBufferSize() const;

			// ========================================
			// AUGraph Utilities
			bool SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize);

			bool SetOutputUnitChannelMap(const ChannelLayout& channelLayout);


			AUGraph		mAUGraph;
			AUNode		mMixerNode;
			AUNode		mOutputNode;
			UInt32		mDefaultMaximumFramesPerSlice;

		public:

			// ========================================
			/*! @cond */

			/*! @internal AUNode render callback */
			OSStatus Render(AudioUnitRenderActionFlags		*ioActionFlags,
							const AudioTimeStamp			*inTimeStamp,
							UInt32							inBusNumber,
							UInt32							inNumberFrames,
							AudioBufferList					*ioData);

			/*! @endcond */
		};
	}
}
