//
//  AmbientSound.m
//
//  Created by JM on 21/06/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "AmbientSound.h"
#import "SoundEngine.h"


@implementation AmbientSound

@synthesize volume;
@synthesize slot;


- (id) init {
	self = [super init];
	if (self) {
		self.volume = -1.0;

	}
	return self;
}

- (void) unload {
	if (self.volume != -1.0) {
		SoundEngine_StopBackgroundMusic(slot, FALSE);
		SoundEngine_UnloadBackgroundMusicTrack(slot);
		self.volume = -1.0;
	}
}

- (void) completedStopWithFade:(double)fade {
	if (_replacing != nil) {
		[self loadFromPath:_replacing];
		[self playLoadedWithFadeTime:fade];
		[_replacing release];
		_replacing = nil;
	}
}

- (void) stopAndUnloadWithStepDelay:(NSNumber*)delay {
	self.volume -= 0.1;
	SoundEngine_SetBackgroundMusicVolume(self.slot, self.volume);
	if (self.volume <= 0.0) {
		[self unload];
		[self completedStopWithFade:[delay doubleValue]*10.0];
	} else {
		[self performSelector:@selector(stopAndUnloadWithStepDelay:) withObject:delay afterDelay:[delay doubleValue]];
	}
}


- (void) stopAndUnloadWithFadeTime:(double)fade {
	NSLog(@"stop and unload");
	[NSObject cancelPreviousPerformRequestsWithTarget:self];
	if (fade <= 0.0 || self.volume == 0.0) {
		[self unload];
		[self completedStopWithFade:fade];
	} else if (self.volume > 0.0) {
		[self stopAndUnloadWithStepDelay:[NSNumber numberWithDouble:fade/10.0]];
	}
	
}


- (void) loadFromPath:(NSString*)path {
	char buf[512];
	[path getCString:buf maxLength:512 encoding:NSUTF8StringEncoding];
	SoundEngine_LoadBackgroundMusicTrack(self.slot, buf, FALSE, FALSE);
}


- (void) playWithStepDelay:(NSNumber*)delay {
		//	NSLog(@"play: increase volumen from %f", self.volume);
	self.volume += 0.1;
	SoundEngine_SetBackgroundMusicVolume(self.slot, self.volume);
	if (self.volume < 1.0) {
		[self performSelector:@selector(playWithStepDelay:) withObject:delay afterDelay:[delay doubleValue]];
	}
}



- (void) playLoadedWithFadeTime:(double)time {
	if (time == 0.0) {
		SoundEngine_SetBackgroundMusicVolume(slot, 1.0f);
		self.volume = 1.0f;
		SoundEngine_StartBackgroundMusic(slot);
	} else {
		SoundEngine_SetBackgroundMusicVolume(slot, 0.0f);
		self.volume = 0.0f;
		[self playWithStepDelay:[NSNumber numberWithDouble:time/10.0]];
		SoundEngine_StartBackgroundMusic(slot);
	}	
}


- (void) replaceCurrentWith:(NSString*)path withFadeTime:(double)fade {
	if (self.volume == -1.0) {
			// nothing to replace
		[self loadFromPath:path];
		[self playLoadedWithFadeTime:fade];
	} else {
		_replacing = [path retain];
		[self stopAndUnloadWithFadeTime:fade];
	}
}



@end
