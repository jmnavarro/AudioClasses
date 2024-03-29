/*
 *  macros.h
 *
 *  Created by JM on 05/07/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#define ARRAY_LEN(x)  (sizeof(x)/sizeof(x[0]))

/*
#define INFO_RELEASE(x)          \
 NSLog(@"INFO RELEASE %p - %@ - %@ - refcount=%d", x, [x class], [x description], [x retainCount]); \
 do {                             \
    if ([x retainCount] == 1) {   \
	   NSLog(@"   Final release %p", x); \
       [x release];               \
	   x = nil;                   \
	   break;                     \
    } else {                      \
		[x release];              \
        NSLog(@"   Released ref %p: %d", x, [x retainCount]); \
	}                             \
 } while(x != nil);
*/

#define SAFE_RELEASE(x)  do { [x release];x = nil; } while(0);
	//#define SAFE_RELEASE(x)  do { NSLog(@"%p - %@ - %@ - refcount=%d", x, [x class], [x description], [x retainCount]);[x release]; } while(0);

