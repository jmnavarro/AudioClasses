//
//  SoundChunck.h
//  Cuentorines
//
//  Created by JM on 09/06/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface SpeechChunk : NSObject {
@public
	NSTimeInterval start;
	NSTimeInterval length;
	float volume;
}

@end
