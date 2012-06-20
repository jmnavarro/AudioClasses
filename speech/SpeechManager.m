//
//  SpeechManager.m
//  SnackGlish
//
//  Created by JM on 15/02/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "SpeechManager.h"
#import "SpeechChunk.h"




@implementation SpeechManager

@synthesize delegate = _delegate;



- (id) initWithFile:(NSString*)file {
	if (self = [super init]) {
		_soundFile = [file retain];
		_sounds = [[NSMutableDictionary alloc] initWithCapacity:8];
	}
	return self;
}


- (void) addSpeech:(id)speechId withChunk:(SpeechChunk*)chunk {
	DDLogVerbose(@"Add speech %@ start=%f len=%f", speechId, chunk->start, chunk->length);
	[_sounds setObject:chunk forKey:speechId];
}


- (void) stopStepWithFadeStep:(NSNumber*)step
{
    if ([_player isPlaying]) {
        if (_player.volume > [step doubleValue]) {
            _player.volume -= [step doubleValue];
            [self performSelector:@selector(stopStepWithFadeStep:) withObject:step afterDelay:0.1];
        } else {
            [self stopSpeechId:_lastSpeechId];
            _player.volume = 1.0;
            [step release];
            _isFading = NO;
        }
    }
}


- (void) stopWithFadeStep:(double)step
{
    if (!_isFading) {
        _isFading = YES;
        [self stopStepWithFadeStep:[[NSNumber alloc] initWithDouble:step]];
    }
}

- (void) stopWithFade
{
    if (!_isFading) {
        _isFading = YES;
        [self stopStepWithFadeStep:[[NSNumber alloc] initWithDouble:0.1f]];
    }
}

/*
- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer *)player successfully:(BOOL)flag
{
}
 */

- (bool) playSpeech:(id)speechId {
	return [self playSpeech:speechId stoppingPrevious:YES];
}


- (void) stopEvent:(NSTimer*) timer {
		//NSLog(@"stop timer for id %@", [_stopTimer userInfo]);
	[self stopSpeechId:[_stopTimer userInfo]];
}

- (bool) invalidateStopTimer {
	bool ret = (_stopTimer != nil);
    //NSLog(@"invalidate timer for id %@", [_stopTimer userInfo]);
    [_stopTimer invalidate];
    _stopTimer = nil;
	return ret;
}


- (void) stopSpeechId:(id)speechId {
	[self invalidateStopTimer];
    [_player stop];
    [_delegate speechFinished:speechId];
}

- (NSTimeInterval) startTimeForSpeech:(id)speechId {
	SpeechChunk* sound = [_sounds objectForKey:speechId];
	return sound ? sound->start : -1.0;
}

- (NSTimeInterval) lengthForSpeech:(id)speechId {
	SpeechChunk* sound = [_sounds objectForKey:speechId];
	return sound ? sound->length : -1.0;
}

- (float) volumeForSpeech:(id)speechId {
	SpeechChunk* sound = [_sounds objectForKey:speechId];
	return sound ? sound->volume : 1.0f;
}

- (void) pause {
	[self invalidateStopTimer];
	[_player pause];
}


- (bool) registerStopTimerAndPlaySpeech:(id)speechId {
	_player.volume = [self volumeForSpeech:speechId];
	bool ret = [_player play];
    if (!ret) DDLogError(@"ERROR playing speech %@", speechId);
	NSTimeInterval length = [self lengthForSpeech:speechId];
		//NSLog(@"play sound %@ with length %f and volume %f", soundId, length, _player.volume); 
    if (length == 0.0) {
        [self stopSpeechId:speechId];
    } else if (length != -1) {
		_stopTimer = [NSTimer scheduledTimerWithTimeInterval: length
													  target: self
													selector: @selector(stopEvent:)
													userInfo: speechId
													 repeats: NO];
	}
	return ret;
}

- (void) resume {
	if (_lastSpeechId) {
		[self registerStopTimerAndPlaySpeech:_lastSpeechId];
	}
}

- (void) createPlayer {
	_player = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:_soundFile] error:nil];
	[_player prepareToPlay];
	[_player play];
	[_player stop];
}

- (id) playingId {
    return [_player isPlaying] ? _lastSpeechId : nil;
}

- (bool) playSpeech:(id)speechId stoppingPrevious:(bool)stop {
    DDLogVerbose(@"playing speech \"%@\"", speechId);
    
	if (!_player) {
		[self createPlayer];		
	} else if ([_player isPlaying] && stop) {
		[self stopSpeechId:_lastSpeechId];
	}
	
	[_lastSpeechId release];
	_lastSpeechId = [speechId retain];
	
	bool ret = NO;
	NSTimeInterval startTime = [self startTimeForSpeech:speechId];
    if (startTime >= _player.duration) {
        DDLogError(@"Sound %@ beyond duration!!", speechId);
    } else if (startTime == -1) {
        DDLogError(@"Speech %@ doesn't found!!", speechId);
    } else {
		_player.currentTime = startTime;
		ret = [self registerStopTimerAndPlaySpeech:speechId];
	}
//	NSLog(@"play page=%d start=%f len=%f stack=%d", _currentPage, startTime, length, _stopSoundStack);

	return ret;
}

- (void) dealloc {
	[self invalidateStopTimer];
	[_player release];
	[_sounds release];
	[_lastSpeechId release];
	[_soundFile release];
	[_delegate release];
	[super dealloc];
}

@end