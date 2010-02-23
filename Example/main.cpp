#include <CoreFoundation/CoreFoundation.h>

#include "SFBAudioEngine/AudioDecoder.h"
#include "SFBAudioEngine/AudioMetadata.h"
#include "SFBAudioEngine/AudioPlayer.h"
#include "SFBAudioEngine/DSPAudioPlayer.h"

#include "CAStreamBasicDescription.h"

#include <unistd.h>


static CFArrayRef
CreateOutputDeviceArray()
{
	UInt32 specifierSize = 0;
	OSStatus status = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &specifierSize, NULL);
	if(kAudioHardwareNoError != status) {
		printf("AudioHardwareGetPropertyInfo (kAudioHardwarePropertyDevices) failed: %i\n", status);
		return NULL;
	}
		
	UInt32 deviceCount = static_cast<UInt32>(specifierSize / sizeof(AudioDeviceID));
		
	AudioDeviceID *audioDevices = static_cast<AudioDeviceID *>(calloc(1, specifierSize));
	if(NULL == audioDevices) {
		puts("Unable to allocate memory");
		return NULL;
	}
	
	status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &specifierSize, audioDevices);
	if(kAudioHardwareNoError != status) {
		printf("AudioHardwareGetProperty (kAudioHardwarePropertyDevices) failed: %i\n", status);
		free(audioDevices), audioDevices = NULL;
		return NULL;
	}
	
	CFMutableArrayRef outputDeviceArray = CFArrayCreateMutable(kCFAllocatorDefault, deviceCount, &kCFTypeArrayCallBacks);
	if(NULL == outputDeviceArray) {
		puts("CFArrayCreateMutable failed");
		free(audioDevices), audioDevices = NULL;
		return NULL;
	}
	
	// Iterate through all the devices and determine which are output-capable
	for(UInt32 i = 0; i < deviceCount; ++i) {
		// Query device UID
		CFStringRef deviceUID = NULL;
		specifierSize = sizeof(deviceUID);
		status = AudioDeviceGetProperty(audioDevices[i], 0, FALSE, kAudioDevicePropertyDeviceUID, &specifierSize, &deviceUID);
		if(kAudioHardwareNoError != status) {
			printf("AudioDeviceGetProperty (kAudioDevicePropertyDeviceUID) failed: %i\n", status);
			continue;
		}
		
		// Query device name
		CFStringRef deviceName = NULL;
		specifierSize = sizeof(deviceName);
		status = AudioDeviceGetProperty(audioDevices[i], 0, FALSE, kAudioDevicePropertyDeviceNameCFString, &specifierSize, &deviceName);
		if(kAudioHardwareNoError != status) {
			printf("AudioDeviceGetProperty (kAudioDevicePropertyDeviceNameCFString) failed: %i\n", status);
			continue;
		}

		// Query device manufacturer
		CFStringRef deviceManufacturer = NULL;
		specifierSize = sizeof(deviceManufacturer);
		status = AudioDeviceGetProperty(audioDevices[i], 0, FALSE, kAudioDevicePropertyDeviceManufacturerCFString, &specifierSize, &deviceManufacturer);
		if(kAudioHardwareNoError != status) {
			printf("AudioDeviceGetProperty (kAudioDevicePropertyDeviceManufacturerCFString) failed: %i\n", status);
			continue;
		}
		
		// Determine if the device is an output device (it is an output device if it has output channels)
		specifierSize = 0;
		status = AudioDeviceGetPropertyInfo(audioDevices[i], 0, FALSE, kAudioDevicePropertyStreamConfiguration, &specifierSize, NULL);
		if(kAudioHardwareNoError != status) {
			printf("AudioDeviceGetPropertyInfo (kAudioDevicePropertyStreamConfiguration) failed: %i\n", status);
			continue;
		}
		
		AudioBufferList *bufferList = static_cast<AudioBufferList *>(calloc(1, specifierSize));
		if(NULL == bufferList) {
			puts("Unable to allocate memory");
			break;
		}
		
		status = AudioDeviceGetProperty(audioDevices[i], 0, FALSE, kAudioDevicePropertyStreamConfiguration, &specifierSize, bufferList);
		if(kAudioHardwareNoError != status || 0 == bufferList->mNumberBuffers) {
			printf("AudioDeviceGetProperty (kAudioDevicePropertyStreamConfiguration) failed: %i\n", status);
			free(bufferList), bufferList = NULL;
			continue;			
		}
		
		free(bufferList), bufferList = NULL;
		
		// Add a dictionary for this device to the array of output devices
		CFStringRef keys	[]	= { CFSTR("deviceUID"),		CFSTR("deviceName"),	CFSTR("deviceManufacturer") };
		CFStringRef values	[]	= { deviceUID,				deviceName,				deviceManufacturer };
		
		CFDictionaryRef deviceDictionary = CFDictionaryCreate(kCFAllocatorDefault, 
															  reinterpret_cast<const void **>(keys), 
															  reinterpret_cast<const void **>(values), 
															  3,
															  &kCFTypeDictionaryKeyCallBacks,
															  &kCFTypeDictionaryValueCallBacks);
		

		CFArrayAppendValue(outputDeviceArray, deviceDictionary);
	}
	
	free(audioDevices), audioDevices = NULL;
	
	// Return a non-mutable copy of the array
	CFArrayRef copy = CFArrayCreateCopy(kCFAllocatorDefault, outputDeviceArray);
	CFRelease(outputDeviceArray), outputDeviceArray = NULL;
	
	return copy;
}

static void
PrintStreamsForDeviceID(AudioDeviceID deviceID)
{
	AudioObjectShow(deviceID);
	
	UInt32 mixable = 0xFFFF;
	UInt32 specifierSize = sizeof(mixable);
	OSStatus result = AudioDeviceGetProperty(deviceID, 
											 0, 
											 FALSE, 
											 kAudioDevicePropertySupportsMixing, 
											 &specifierSize, 
											 &mixable);
	
	if(noErr != result)
		fprintf(stderr, "AudioDeviceGetProperty (kAudioDevicePropertySupportsMixing) failed: %i\n", result);
	
	printf("Device supports mixing: %s\n", mixable ? "Yes" : "No");

	result = AudioDeviceGetPropertyInfo(deviceID, 
										0, 
										FALSE, 
										kAudioDevicePropertyStreams, 
										&specifierSize, 
										NULL);

	if(noErr != result) {
		fprintf(stderr, "AudioDeviceGetPropertyInfo (kAudioDevicePropertyStreams) failed: %i\n", result);
		return;
	}
	
	UInt32 streamCount = static_cast<UInt32>(specifierSize / sizeof(AudioStreamID));

	AudioStreamID audioStreams [streamCount];
	
	result = AudioDeviceGetProperty(deviceID, 
									0,
									FALSE, 
									kAudioDevicePropertyStreams,
									&specifierSize,
									audioStreams);
	
	if(noErr != result) {
		fprintf(stderr, "AudioDeviceGetProperty (kAudioDevicePropertyStreams) failed: %i\n", result);
		return;
	}
	
	printf("\nDevice has %d streams\n", streamCount);
	
	for(UInt32 i = 0; i < streamCount; ++i) {
		AudioStreamID streamID = audioStreams[i];
		
		AudioObjectShow(streamID);

		result = AudioStreamGetPropertyInfo(streamID, 
											0, 
											kAudioStreamPropertyAvailableVirtualFormats, 
											&specifierSize, 
											NULL);

		if(noErr != result) {
			fprintf(stderr, "AudioStreamGetPropertyInfo (kAudioStreamPropertyAvailableVirtualFormats) failed: %i\n", result);
			continue;
		}
		
		UInt32 virtualFormatCount = static_cast<UInt32>(specifierSize / sizeof(AudioStreamRangedDescription));

		AudioStreamRangedDescription virtualFormats [virtualFormatCount];
		
		result = AudioStreamGetProperty(streamID, 
										0, 
										kAudioStreamPropertyAvailableVirtualFormats,
										&specifierSize, 
										virtualFormats);

		if(noErr != result) {
			fprintf(stderr, "AudioStreamGetProperty (kAudioStreamPropertyAvailableVirtualFormats) failed: %i\n", result);
			continue;
		}
		
		printf("\nAudioStream %x Virtual Formats:\n",streamID);
		for(UInt32 j = 0; j < virtualFormatCount; ++j) {
			CAStreamBasicDescription sd = virtualFormats[j].mFormat;
			sd.Print(stdout);
			printf("reserved: %i\n", sd.mReserved);
		}

		result = AudioStreamGetPropertyInfo(streamID, 
											0, 
											kAudioStreamPropertyAvailablePhysicalFormats, 
											&specifierSize, 
											NULL);
		
		if(noErr != result) {
			fprintf(stderr, "AudioStreamGetPropertyInfo (kAudioStreamPropertyAvailablePhysicalFormats) failed: %i\n", result);
			continue;
		}
		
		UInt32 physicalFormatCount = static_cast<UInt32>(specifierSize / sizeof(AudioStreamRangedDescription));
		
		AudioStreamRangedDescription physicalFormats [physicalFormatCount];
		
		result = AudioStreamGetProperty(streamID, 
										0, 
										kAudioStreamPropertyAvailablePhysicalFormats, 
										&specifierSize, 
										physicalFormats);
		
		if(noErr != result) {
			fprintf(stderr, "AudioStreamGetProperty (kAudioStreamPropertyAvailablePhysicalFormats) failed: %i\n", result);
			continue;
		}
		
		printf("\nAudioStream %x Physical Formats:\n",streamID);
		for(UInt32 j = 0; j < physicalFormatCount; ++j) {
			CAStreamBasicDescription sd = physicalFormats[j].mFormat;
			sd.Print(stdout);
		}
	}
}

static void
PrintStreamsForDeviceUID(CFStringRef deviceUID)
{
	assert(NULL != deviceUID);
	
	AudioDeviceID deviceID = kAudioDeviceUnknown;
	AudioValueTranslation translation;
	
	translation.mInputData			= &deviceUID;
	translation.mInputDataSize		= sizeof(deviceUID);
	translation.mOutputData			= &deviceID;
	translation.mOutputDataSize		= sizeof(deviceID);
	
	UInt32 specifierSize			= sizeof(translation);
	
	OSStatus result = AudioHardwareGetProperty(kAudioHardwarePropertyDeviceForUID, 
											   &specifierSize, 
											   &translation);
	
	if(noErr != result) {
		fprintf(stderr, "AudioHardwareGetProperty (kAudioHardwarePropertyDeviceForUID) failed: %i", result);
		return;
	}
	
	PrintStreamsForDeviceID(deviceID);
}

static void
PrintStreamFormatsForDeviceID(AudioDeviceID deviceID)
{
	OSStatus result;
	UInt32 specifierSize;
	
	result = AudioDeviceGetPropertyInfo(deviceID, 
										0, 
										FALSE, 
										kAudioDevicePropertyStreams, 
										&specifierSize, 
										NULL);
	
	if(noErr != result) {
		fprintf(stderr, "AudioDeviceGetPropertyInfo (kAudioDevicePropertyStreams) failed: %i\n", result);
		return;
	}
	
	UInt32 streamCount = static_cast<UInt32>(specifierSize / sizeof(AudioStreamID));
	
	AudioStreamID audioStreams [streamCount];
	
	result = AudioDeviceGetProperty(deviceID, 
									0,
									FALSE, 
									kAudioDevicePropertyStreams,
									&specifierSize,
									audioStreams);
	
	if(noErr != result) {
		fprintf(stderr, "AudioDeviceGetProperty (kAudioDevicePropertyStreams) failed: %i\n", result);
		return;
	}
		
	for(UInt32 i = 0; i < streamCount; ++i) {
		AudioStreamID streamID = audioStreams[i];
		
		CAStreamBasicDescription virtualFormat;
		specifierSize = sizeof(virtualFormat);
		
		result = AudioStreamGetProperty(streamID, 
										0, 
										kAudioStreamPropertyVirtualFormat,
										&specifierSize, 
										&virtualFormat);
		
		if(noErr != result) {
			fprintf(stderr, "AudioStreamGetProperty (kAudioStreamPropertyVirtualFormat) failed: %i\n", result);
			continue;
		}
		
		printf("Current virtual format: ");
		virtualFormat.Print(stdout);
		
		CAStreamBasicDescription physicalFormat;
		specifierSize = sizeof(physicalFormat);
		
		result = AudioStreamGetProperty(streamID, 
										0, 
										kAudioStreamPropertyPhysicalFormat,
										&specifierSize, 
										&physicalFormat);
		
		if(noErr != result) {
			fprintf(stderr, "AudioStreamGetProperty (kAudioStreamPropertyPhysicalFormat) failed: %i\n", result);
			continue;
		}
		
		printf("Current physical format: ");
		physicalFormat.Print(stdout);		
	}
}

static void
PrintStreamFormatsForDeviceUID(CFStringRef deviceUID)
{
	assert(NULL != deviceUID);
	
	AudioDeviceID deviceID = kAudioDeviceUnknown;
	AudioValueTranslation translation;
	
	translation.mInputData			= &deviceUID;
	translation.mInputDataSize		= sizeof(deviceUID);
	translation.mOutputData			= &deviceID;
	translation.mOutputDataSize		= sizeof(deviceID);
	
	UInt32 specifierSize			= sizeof(translation);
	
	OSStatus result = AudioHardwareGetProperty(kAudioHardwarePropertyDeviceForUID, 
											   &specifierSize, 
											   &translation);
	
	if(noErr != result) {
		fprintf(stderr, "AudioHardwareGetProperty (kAudioHardwarePropertyDeviceForUID) failed: %i", result);
		return;
	}
	
	PrintStreamFormatsForDeviceID(deviceID);
}

static void
decodingStarted(void					*context,
				const AudioDecoder		*decoder)
{
#pragma unused(context)
	printf("Decoding started: ");
	CFShow(const_cast<CFURLRef>(const_cast<AudioDecoder *>(decoder)->GetURL()));
}

static void
decodingFinished(void					*context,
				 const AudioDecoder		*decoder)
{
#pragma unused(context)
	printf("Decoding finished: ");
	CFShow(const_cast<CFURLRef>(const_cast<AudioDecoder *>(decoder)->GetURL()));
}

static void
renderingStarted(void					*context,
				 const AudioDecoder		*decoder)
{
#pragma unused(context)
	printf("Rendering started: ");
	CFShow(const_cast<CFURLRef>(const_cast<AudioDecoder *>(decoder)->GetURL()));
}

static void
renderingFinished(void					*context,
				  const AudioDecoder		*decoder)
{
#pragma unused(context)
	printf("Rendering finished: ");
	CFShow(const_cast<CFURLRef>(const_cast<AudioDecoder *>(decoder)->GetURL()));
}

static void usage(const char *argv0)
{
	assert(NULL != argv0);
	
	fprintf(stderr, "Usage: %s file [file ...]", argv0);
}

//#include <getopt.h>
//
///* Flag set by ‘--verbose’. */
//static int verbose_flag;
//
//static struct option long_options [] = {
//	/* These options set a flag. */
//	{"verbose", no_argument,       &verbose_flag, 1},
//	{"brief",   no_argument,       &verbose_flag, 0},
//	/* These options don't set a flag.
//	 We distinguish them by their indices. */
//	{"add",     no_argument,       0, 'a'},
//	{"append",  no_argument,       0, 'b'},
//	{"delete",  required_argument, 0, 'd'},
//	{"create",  required_argument, 0, 'c'},
//	{"file",    required_argument, 0, 'f'},
//	{0, 0, 0, 0}
//};

int main(int argc, char *argv [])
{
//	for(;;) {
//		/* getopt_long stores the option index here. */
//		int option_index = 0;
//		
//		int c = getopt_long(argc, 
//							argv, 
//							"abc:d:f:",
//							long_options, 
//							&option_index);
//		
//		/* Detect the end of the options. */
//		if (c == -1)
//			break;
//		
//		switch(c) {
//			case 0:
//				/* If this option set a flag, do nothing else now. */
//				if(long_options[option_index].flag != 0)
//					break;
//				printf("option %s", long_options[option_index].name);
//				if(optarg)
//					printf (" with arg %s", optarg);
//				printf ("\n");
//				break;
//				
//			case 'a':
//				puts("option -a\n");
//				break;
//				
//			case 'b':
//				puts("option -b\n");
//				break;
//				
//			case 'c':
//				printf("option -c with value `%s'\n", optarg);
//				break;
//				
//			case 'd':
//				printf("option -d with value `%s'\n", optarg);
//				break;
//				
//			case 'f':
//				printf("option -f with value `%s'\n", optarg);
//				break;
//				
//			case '?':
//				/* getopt_long already printed an error message. */
//				break;
//				
//			default:
//				abort();
//		}
//	}
//	
//	/* Instead of reporting ‘--verbose’
//	 and ‘--brief’ as they are encountered,
//	 we report the final status resulting from them. */
//	if(verbose_flag)
//		puts("verbose flag is set");
//	
//	/* Print any remaining command line arguments (not options). */
//	if(optind < argc) {
//		printf("non-option ARGV-elements: ");
//		while(optind < argc)
//			printf ("%s ", argv[optind++]);
//		putchar('\n');
//	}
// 
//	return EXIT_SUCCESS;
//	
//	
	
	if(1 == argc) {
		printf("Usage: %s file [file ...]", argv[0]);
		return EXIT_FAILURE;
	}
	
//	CFArrayRef outputDevices = CreateOutputDeviceArray();
//	CFShow(outputDevices);
//	CFRelease(outputDevices), outputDevices = NULL;
//	return 1;
	AudioPlayer player;
	
	Float32 volume;
	if(player.GetMasterVolume(volume))
		printf("Master Volume: %f\n", volume);

	if(player.GetVolumeForChannel(1, volume))
		printf("Channel 1 Volume: %f\n", volume);
	
//	if(false == player.SetOutputDeviceUID(CFSTR("SoundflowerEngine:0")))
	if(false == player.SetOutputDeviceUID(CFSTR("AppleUSBAudioEngine:Wavelength Audio,ltd:Proton USBDAC:(C) 2009 Wavelength Audio, ltd.:1")))
//	if(false == player.SetOutputDeviceUID(CFSTR("AppleUSBAudioEngine:Texas Instruments:Benchmark 1.0:fd111000:1")))
		puts("Couldn't set output device UID");

	if(false == player.StartHoggingOutputDevice())
		puts("Couldn't hog output device");

	CAStreamBasicDescription asbd, oldFormat, vFormat;

	puts("====================");
	
	if(player.GetOutputStreamPhysicalFormat(oldFormat)) {
		printf("Original physical format: ");
		oldFormat.Print(stdout);
	}
	
	if(player.GetOutputStreamVirtualFormat(vFormat)) {
		printf("Original virtual format:  ");
		vFormat.Print(stdout);
	}
	
	puts("====================");

	asbd.mFormatID			= kAudioFormatLinearPCM;
	asbd.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonMixable;
	asbd.mBitsPerChannel	= 24;
	asbd.mChannelsPerFrame	= 2;
	asbd.mSampleRate		= 44100;
	asbd.mFramesPerPacket	= 1;
	asbd.mBytesPerFrame		= 6;
	asbd.mBytesPerPacket	= 6;
	asbd.mReserved			= 0;
	
	if(false == player.SetOutputStreamPhysicalFormat(asbd))
		puts("Couldn't set physical format");
//	else
//		asbd.Print(stderr);

	while(0 < --argc) {
		
		const char *path = *++argv;

		CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(path), strlen(path), FALSE);

		AudioMetadata *metadata = AudioMetadata::CreateMetadataForURL(fileURL);
		if(NULL == metadata)
			puts("Couldn't create metadata");
		
//		13:33:52 - 17:21:06
//		AudioDecoder *decoder = AudioDecoder::CreateDecoderForURLRegion(fileURL,
//																		static_cast<SInt64>(44100*((13*60) + 33 + (52./75))), 
//																		static_cast<UInt32>(44100*(3*60 + 47 + (14./75))),
//																		0);
		AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(fileURL);
		if(NULL == decoder) {
			puts("Couldn't create decoder");
			continue;
		}

		decoder->SetDecodingStartedCallback(decodingStarted, NULL);
		decoder->SetDecodingFinishedCallback(decodingFinished, NULL);
		decoder->SetRenderingStartedCallback(renderingStarted, NULL);
		decoder->SetRenderingFinishedCallback(renderingFinished, NULL);
		
		if(false == player.Enqueue(decoder))
			puts("Couldn't enqueue");
	}

	player.Play();

	while(player.IsPlaying() && 30 > player.GetCurrentTime()) {
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2, true);

		printf("%.2f / %.2f [%.2f]\n", player.GetCurrentTime(), player.GetTotalTime(), player.GetRemainingTime());
	}
	
	player.Stop();

	// Restore the old format
	if(false == player.SetOutputStreamPhysicalFormat(oldFormat))
		puts("Couldn't restore physical format");
	
	if(false == player.StopHoggingOutputDevice())
		puts("Couldn't stop hogging output device");

	return EXIT_SUCCESS;
}
