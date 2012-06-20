//
//  SpeechManager.h
//  SnackGlish
//
//  Created by JM on 15/02/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>



@protocol SpeechManagerDelegate <NSObject>

- (void) speechFinished:(id)soundId;

@end


@class SpeechChunk;


@interface SpeechManager : NSObject<AVAudioPlayerDelegate> {
	NSString* _soundFile;
	AVAudioPlayer *_player;
	NSTimer *_stopTimer;
	NSMutableDictionary *_sounds;
	id _lastSpeechId;
	id<SpeechManagerDelegate> _delegate;
    bool _isFading;
}


@property (nonatomic, retain) id<SpeechManagerDelegate> delegate;


- (id) initWithFile:(NSString *)file;

- (void) addSpeech:(id)speechId withChunk:(SpeechChunk*)chunk;

- (bool) playSpeech:(id)speechId;
- (bool) playSpeech:(id)speechId stoppingPrevious:(bool)stop;
- (void) pause;
- (void) resume;
- (void) stopSpeechId:(id)speechId;
- (void) stopWithFadeStep:(double)step;
- (void) stopWithFade;
- (id) playingId;
- (NSTimeInterval) lengthForSpeech:(id)speechId;



@end
