//
//  AmbientSound.h
//
//  Created by JM on 21/06/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface AmbientSound : NSObject {
	NSString *_replacing;
}

@property (nonatomic, assign) float volume;
@property (nonatomic, assign) int slot;


- (void) stopAndUnloadWithFadeTime:(double)fade;

- (void) loadFromPath:(NSString*)path;
- (void) playLoadedWithFadeTime:(double)time;

- (void) replaceCurrentWith:(NSString*)path withFadeTime:(double)fade;


@end
