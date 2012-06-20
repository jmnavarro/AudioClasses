//
//  PagedSpeechManager.m
//  Cuentorines
//
//  Created by JM on 08/06/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "PagedSpeechManager.h"
#import "SpeechChunk.h"

@implementation PagedSpeechManager

@synthesize currentPage = _currentPage;


- (void) addSpeechForPageNumber:(NSNumber*)page withChunk:(SpeechChunk*)chunk {
    DDLogCVerbose(@"Add speech for page %@ start=%f len=%f", page, chunk->start, chunk->length);
	[self addSpeech:page withChunk:chunk];
}


- (void) addSpeechForPage:(int)page withChunk:(SpeechChunk*)chunk {
	DDLogCVerbose(@"Add speech for page %d start=%f len=%f", page, chunk->start, chunk->length);
	[self addSpeech:[NSNumber numberWithInt:page] withChunk:chunk];
}

- (bool) playSpeechForPage:(int)page {
	return [self playSpeech:[NSNumber numberWithInt:page]];	
}

- (bool) isPlayingPage:(int)page {
    NSNumber *n = (NSNumber*) [self playingId];
    return (n != nil && [n intValue] == page);
}

- (bool) playSpeechForPage:(int)page stoppingPrevious:(bool)stop {
	return [self playSpeech:[NSNumber numberWithInt:page] stoppingPrevious:stop];
}



@end
