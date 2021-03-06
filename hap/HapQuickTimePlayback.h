/*
 HapQuickTimePlayback.h
 Hap QuickTime Playback
 
 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 * Neither the name of Hap nor the name of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
  Modified by Rob Ramirez for jit.gl.hap Max 6 external, 2013
 */

#include "ext.h"
#ifndef C74_X64
#ifdef MAC_VERSION

#import <Cocoa/Cocoa.h>
#import <QTKit/QTKit.h>
#import <Foundation/Foundation.h>

void jit_gl_hap_draw_frame(void *x, CVImageBufferRef frame);

@interface HapQuickTimePlayback : NSObject
{
    QTMovie                 *movie;
    QTVisualContextRef      visualContext;
	CVImageBufferRef		curimage;
	CGLContextObj			ctx;
    void					*jitob;
	BOOL					firstload;
	NSError					*lasterror;
}

- (void)setJitob:(void*)ob;
- (BOOL)read:(const char *)filePath rcp:(BOOL)rcp;
- (void)movieDidEnd:(NSNotification *)notification;
- (t_uint8)addMovieToContext;
- (void)getCurFrame;
- (void)releaseCurFrame;
- (void)disposeMovie;
- (void)disposeContext;

- (double)frameRate;
- (long long)frameCount;

- (BOOL)hasVideoTrack;
@property (readonly) QTMovie *movie;
@property (readonly) NSError *lasterror;

@end

#endif
#endif