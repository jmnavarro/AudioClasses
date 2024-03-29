/*

File: SoundEngine.cpp
Abstract: These functions play background music tracks, multiple sound effects,
and support stereo panning with a low-latency response.

Version: 2.0.3 BETA    - the Stormy Productions remix
 
PLEASE NOTE:   This code was based on Apple's 1.7 SoundEngine code available 
 from the Crash Landing demo supplied with the iPhone SDK 2.0.
 
 Apple has acknowledged significant problems with the original code and has 
 in fact removed the code from future SDKs.    Per the original source code 
 agreement, Apple grants the right to anyone to modify and reuse the software
 in any manner (see original licensing agreement below).  As such, I 
 have changed the structure of the code to better suit my needs.
 
 THIS IS NOT A DROP-IN REPLACEMENT FOR THE 1.7 SOUND ENGINE!!!  
 
 Specifically, I removed the tying of a sound effect buffer to one OpenAL source. 
 Instead, you load a sound effect, which merely creates the static buffer.  To
 play the sound effect, you then "prime" it, which assigns it to an OpenAL source.
 If you want to play multiple instances of the same sound, you just prime the sound
 multiple times.  Each time you prime a sound effect, you are returned an OpenAL
 source.   There are a maximum of 32 sources and the code assumes a source is 
 available for use if you haven't yet primed it in the past or if it is currently
 in the AL_STOPPED state.  (If you don't know what that means, feel free to ignore
 it.)  I tried to hide the OpenAL details, while at the same time exposing the source ID
 for those who want direct control of the OpenAL features for a source.
 
 By making the sounds distinct from the sources, this now allows an easy method 
 for playing multiple instances of the same sound effect without using any extra buffers.
 If you tried to play the same sound with the original 1.7 SoundEngine code, it 
 would simply stop the sound and restart it.  Not ideal.
 
 The ideal usage of this library is to first load all your sound effects once. Then,
 when you want to play a sound effect, prime it, and start it.  Once you start
 a sound, you can pretty much ignore it.  OpenAL will handle stopping it for you 
 and as long as you don't exceed the 32 simultaneous sound limit, you should be ok.
 
 This is my first pass at modifying this library.  I assume it still has bugs,
 however I thought it might be useful for others looking for a quick and (very)
 dirty solution for playing multiple instances of the same sound.
 
 My latest version of the code will always be available at:
 
 http://stormyprods.com/SoundEngine
 
 Cheers,
 Brian
 
 Below is Apple's original license:
 

Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple Inc.
("Apple") in consideration of your agreement to the following terms, and your
use, installation, modification or redistribution of this Apple software
constitutes acceptance of these terms.  If you do not agree with these terms,
please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under
Apple's copyrights in this original Apple software (the "Apple Software"), to
use, reproduce, modify and redistribute the Apple Software, with or without
modifications, in source and/or binary forms; provided that if you redistribute
the Apple Software in its entirety and without modifications, you must retain
this notice and the following text and disclaimers in all such redistributions
of the Apple Software.
Neither the name, trademarks, service marks or logos of Apple Inc. may be used
to endorse or promote products derived from the Apple Software without specific
prior written permission from Apple.  Except as expressly stated in this notice,
no other rights or licenses, express or implied, are granted by Apple herein,
including but not limited to any patent rights that may be infringed by your
derivative works or by other works in which the Apple Software may be
incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
COMBINATION WITH YOUR PRODUCTS.

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR
DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF
CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF
APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Copyright (C) 2008 Apple Inc. All Rights Reserved.

*/

#if !TARGET_IPHONE_SIMULATOR
/*==================================================================================================
	SoundEngine.cpp
==================================================================================================*/
#if !defined(__SoundEngine_cpp__)
#define __SoundEngine_cpp__

//==================================================================================================
//	Includes
//==================================================================================================

//	System Includes
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CFURL.h>
#include <map>
#include <vector>
#include <pthread.h>
#include <mach/mach.h>

// Local Includes
#include "SoundEngine.h"

#define	AssertNoError(inMessage, inHandler)						\
			if(result != noErr)									\
			{													\
				printf("%s: %d\n", inMessage, (int)result);		\
				goto inHandler;									\
			}
			
#define AssertNoOALError(inMessage, inHandler)					\
			if((result = alGetError()) != AL_NO_ERROR)			\
			{													\
				printf("%s: %x\n", inMessage, (int)result);		\
				goto inHandler;									\
			}

#define kNumberBuffers 3    // Used for the bgMusic audio queue
#define MAX_SOURCES 10
#define kBackgroundMusicSlots 2

class OpenALObject;
class BackgroundTrackMgr;

static OpenALObject			*sOpenALObject = NULL;
static BackgroundTrackMgr	*sBackgroundTrackMgr[kBackgroundMusicSlots] = {NULL, NULL};
static Float32				gMasterVolumeGain = 1.0;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
typedef ALvoid	AL_APIENTRY	(*alBufferDataStaticProcPtr) (const ALint bid, ALenum format, ALvoid* data, ALsizei size, ALsizei freq);
ALvoid  alBufferDataStaticProc(const ALint bid, ALenum format, ALvoid* data, ALsizei size, ALsizei freq)
{
	static	alBufferDataStaticProcPtr	proc = NULL;
    
    if (proc == NULL) {
        proc = (alBufferDataStaticProcPtr) alcGetProcAddress(NULL, (const ALCchar*) "alBufferDataStatic");
    }
    
    if (proc)
        proc(bid, format, data, size, freq);

    return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
typedef ALvoid	AL_APIENTRY	(*alcMacOSXMixerOutputRateProcPtr) (const ALdouble value);
ALvoid  alcMacOSXMixerOutputRateProc(const ALdouble value)
{
	static	alcMacOSXMixerOutputRateProcPtr	proc = NULL;
    
    if (proc == NULL) {
        proc = (alcMacOSXMixerOutputRateProcPtr) alcGetProcAddress(NULL, (const ALCchar*) "alcMacOSXMixerOutputRate");
    }
    
    if (proc)
        proc(value);

    return;
}


//==================================================================================================
//	Helper functions
//==================================================================================================
OSStatus OpenFile(const char *inFilePath, AudioFileID &outAFID)
{
	
	CFURLRef theURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (UInt8*)inFilePath, strlen(inFilePath), false);
	if (theURL == NULL)
		return kSoundEngineErrFileNotFound;

#if TARGET_OS_IPHONE
	OSStatus result = AudioFileOpenURL(theURL, kAudioFileReadPermission, 0, &outAFID);
#else
	OSStatus result = AudioFileOpenURL(theURL, fsRdPerm, 0, &outAFID);
#endif
	CFRelease(theURL);
		AssertNoError("Error opening file", end);
	end:
		return result;
}

OSStatus LoadFileDataInfo(const char *inFilePath, AudioFileID &outAFID, AudioStreamBasicDescription &outFormat, UInt64 &outDataSize)
{
	UInt32 thePropSize = sizeof(outFormat);				
	OSStatus result = OpenFile(inFilePath, outAFID);
		AssertNoError("Error opening file", end);

	result = AudioFileGetProperty(outAFID, kAudioFilePropertyDataFormat, &thePropSize, &outFormat);
		AssertNoError("Error getting file format", end);
	
	thePropSize = sizeof(UInt64);
	result = AudioFileGetProperty(outAFID, kAudioFilePropertyAudioDataByteCount, &thePropSize, &outDataSize);
		AssertNoError("Error getting file data size", end);

end:
	return result;
}

void CalculateBytesForTime (AudioStreamBasicDescription & inDesc, UInt32 inMaxPacketSize, Float64 inSeconds, UInt32 *outBufferSize, UInt32 *outNumPackets)
{
	static const UInt32 maxBufferSize = 0x10000; // limit size to 64K
	static const UInt32 minBufferSize = 0x4000; // limit size to 16K

	if (inDesc.mFramesPerPacket) {
		Float64 numPacketsForTime = inDesc.mSampleRate / inDesc.mFramesPerPacket * inSeconds;
		*outBufferSize = numPacketsForTime * inMaxPacketSize;
	} else {
		// if frames per packet is zero, then the codec has no predictable packet == time
		// so we can't tailor this (we don't know how many Packets represent a time period
		// we'll just return a default buffer size
		*outBufferSize = maxBufferSize > inMaxPacketSize ? maxBufferSize : inMaxPacketSize;
	}
	
		// we're going to limit our size to our default
	if (*outBufferSize > maxBufferSize && *outBufferSize > inMaxPacketSize)
		*outBufferSize = maxBufferSize;
	else {
		// also make sure we're not too small - we don't want to go the disk for too small chunks
		if (*outBufferSize < minBufferSize)
			*outBufferSize = minBufferSize;
	}
	*outNumPackets = *outBufferSize / inMaxPacketSize;
}

static Boolean MatchFormatFlags(const AudioStreamBasicDescription& x, const AudioStreamBasicDescription& y)
{
	UInt32 xFlags = x.mFormatFlags;
	UInt32 yFlags = y.mFormatFlags;
	
	// match wildcards
	if (x.mFormatID == 0 || y.mFormatID == 0 || xFlags == 0 || yFlags == 0) 
		return true;
	
	if (x.mFormatID == kAudioFormatLinearPCM)
	{		 		
		// knock off the all clear flag
		xFlags = xFlags & ~kAudioFormatFlagsAreAllClear;
		yFlags = yFlags & ~kAudioFormatFlagsAreAllClear;
	
		// if both kAudioFormatFlagIsPacked bits are set, then we don't care about the kAudioFormatFlagIsAlignedHigh bit.
		if (xFlags & yFlags & kAudioFormatFlagIsPacked) {
			xFlags = xFlags & ~kAudioFormatFlagIsAlignedHigh;
			yFlags = yFlags & ~kAudioFormatFlagIsAlignedHigh;
		}
		
		// if both kAudioFormatFlagIsFloat bits are set, then we don't care about the kAudioFormatFlagIsSignedInteger bit.
		if (xFlags & yFlags & kAudioFormatFlagIsFloat) {
			xFlags = xFlags & ~kAudioFormatFlagIsSignedInteger;
			yFlags = yFlags & ~kAudioFormatFlagIsSignedInteger;
		}
		
		//	if the bit depth is 8 bits or less and the format is packed, we don't care about endianness
		if((x.mBitsPerChannel <= 8) && ((xFlags & kAudioFormatFlagIsPacked) == kAudioFormatFlagIsPacked))
		{
			xFlags = xFlags & ~kAudioFormatFlagIsBigEndian;
		}
		if((y.mBitsPerChannel <= 8) && ((yFlags & kAudioFormatFlagIsPacked) == kAudioFormatFlagIsPacked))
		{
			yFlags = yFlags & ~kAudioFormatFlagIsBigEndian;
		}
		
		//	if the number of channels is 0 or 1, we don't care about non-interleavedness
		if (x.mChannelsPerFrame <= 1 && y.mChannelsPerFrame <= 1) {
			xFlags &= ~kLinearPCMFormatFlagIsNonInterleaved;
			yFlags &= ~kLinearPCMFormatFlagIsNonInterleaved;
		}
	}
	return xFlags == yFlags;
}

Boolean FormatIsEqual(AudioStreamBasicDescription x, AudioStreamBasicDescription y)
{
	//	the semantics for equality are:
	//		1) Values must match exactly
	//		2) wildcard's are ignored in the comparison
	
#define MATCH(name) ((x.name) == 0 || (y.name) == 0 || (x.name) == (y.name))
	
	return 
		((x.mSampleRate==0.) || (y.mSampleRate==0.) || (x.mSampleRate==y.mSampleRate)) 
		&& MATCH(mFormatID)
		&& MatchFormatFlags(x, y)  
		&& MATCH(mBytesPerPacket) 
		&& MATCH(mFramesPerPacket) 
		&& MATCH(mBytesPerFrame) 
		&& MATCH(mChannelsPerFrame) 		
		&& MATCH(mBitsPerChannel) ;
}

#pragma mark ***** BackgroundTrackMgr *****
//==================================================================================================
//	BackgroundTrackMgr class
//==================================================================================================
class BackgroundTrackMgr
{	
	#define CurFileInfo THIS->mBGFileInfo[THIS->mCurrentFileIndex]
	public:
		typedef struct BG_FileInfo {
			char*						mFilePath;
			AudioFileID						mAFID;
			AudioStreamBasicDescription		mFileFormat;
			UInt64							mFileDataSize;
			//UInt64							mFileNumPackets; // this is only used if loading file to memory
			Boolean							mLoadAtOnce;
			Boolean							mFileDataInQueue;
		} BackgroundMusicFileInfo;
		
		BackgroundTrackMgr() 
			:	mQueue(0),
				mBufferByteSize(0),
				mCurrentPacket(0),
				mNumPacketsToRead(0),
				mVolume(1.0),
				mPacketDescs(NULL),
				mCurrentFileIndex(0),
				mMakeNewQueueWhenStopped(false),
				mStopAtEnd(false),
				mStopped(false){ }
		
		~BackgroundTrackMgr() { Teardown(); }

		void Teardown()
		{
			if (mQueue)
				AudioQueueDispose(mQueue, true);
			for (UInt32 i=0; i < mBGFileInfo.size(); i++)
				if (mBGFileInfo[i]->mAFID)
					AudioFileClose(mBGFileInfo[i]->mAFID);

			if (mPacketDescs)
				delete mPacketDescs;
		}
		
		AudioStreamPacketDescription *GetPacketDescsPtr() { return mPacketDescs; }
		
		UInt32 GetNumPacketsToRead(BackgroundTrackMgr::BG_FileInfo *inFileInfo) 
		{ 
			return mNumPacketsToRead; 
		}

		static OSStatus AttachNewCookie(AudioQueueRef inQueue, BackgroundTrackMgr::BG_FileInfo *inFileInfo)
		{
			OSStatus result = noErr;
			UInt32 size = sizeof(UInt32);
			result = AudioFileGetPropertyInfo (inFileInfo->mAFID, kAudioFilePropertyMagicCookieData, &size, NULL);
			if (!result && size) 
			{
				char* cookie = new char [size];		
				result = AudioFileGetProperty (inFileInfo->mAFID, kAudioFilePropertyMagicCookieData, &size, cookie);
					AssertNoError("Error getting cookie data", end);
				result = AudioQueueSetProperty(inQueue, kAudioQueueProperty_MagicCookie, cookie, size);
				delete [] cookie;
					AssertNoError("Error setting cookie data for queue", end);
			}
			return noErr;
		
		end:
			return noErr;
		}

		static void QueueStoppedProc(	void *                  inUserData,
										AudioQueueRef           inAQ,
										AudioQueuePropertyID    inID)
		{
			UInt32 isRunning;
			UInt32 propSize = sizeof(isRunning);

			BackgroundTrackMgr *THIS = (BackgroundTrackMgr*)inUserData;
			OSStatus result = AudioQueueGetProperty(inAQ, kAudioQueueProperty_IsRunning, &isRunning, &propSize);
				
			if ((!isRunning) && (THIS->mMakeNewQueueWhenStopped))
			{
				result = AudioQueueDispose(inAQ, true);
					AssertNoError("Error disposing queue", end);
				result = THIS->SetupQueue(CurFileInfo);
					AssertNoError("Error setting up new queue", end);
				result = THIS->SetupBuffers(CurFileInfo);
					AssertNoError("Error setting up new queue buffers", end);
				result = THIS->Start();
					AssertNoError("Error starting queue", end);
			}
		end:
			return;
		}
		
		static Boolean DisposeBuffer(AudioQueueRef inAQ, std::vector<AudioQueueBufferRef> inDisposeBufferList, AudioQueueBufferRef inBufferToDispose)
		{
			for (unsigned int i=0; i < inDisposeBufferList.size(); i++)
			{
				if (inBufferToDispose == inDisposeBufferList[i])
				{
					OSStatus result = AudioQueueFreeBuffer(inAQ, inBufferToDispose);
					if (result == noErr)
						inDisposeBufferList.pop_back();
					return true;
				}
			}
			return false;
		}
		
		enum {
			kQueueState_DoNothing		= 0,
			kQueueState_ResizeBuffer	= 1,
			kQueueState_NeedNewCookie	= 2,
			kQueueState_NeedNewBuffers	= 3,
			kQueueState_NeedNewQueue	= 4,
		};
		
		static SInt8 GetQueueStateForNextBuffer(BackgroundTrackMgr::BG_FileInfo *inFileInfo, BackgroundTrackMgr::BG_FileInfo *inNextFileInfo)
		{
			inFileInfo->mFileDataInQueue = false;
			
			// unless the data formats are the same, we need a new queue
			if (!FormatIsEqual(inFileInfo->mFileFormat, inNextFileInfo->mFileFormat))
				return kQueueState_NeedNewQueue;
				
			// if going from a load-at-once file to streaming or vice versa, we need new buffers
			if (inFileInfo->mLoadAtOnce != inNextFileInfo->mLoadAtOnce)
				return kQueueState_NeedNewBuffers;
			
			// if the next file is smaller than the current, we just need to resize
			if (inNextFileInfo->mLoadAtOnce)
				return (inFileInfo->mFileDataSize >= inNextFileInfo->mFileDataSize) ? kQueueState_ResizeBuffer : kQueueState_NeedNewBuffers;
				
			return kQueueState_NeedNewCookie;
		}
		
		static void QueueCallback(	void *					inUserData,
									AudioQueueRef			inAQ,
									AudioQueueBufferRef		inCompleteAQBuffer) 
		{
			// dispose of the buffer if no longer in use
			OSStatus result = noErr;
			BackgroundTrackMgr *THIS = (BackgroundTrackMgr*)inUserData;
			if (DisposeBuffer(inAQ, THIS->mBuffersToDispose, inCompleteAQBuffer))
				return;
			
			if (THIS->mStopped){
				return;
			}
			
			UInt32 nPackets = 0;
			// loop the current buffer if the following:
			// 1. file was loaded into the buffer previously
			// 2. only one file in the queue
			// 3. we have not been told to stop at playlist completion
			if ((CurFileInfo->mFileDataInQueue) && (THIS->mBGFileInfo.size() == 1) && (!THIS->mStopAtEnd))
				nPackets = THIS->GetNumPacketsToRead(CurFileInfo);

			else
			{
				UInt32 numBytes;
				while (nPackets == 0)
				{
					// if loadAtOnce, get all packets in the file, otherwise ~.5 seconds of data
					nPackets = THIS->GetNumPacketsToRead(CurFileInfo);					
					result = AudioFileReadPackets(CurFileInfo->mAFID, false, &numBytes, THIS->mPacketDescs, THIS->mCurrentPacket, &nPackets, 
											inCompleteAQBuffer->mAudioData);
						AssertNoError("Error reading file data", end);
					
					inCompleteAQBuffer->mAudioDataByteSize = numBytes;	
											
					if (nPackets == 0) // no packets were read, this file has ended.
					{
						if (CurFileInfo->mLoadAtOnce)
							CurFileInfo->mFileDataInQueue = true;
						
						THIS->mCurrentPacket = 0;
						UInt32 theNextFileIndex = (THIS->mCurrentFileIndex < THIS->mBGFileInfo.size()-1) ? THIS->mCurrentFileIndex+1 : 0;
						
						// we have gone through the playlist. if mStopAtEnd, stop the queue here
						if (theNextFileIndex == 0 && THIS->mStopAtEnd)
						{
							THIS->mStopped = true;
							result = AudioQueueStop(inAQ, false);
								AssertNoError("Error stopping queue", end);
							return;
						}
						
						SInt8 theQueueState = GetQueueStateForNextBuffer(CurFileInfo, THIS->mBGFileInfo[theNextFileIndex]);
						if (theNextFileIndex != THIS->mCurrentFileIndex)
						{
							// if were are not looping the same file. Close the old one and open the new
							result = AudioFileClose(CurFileInfo->mAFID);
								AssertNoError("Error closing file", end);
							THIS->mCurrentFileIndex = theNextFileIndex;

							result = LoadFileDataInfo(CurFileInfo->mFilePath, CurFileInfo->mAFID, CurFileInfo->mFileFormat, CurFileInfo->mFileDataSize);
								AssertNoError("Error opening file", end);
						}
						
						switch (theQueueState) 
						{							
							// if we need to resize the buffer, set the buffer's audio data size to the new file's size
							// we will also need to get the new file cookie
							case kQueueState_ResizeBuffer:
								inCompleteAQBuffer->mAudioDataByteSize = CurFileInfo->mFileDataSize;							
							// if the data format is the same but we just need a new cookie, attach a new cookie
							case kQueueState_NeedNewCookie:
								result = AttachNewCookie(inAQ, CurFileInfo);
									AssertNoError("Error attaching new file cookie data to queue", end);
								break;
							
							// we can keep the same queue, but not the same buffer(s)
							case kQueueState_NeedNewBuffers:
								THIS->mBuffersToDispose.push_back(inCompleteAQBuffer);
								THIS->SetupBuffers(CurFileInfo);
								break;
							
							// if the data formats are not the same, we need to dispose the current queue and create a new one
							case kQueueState_NeedNewQueue:
								THIS->mMakeNewQueueWhenStopped = true;
								result = AudioQueueStop(inAQ, false);
									AssertNoError("Error stopping queue", end);
								return;
								
							default:
								break;
						}
					}
				}
			}
			
			result = AudioQueueEnqueueBuffer(inAQ, inCompleteAQBuffer, (THIS->mPacketDescs ? nPackets : 0), THIS->mPacketDescs);
				AssertNoError("Error enqueuing new buffer", end);
			if (CurFileInfo->mLoadAtOnce)
				CurFileInfo->mFileDataInQueue = true;
				
			THIS->mCurrentPacket += nPackets;
		
		end:
			return;
		}
		
		OSStatus SetupQueue(BG_FileInfo *inFileInfo)
		{
			UInt32 size = 0;
			OSStatus result = AudioQueueNewOutput(&inFileInfo->mFileFormat, QueueCallback, this, CFRunLoopGetCurrent(), kCFRunLoopCommonModes, 0, &mQueue);
			if(result != noErr)
			{
				printf("%s: %d\n", "Error creating queue", (int)result);
				return result;
			}
			
			// (2) If the file has a cookie, we should get it and set it on the AQ
			size = sizeof(UInt32);
			result = AudioFileGetPropertyInfo (inFileInfo->mAFID, kAudioFilePropertyMagicCookieData, &size, NULL);

			if (!result && size) {
				char* cookie = new char [size];		
				result = AudioFileGetProperty (inFileInfo->mAFID, kAudioFilePropertyMagicCookieData, &size, cookie);
				if(result != noErr)
				{
					printf("%s: %d\n", "Error getting magic cookie", (int)result);
					return result;
				}
				
				result = AudioQueueSetProperty(mQueue, kAudioQueueProperty_MagicCookie, cookie, size);
				delete [] cookie;
				if(result != noErr)
				{
					printf("%s: %d\n", "Error setting magic cookie", (int)result);
					return result;
				}
			}

			// channel layout
			OSStatus err = AudioFileGetPropertyInfo(inFileInfo->mAFID, kAudioFilePropertyChannelLayout, &size, NULL);
			if (err == noErr && size > 0) {
				AudioChannelLayout *acl = (AudioChannelLayout *)malloc(size);
				result = AudioFileGetProperty(inFileInfo->mAFID, kAudioFilePropertyChannelLayout, &size, acl);
				if(result != noErr)
				{
					printf("%s: %d\n", "Error getting channel layout from file", (int)result);
					return result;
				}
				
				result = AudioQueueSetProperty(mQueue, kAudioQueueProperty_ChannelLayout, acl, size);
				free(acl);
				if(result != noErr)
				{
					printf("%s: %d\n", "Error setting channel layout on queue", (int)result);
					return result;
				}
			}
			
			// add a notification proc for when the queue stops
			result = AudioQueueAddPropertyListener(mQueue, kAudioQueueProperty_IsRunning, QueueStoppedProc, this);
			if(result != noErr)
			{
				printf("%s: %d\n", "Error adding isRunning property listener to queue", (int)result);
				return result;
			}
			
			// we need to reset this variable so that if the queue is stopped mid buffer we don't dispose it 
			mMakeNewQueueWhenStopped = false;
			
			// volume
			result = SetVolume(mVolume);
			
		end:
			return result;
		}

		OSStatus SetupBuffers(BG_FileInfo *inFileInfo)
		{
			int numBuffersToQueue = kNumberBuffers;
			UInt32 maxPacketSize;
			UInt32 size = sizeof(maxPacketSize);
			// we need to calculate how many packets we read at a time, and how big a buffer we need
			// we base this on the size of the packets in the file and an approximate duration for each buffer
				
			// first check to see what the max size of a packet is - if it is bigger
			// than our allocation default size, that needs to become larger
			OSStatus result = AudioFileGetProperty(inFileInfo->mAFID, kAudioFilePropertyPacketSizeUpperBound, &size, &maxPacketSize);
			if(result != noErr)
			{
				printf("%s: %d\n", "Error getting packet upper bound size", (int)result);
				return result;
			}
			
			bool isFormatVBR = (inFileInfo->mFileFormat.mBytesPerPacket == 0 || inFileInfo->mFileFormat.mFramesPerPacket == 0);

			CalculateBytesForTime(inFileInfo->mFileFormat, maxPacketSize, 0.5/*seconds*/, &mBufferByteSize, &mNumPacketsToRead);
			
			// if the file is smaller than the capacity of all the buffer queues, always load it at once
			if ((mBufferByteSize * numBuffersToQueue) > inFileInfo->mFileDataSize)
				inFileInfo->mLoadAtOnce = true;
				
			if (inFileInfo->mLoadAtOnce)
			{
				UInt64 theFileNumPackets;
				size = sizeof(UInt64);
				result = AudioFileGetProperty(inFileInfo->mAFID, kAudioFilePropertyAudioDataPacketCount, &size, &theFileNumPackets);
					AssertNoError("Error getting packet count for file", end);
				
				mNumPacketsToRead = (UInt32)theFileNumPackets;
				mBufferByteSize = inFileInfo->mFileDataSize;
				numBuffersToQueue = 1;
			}	
			else
			{
				mNumPacketsToRead = mBufferByteSize / maxPacketSize;
			}
			
			if (isFormatVBR)
				mPacketDescs = new AudioStreamPacketDescription [mNumPacketsToRead];
			else
				mPacketDescs = NULL; // we don't provide packet descriptions for constant bit rate formats (like linear PCM)	
				
			// allocate the queue's buffers
			for (int i = 0; i < numBuffersToQueue; ++i) 
			{
				result = AudioQueueAllocateBuffer(mQueue, mBufferByteSize, &mBuffers[i]);
					AssertNoError("Error allocating buffer for queue", end);
				QueueCallback (this, mQueue, mBuffers[i]);
				if (inFileInfo->mLoadAtOnce)
					inFileInfo->mFileDataInQueue = true;
			}
		
		end:
			return result;
		}
		
		OSStatus LoadTrack(const char* inFilePath, Boolean inAddToQueue, Boolean inLoadAtOnce)
		{
			BG_FileInfo *fileInfo = new BG_FileInfo;
			fileInfo->mFilePath = (char *)malloc(strlen(inFilePath)+1);
			strcpy(fileInfo->mFilePath, inFilePath);
			
			OSStatus result = LoadFileDataInfo(fileInfo->mFilePath, fileInfo->mAFID, fileInfo->mFileFormat, fileInfo->mFileDataSize);
				AssertNoError("Error getting file data info", fail);
			fileInfo->mLoadAtOnce = inLoadAtOnce;
			fileInfo->mFileDataInQueue = false;
			// if not adding to the queue, clear the current file vector
			if (!inAddToQueue)
				mBGFileInfo.clear();
				
			mBGFileInfo.push_back(fileInfo);
			
			// setup the queue if this is the first (or only) file
			if (mBGFileInfo.size() == 1)
			{
				result = SetupQueue(fileInfo);
					AssertNoError("Error setting up queue", fail);
				result = SetupBuffers(fileInfo);
					AssertNoError("Error setting up queue buffers", fail);					
			}
			// if this is just part of the playlist, close the file for now
			else
			{
				result = AudioFileClose(fileInfo->mAFID);
					AssertNoError("Error closing file", fail);
			}	
			return result;
		
		fail:
			if (fileInfo)
				delete fileInfo;
			return result;
		}
		
		OSStatus UpdateGain()
		{
			return SetVolume(mVolume);
		}
		
		OSStatus SetVolume(Float32 inVolume)
		{
			mVolume = inVolume;
			return AudioQueueSetParameter(mQueue, kAudioQueueParam_Volume, mVolume * gMasterVolumeGain);
		}
		
		OSStatus Start()
		{
			
			OSStatus result = AudioQueuePrime(mQueue, 1, NULL);	
			if (result)
			{
				mStopped = true;
				printf("Error priming queue");
				return result;
			}
			mStopped = false;
			return AudioQueueStart(mQueue, NULL);
		}
		
		OSStatus Stop(Boolean inStopAtEnd)
		{
			if (inStopAtEnd)
			{
				mStopAtEnd = true;
				return noErr;
			}
			else{
				mStopped = true;
				return AudioQueueStop(mQueue, true);
			}
		}
	
	private:
		AudioQueueRef						mQueue;
		AudioQueueBufferRef					mBuffers[kNumberBuffers];
		UInt32								mBufferByteSize;
		SInt64								mCurrentPacket;
		UInt32								mNumPacketsToRead;
		Float32								mVolume;
		AudioStreamPacketDescription *		mPacketDescs;
		std::vector<BG_FileInfo*>			mBGFileInfo;
		UInt32								mCurrentFileIndex;
		Boolean								mMakeNewQueueWhenStopped;
		Boolean								mStopAtEnd;
		Boolean								mStopped;
		std::vector<AudioQueueBufferRef>	mBuffersToDispose;
};

#pragma mark ***** SoundEngineEffect *****
//==================================================================================================
//	SoundEngineEffect class
//==================================================================================================
class SoundEngineEffect
{
	public:	
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		SoundEngineEffect(const char* inPath) 
			:	
				mBufferID(0),
				mPath(inPath),
				mData(NULL),
				mDataSize(0)
			{ }
		
		~SoundEngineEffect()
		{			
			if (mData)
				free(mData);
		}
		
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// Accessors
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		UInt32	GetEffectID() { return mBufferID; }		

		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// Helper Functions
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		ALenum GetALFormat(AudioStreamBasicDescription inFileFormat)
		{
			if (inFileFormat.mFormatID != kAudioFormatLinearPCM)
				return kSoundEngineErrInvalidFileFormat;
				
			if ((inFileFormat.mChannelsPerFrame > 2) || (inFileFormat.mChannelsPerFrame < 1))
				return kSoundEngineErrInvalidFileFormat;
			
			if(inFileFormat.mBitsPerChannel == 8)
				return (inFileFormat.mChannelsPerFrame == 1) ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8;
			else if(inFileFormat.mBitsPerChannel == 16)
				return (inFileFormat.mChannelsPerFrame == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

			return kSoundEngineErrInvalidFileFormat;
		}

		OSStatus LoadFileData(const char *inFilePath, void* &outData, UInt32 &outDataSize, ALuint &outBufferID)
		{
			AudioFileID theAFID = 0;
			OSStatus result = noErr;
			UInt64 theFileSize = 0;
			AudioStreamBasicDescription theFileFormat;
			
			result = LoadFileDataInfo(inFilePath, theAFID, theFileFormat, theFileSize);
			outDataSize = (UInt32)theFileSize;
				AssertNoError("Error loading file info", fail)

			outData = malloc(outDataSize);

			result = AudioFileReadBytes(theAFID, false, 0, &outDataSize, outData);
				AssertNoError("Error reading file data", fail)
				
			if (!TestAudioFormatNativeEndian(theFileFormat) && (theFileFormat.mBitsPerChannel > 8)) 
				return kSoundEngineErrInvalidFileFormat;
		
			alGenBuffers(1, &outBufferID);
			/*ALenum			error;
			if((error = alGetError()) != AL_NO_ERROR) {
				printf("Error Generating Buffers: %x", error);
				exit(1);
			}*/
				AssertNoOALError("Error generating buffer\n", fail);
			
			alBufferDataStaticProc(outBufferID, GetALFormat(theFileFormat), outData, outDataSize, theFileFormat.mSampleRate);
				AssertNoOALError("Error attaching data to buffer\n", fail);

			AudioFileClose(theAFID);
			return result;
			
		fail:			
			if (theAFID)
				AudioFileClose(theAFID);
			if (outData)
			{
				free(outData);
				outData = NULL;
			}
			return result;
		}
		
		OSStatus initialize()
		{
			OSStatus result = AL_NO_ERROR;			

			result = LoadFileData(mPath, mData, mDataSize, mBufferID);
				AssertNoError("Error loading sound file info", end)

		end:
			return result;
		}

	private:
		ALuint					mBufferID;
		const char*				mPath;
		void*					mData;
		UInt32					mDataSize;
};

#pragma mark ***** SoundEngineEffectMap *****
//==================================================================================================
//	SoundEngineEffectMap class
//==================================================================================================
class SoundEngineEffectMap 
	: std::multimap<UInt32, SoundEngineEffect*, std::less<ALuint> > 
{
	public:
    // add a new context to the map
    void Add (const	ALuint	inEffectToken, SoundEngineEffect **inEffect)
	{
		iterator	it = upper_bound(inEffectToken);
		insert(it, value_type (inEffectToken, *inEffect));
	}

    SoundEngineEffect* Get(ALuint	inEffectToken) 
	{
        iterator	it = find(inEffectToken);
        if (it != end())
            return ((*it).second);
		return (NULL);
    }

    void Remove (const	ALuint	inSourceToken) {
        iterator 	it = find(inSourceToken);
        if (it != end())
            erase(it);
    }
	
    SoundEngineEffect* GetEffectByIndex(UInt32	inIndex) {
        iterator	it = begin();

		for (UInt32 i = 0; i < inIndex; i++) {
            if (it != end())
                ++it;
            else
                i = inIndex;
        }
        
        if (it != end())
            return ((*it).second);		
		return (NULL);
    }

	iterator GetIterator() { return begin(); }
	
    UInt32 Size () const { return size(); }
    bool Empty () const { return empty(); }
};

#pragma mark ***** OpenALObject *****
//==================================================================================================
//	OpenALObject class
//==================================================================================================
class OpenALObject
{	
	public:	
		OpenALObject(Float32 inMixerOutputRate)
			:	mOutputRate(inMixerOutputRate),
				mGain(1.0),
				mContext(NULL),
				mDevice(NULL),
				mEffectsMap(NULL) 
		{
			mEffectsMap = new SoundEngineEffectMap();
		}
		
		~OpenALObject() { Teardown(); }

		OSStatus Initialize()
		{
			OSStatus result = noErr;
			mDevice = alcOpenDevice(NULL);
				AssertNoOALError("Error opening output device", end)
			if(mDevice == NULL) { return kSoundEngineErrDeviceNotFound; }
			
			// if a mixer output rate was specified, set it here
			// must be done before the alcCreateContext() call
			if (mOutputRate)
				alcMacOSXMixerOutputRateProc(mOutputRate);
			
			// Create an OpenAL Context
			mContext = alcCreateContext(mDevice, NULL);
				AssertNoOALError("Error creating OpenAL context", end)
			
			alcMakeContextCurrent(mContext);
				AssertNoOALError("Error setting current OpenAL context", end)
			
			alGenSources(MAX_SOURCES, mSourceID); 
				AssertNoOALError("Error generating sources", end)
			
			for (int i = 0; i < MAX_SOURCES; ++i){
				mSourcePrimed[i] = FALSE;
			}
			 
		end:
			return result;
		}
		
		void Teardown()
		{
			if (mEffectsMap) {
				// [FIXED] In old FOR loop, Remove() will decrease Size(), but variable i will increase whenever
				while (mEffectsMap->Size()){
					SoundEngineEffect *theEffect = mEffectsMap->GetEffectByIndex(0);
					if (theEffect){
						mEffectsMap->Remove(theEffect->GetEffectID());
						delete theEffect;
					}
				}
				delete mEffectsMap;
			}
			
			// [FIXED] alGenSources() created sources should be deleted.
			alDeleteSources(MAX_SOURCES, mSourceID);
			
			if (mContext){
				alcMakeContextCurrent(NULL);
				alcDestroyContext(mContext);
			}
			
			if (mDevice) alcCloseDevice(mDevice);
		}

		OSStatus SetListenerPosition(Float32 inX, Float32 inY, Float32 inZ)
		{
			alListener3f(AL_POSITION, inX, inY, inZ);
			return alGetError();
		}

		OSStatus SetListenerGain(Float32 inValue)
		{
			alListenerf(AL_GAIN, inValue);
			return alGetError();
		}
		
		OSStatus SetMaxDistance(Float32 inValue)
		{
			OSStatus result = 0;
			for (UInt32 i=0; i < MAX_SOURCES; i++)
			{
				alSourcef(mSourceID[i], AL_MAX_DISTANCE, inValue);

				if ((result = alGetError()) != AL_NO_ERROR)
					return result;
			}
			return result;			
		}

	
		OSStatus SetReferenceDistance(Float32 inValue)
		{
			OSStatus result = 0;
			for (UInt32 i=0; i < MAX_SOURCES; i++)
			{
				alSourcef(mSourceID[i], AL_REFERENCE_DISTANCE, inValue);
				
				if ((result = alGetError()) != AL_NO_ERROR)
					return result;
			}
			return result;		
		}

	
		OSStatus SetEffectsVolume(Float32 inValue)
		{
			OSStatus result = 0;
			for (UInt32 i=0; i < MAX_SOURCES; i++)
			{
				alSourcef(mSourceID[i], AL_GAIN, inValue * gMasterVolumeGain);
				
				if ((result = alGetError()) != AL_NO_ERROR)
					return result;
			}
			return result;		
		}
		
	
		OSStatus UpdateGain()
		{
			return SetEffectsVolume(mGain);
		}
						
		OSStatus LoadEffect(const char *inFilePath, UInt32 *outEffectID)
		{
			SoundEngineEffect *theEffect = new SoundEngineEffect(inFilePath);
			OSStatus result = theEffect->initialize();
			if (result == noErr)
			{
				*outEffectID = theEffect->GetEffectID();
				mEffectsMap->Add(*outEffectID, &theEffect);
			}
			return result;
		}
				
		OSStatus UnloadEffect(UInt32 inEffectID)
		{
			// [FIXED] SoundEngineEffect should be deleted before remove from the map
			SoundEngineEffect *theEffect = mEffectsMap->Get(inEffectID);
			if (theEffect){
				delete theEffect;
			}
			mEffectsMap->Remove(inEffectID);
			return 0;
		}


		OSStatus PrimeEffect(UInt32 inEffectID, ALuint *sourceID)
		{
// JM hack!
// Usar el recurso que esté libre, nada de el primero que esté libre o parado (código original)
            for (int i = 0; i < MAX_SOURCES; ++i){
				if (! mSourcePrimed[i]){
					mSourcePrimed[i] = TRUE;
					alSourcei(mSourceID[i], AL_BUFFER, inEffectID);
					*sourceID = mSourceID[i];
					return alGetError();
				}
            }

/*            
			// Locate an available source.  We consider a source available if it is either not primed or is in the stoppped state
			for (int i = 0; i < MAX_SOURCES; ++i){
				if (! mSourcePrimed[i]){
					mSourcePrimed[i] = TRUE;
					alSourcei(mSourceID[i], AL_BUFFER, inEffectID);
					*sourceID = mSourceID[i];
					return alGetError();
				}
				
				ALint sourceState;
				alGetSourcei(mSourceID[i], AL_SOURCE_STATE, &sourceState);
				if (sourceState == AL_STOPPED){
					alSourcei(mSourceID[i], AL_BUFFER, inEffectID);
					*sourceID = mSourceID[i];
					return 0;
				}
			}
*/
				

			// If we fell through the loop, then no available sources
			return kSoundEngineErrNoSourcesAvailable;
		}
	

		OSStatus StartEffect(ALuint sourceID)
		{
			alSourcePlay(sourceID);
			return alGetError();
		}
	
	
		OSStatus StopEffect(ALuint sourceID)
		{
			alSourceStop(sourceID);
			return alGetError();
		}
		
		OSStatus SetEffectPitch(ALuint sourceID, Float32 inValue)
		{
			alSourcef(sourceID, AL_PITCH, inValue);
			return alGetError();
		}

		OSStatus SetEffectVolume(ALuint sourceID, Float32 inValue)
		{
			alSourcef(sourceID, AL_GAIN, inValue * gMasterVolumeGain);
			return alGetError();
		}
				
		OSStatus	SetEffectPosition(ALuint sourceID, Float32 inX, Float32 inY, Float32 inZ)	
		{
			alSource3f(sourceID, AL_POSITION, inX, inY, inZ);
			return alGetError();
		}
				
	private:
		Float32									mOutputRate;
		Float32									mGain;
		ALCcontext*								mContext;
		ALCdevice*								mDevice;
		SoundEngineEffectMap*					mEffectsMap;
		ALuint									mSourceID[MAX_SOURCES];
		bool									mSourcePrimed[MAX_SOURCES];
};

#pragma mark ***** API *****
//==================================================================================================
//	Sound Engine
//==================================================================================================

extern "C"
OSStatus  SoundEngine_Initialize(Float32 inMixerOutputRate)
{
	if (sOpenALObject)
		delete sOpenALObject;

	for (int i = 0; i < kBackgroundMusicSlots; ++i) {
		if (sBackgroundTrackMgr[i]) {
			delete sBackgroundTrackMgr[i];
			sBackgroundTrackMgr[i] = NULL;
		}
	}

	sOpenALObject = new OpenALObject(inMixerOutputRate);	
	for (int i = 0; i < kBackgroundMusicSlots; ++i)
		sBackgroundTrackMgr[i] = new BackgroundTrackMgr();
	
	return sOpenALObject->Initialize();
	
}

extern "C"
OSStatus  SoundEngine_Teardown()
{
	if (sOpenALObject)
	{
		delete sOpenALObject;
		sOpenALObject = NULL;
	}
	
	for (int i = 0; i < kBackgroundMusicSlots; ++i) {
		if (sBackgroundTrackMgr[i])
			delete sBackgroundTrackMgr[i];
	}
	
	for (int i = 0; i < kBackgroundMusicSlots; ++i) {
		if (sBackgroundTrackMgr[i]) {
			delete sBackgroundTrackMgr[i];
			sBackgroundTrackMgr[i] = NULL;
		}
	}
	
	return 0; 
}

extern "C"
OSStatus  SoundEngine_SetMasterVolume(int slot, Float32 inValue)
{
	OSStatus result = noErr;
	gMasterVolumeGain = inValue;
	if (sBackgroundTrackMgr[slot])
		result = sBackgroundTrackMgr[slot]->UpdateGain();
	
	if (result) return result;
		
	if (sOpenALObject) 
		return sOpenALObject->UpdateGain();
	
	return result;
}

extern "C"
OSStatus  SoundEngine_SetListenerPosition(Float32 inX, Float32 inY, Float32 inZ)
{	
	return (sOpenALObject) ? sOpenALObject->SetListenerPosition(inX, inY, inZ) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_SetListenerGain(Float32 inValue)
{
	return (sOpenALObject) ? sOpenALObject->SetListenerGain(inValue) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_LoadBackgroundMusicTrack(int slot, const char* inPath, Boolean inAddToQueue, Boolean inLoadAtOnce)
{
	if (sBackgroundTrackMgr[slot] == NULL)
		sBackgroundTrackMgr[slot] = new BackgroundTrackMgr();
	return sBackgroundTrackMgr[slot]->LoadTrack(inPath, inAddToQueue, inLoadAtOnce);
}

extern "C"
OSStatus  SoundEngine_UnloadBackgroundMusicTrack(int slot)
{
	if (sBackgroundTrackMgr[slot])
	{
		delete sBackgroundTrackMgr[slot];
		sBackgroundTrackMgr[slot] = NULL;
	}
		
	return 0;
}

extern "C"
OSStatus  SoundEngine_StartBackgroundMusic(int slot)
{
	return (sBackgroundTrackMgr[slot]) ? sBackgroundTrackMgr[slot]->Start() : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_StopBackgroundMusic(int slot, Boolean stopAtEnd)
{
	return (sBackgroundTrackMgr[slot]) ?  sBackgroundTrackMgr[slot]->Stop(stopAtEnd) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_SetBackgroundMusicVolume(int slot, Float32 inValue)
{
	return (sBackgroundTrackMgr[slot]) ? sBackgroundTrackMgr[slot]->SetVolume(inValue) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_LoadEffect(const char* inPath, UInt32* outEffectID)
{
	OSStatus result = noErr;
	if (sOpenALObject == NULL)
	{
		sOpenALObject = new OpenALObject(0.0);
		result = sOpenALObject->Initialize();
	}	
	return (result) ? result : sOpenALObject->LoadEffect(inPath, outEffectID);
}


extern "C"
OSStatus  SoundEngine_UnloadEffect(UInt32 inEffectID)
{
	return (sOpenALObject) ? sOpenALObject->UnloadEffect(inEffectID) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_PrimeEffect(UInt32 inEffectID, ALuint *sourceID)
{
	return (sOpenALObject) ? sOpenALObject->PrimeEffect(inEffectID, sourceID) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_StartEffect(ALuint sourceID)
{
	return (sOpenALObject) ? sOpenALObject->StartEffect(sourceID) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_StopEffect(ALuint sourceID)
{	
	return (sOpenALObject) ?  sOpenALObject->StopEffect(sourceID) : kSoundEngineErrUnitialized;
}
		
extern "C"
OSStatus  SoundEngine_SetEffectPitch(ALuint sourceID, Float32 inValue)
{
	return (sOpenALObject) ? sOpenALObject->SetEffectPitch(sourceID, inValue) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_SetEffectLevel(ALuint sourceID, Float32 inValue)
{
	return (sOpenALObject) ? sOpenALObject->SetEffectVolume(sourceID, inValue) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus	SoundEngine_SetEffectPosition(ALuint sourceID, Float32 inX, Float32 inY, Float32 inZ)
{
	return (sOpenALObject) ? sOpenALObject->SetEffectPosition(sourceID, inX, inY, inZ) : kSoundEngineErrUnitialized;	
}

extern "C"
OSStatus  SoundEngine_SetEffectsVolume(Float32 inValue)
{
	return (sOpenALObject) ? sOpenALObject->SetEffectsVolume(inValue) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_SetMaxDistance(Float32 inValue)
{
	return (sOpenALObject) ? sOpenALObject->SetMaxDistance(inValue) : kSoundEngineErrUnitialized;
}

extern "C"
OSStatus  SoundEngine_SetReferenceDistance(Float32 inValue)
{
	return (sOpenALObject) ? sOpenALObject->SetReferenceDistance(inValue) : kSoundEngineErrUnitialized;
}

#endif
#endif
