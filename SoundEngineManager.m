//
//  SoundEngineManager.m
//
//  Created by JM on 03/07/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "SoundEngineManager.h"
#import "SoundEngine.h"
#import "AmbientSound.h"
#import "macros.h"

static SoundEngineManager *sharedManager;


static const NSString* kNameParam = @"name";
static const NSString* kPathParam = @"path";
static const NSString* kPageParam = @"page";



@implementation SoundEngineManager




+ (SoundEngineManager *) sharedManager
{
	@synchronized ([SoundEngineManager class]) {
		if (!sharedManager) {
			sharedManager = [[SoundEngineManager alloc] init];	
		}
		return sharedManager;
	}
	return nil;
}


+ (id) alloc
{
	@synchronized ([SoundEngineManager class]) {
		NSAssert(sharedManager == nil, @"Can not allocate a second instance of the singleton.");
		sharedManager = [super alloc];
		return sharedManager;
	}
	return nil;
}

- (id) init
{
	if (self = [super init]) {
		_initialized = false;
		_effects = [[NSMutableDictionary alloc] init];
		_ambients[0] = nil;
		_ambients[1] = nil;
	}
	return self;
}

- (void) setEffectsVolume:(double)v
{
	SoundEngine_SetEffectsVolume(v);
}


- (void) initialize
{
	NSLog(@"initilizing sound engine");
	_initialized = true;

	SoundEngine_Initialize(44100);
	SoundEngine_SetListenerPosition(0.0, 0.0, 1.0);
	SoundEngine_SetMasterVolume(0, 1.0);
	SoundEngine_SetMasterVolume(1, 1.0);
	SoundEngine_SetEffectsVolume(1.0);
}

- (bool) isEffectPrepared:(NSString*)name fromPage:(NSString*)page
{
    NSArray *pageComponents = [_effects objectForKey:page];
    NSDictionary *pageEffects = [pageComponents objectAtIndex:0];
    return ([pageEffects objectForKey:name] != nil);
}

- (void) setEffectId:(UInt32)soundId andSourceId:(ALuint)sourceId withName:(NSString*)name fromPage:(NSString*)page
{
	NSMutableDictionary *pageEffects, *pageSources;
	NSArray *pageComponents = [_effects objectForKey:page];
    if (pageComponents) {
        pageEffects = [pageComponents objectAtIndex:0];
        pageSources = [pageComponents objectAtIndex:1];
    } else {
		pageEffects = [[NSMutableDictionary alloc] init];
        pageSources = [[NSMutableDictionary alloc] init];
        pageComponents = [NSArray arrayWithObjects:pageEffects, pageSources, nil];
		[_effects setObject:pageComponents forKey:page];
        [pageEffects release];
        [pageSources release];
    }

	NSNumber *sid = [[NSNumber alloc] initWithUnsignedLong:soundId];
	[pageEffects setObject:sid forKey:name];
	[sid release];

	sid = [[NSNumber alloc] initWithUnsignedInt:sourceId];
	[pageSources setObject:sid forKey:name];
	[sid release];
}

- (UInt32) effectIdForName:(NSString*)name fromPage:(NSString*)page
{
    UInt32 ret = -1;
	NSArray *pageComponents = [_effects objectForKey:page];
    if (pageComponents) {
        NSMutableDictionary *pageEffects = [pageComponents objectAtIndex:0];
        ret = [[pageEffects objectForKey:name] unsignedLongValue];
    }
    return ret;
}

- (ALuint) sourceIdForName:(NSString*)name fromPage:(NSString*)page
{
    ALuint ret = -1;
	NSArray *pageComponents = [_effects objectForKey:page];
    if (pageComponents) {
        NSMutableDictionary *pageSources = [pageComponents objectAtIndex:1];
        ret = [[pageSources objectForKey:name] unsignedIntValue];
    }
    return ret;
}



- (void) performPrepareEffectInBackgroundWithParams:(NSDictionary*)params
{
    // Set up a pool for the background task.
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
    [self performPrepareEffectWithParams:params];
    [params release];
    
    [pool release];
}


- (void) performPrepareEffectWithParams:(NSDictionary*)params
{
    NSString* name = [params objectForKey:kNameParam];
    NSString* path = [params objectForKey:kPathParam];
    NSString* page = [params objectForKey:kPageParam];
    
	if (!_initialized) {
		[self initialize];
	}
	
    UInt32 soundId;
    ALuint sourceId;
    SoundEngine_LoadEffect([path UTF8String], &soundId);
    SoundEngine_PrimeEffect(soundId, &sourceId);
        
    [self setEffectId:soundId andSourceId:sourceId withName:name fromPage:page];
    NSLog(@"Effect with name %@ for page %@ loaded with soundId=%lu and sourceId=%u", name, page, soundId, sourceId);
}

- (void) prepareEffect:(NSString*)name withFile:(NSString*)path fromPage:(NSString*)page inBackground:(bool)background
{
    if ([self isEffectPrepared:name fromPage:page]) {
        return;
    }

    NSDictionary *params = [[NSDictionary alloc] initWithObjectsAndKeys:
                                name, kNameParam, 
                                path, kPathParam,
                                page, kPageParam,
                                nil];
    if (background) {
        [params retain]; // released on background thread
        [self performSelectorInBackground:@selector(performPrepareEffectInBackgroundWithParams:) withObject:params];
    } else {
        [self performPrepareEffectWithParams:params];
    }
    
    [params release];
}

- (void) prepareEffect:(NSString*)name withFile:(NSString*)path fromPage:(NSString*)page
{
    [self prepareEffect:name withFile:path fromPage:page inBackground:NO];
}

- (void) playEffect:(NSString*)name fromPage:(NSString*)page
{
    ALuint sourceId = [self sourceIdForName:name fromPage:page];
	if (sourceId == -1) {
		NSLog(@"Sound %@ doesn't prepared", name);
		return;
	}
	SoundEngine_StartEffect(sourceId);
}

- (void) unloadEffectsFromPage:(NSString*)page
{
	NSArray *pageComponents = [_effects objectForKey:page];
	NSMutableDictionary *pageEffects = [pageComponents objectAtIndex:0];
	for (NSString* name in pageEffects) {
		NSLog(@"Unload effect name %@ for page %@", name, page);
        NSNumber *sid = [pageEffects objectForKey:name];
        SoundEngine_UnloadEffect([sid unsignedLongValue]);
	}
    [_effects removeObjectForKey:page];
}

+ (void) vibrate
{
	SoundEngine_Vibrate();
}

- (void) loadAmbient:(NSString*)path inSlot:(int)slot
{
	if (!_initialized) {
		[self initialize];
	}

	if (_ambients[slot] == nil) {
		_ambients[slot] = [[AmbientSound alloc] init];
	} else{
		[_ambients[slot] stopAndUnloadWithFadeTime:0.0];
	}

	_ambients[slot].slot = slot;
	[_ambients[slot] loadFromPath:path];
}

- (void) loadAndPlayAmbient:(NSString*)path withFadeTime:(double)time inSlot:(int)slot
{
	[self loadAmbient:path inSlot:slot];
	[self playLoadedAmbientWithFadeTime:time inSlot:slot];
}


- (void) playLoadedAmbientWithFadeTime:(double)time inSlot:(int)slot
{
	[_ambients[slot] playLoadedWithFadeTime:time];
}


- (void) stopAndUnloadAmbientWithFadeTime:(double)fade inSlot:(int)slot
{
	[_ambients[slot] stopAndUnloadWithFadeTime:fade];
}

- (void) replaceCurrentAmbientWith:(NSString*)path withFadeTime:(double)fade inSlot:(int)slot
{
    if (_ambients[slot] == nil) {
        [self loadAndPlayAmbient:path withFadeTime:fade inSlot:slot];
    } else {
        [_ambients[slot] replaceCurrentWith:path withFadeTime:fade];
    }    
}


- (void) dealloc
{
	for (int i = 0; i < ARRAY_LEN(_ambients); ++i) {
		if (_ambients[i] != nil) {
			if (_ambients[i].volume != -1.0) {
				[_ambients[i] stopAndUnloadWithFadeTime:0.0];
			}
			SAFE_RELEASE(_ambients[i]);
		}
	}

	for (NSString* page in _effects) {
		[self unloadEffectsFromPage:page];
	}
	[_effects release];
	SoundEngine_Teardown();
	[super dealloc];
}

@end
