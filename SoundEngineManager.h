//
//  SoundEngineManager.h
//
//  Created by JM on 03/07/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>


@class AmbientSound;


@interface SoundEngineManager : NSObject {
	bool _initialized;
	NSMutableDictionary *_effects;
	
	AmbientSound *_ambients[2];
}

+ (SoundEngineManager*) sharedManager;
+ (void) vibrate;

- (id) init;

- (void) initialize;

- (void) setEffectsVolume:(double)v;

- (bool) isEffectPrepared:(NSString*)name fromPage:(NSString*)page;

- (void) prepareEffect:(NSString*)name withFile:(NSString*)path fromPage:(NSString*)page inBackground:(bool)background;
- (void) prepareEffect:(NSString*)name withFile:(NSString*)path fromPage:(NSString*)page;
- (void) playEffect:(NSString*)name fromPage:(NSString*)page;
- (void) unloadEffectsFromPage:(NSString*)page;


- (void) loadAndPlayAmbient:(NSString*)path withFadeTime:(double)time inSlot:(int)slot;
- (void) stopAndUnloadAmbientWithFadeTime:(double)fade inSlot:(int)slot;
- (void) replaceCurrentAmbientWith:(NSString*)path withFadeTime:(double)fade inSlot:(int)slot;

- (void) loadAmbient:(NSString*)path inSlot:(int)slot;
- (void) playLoadedAmbientWithFadeTime:(double)time inSlot:(int)slot;


@end
