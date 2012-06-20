//
//  PagedSpeechManager.h
//  Cuentorines
//
//  Created by JM on 08/06/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "SpeechManager.h"


@interface PagedSpeechManager : SpeechManager {
	int _currentPage;
}

@property (nonatomic, assign) int currentPage;


- (void) addSpeechForPageNumber:(NSNumber*)page withChunk:(SpeechChunk*)chunk;
- (void) addSpeechForPage:(int)page withChunk:(SpeechChunk*)chunk;

- (bool) playSpeechForPage:(int)page;
- (bool) playSpeechForPage:(int)page stoppingPrevious:(bool)stop;

- (bool) isPlayingPage:(int)page;

@end
