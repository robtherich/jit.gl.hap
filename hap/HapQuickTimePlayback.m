/*
 HapQuickTimePlayback.m
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

#import "HapSupport.h"

#ifndef C74_X64

#import "HapQuickTimePlayback.h"
#import <QuickTime/QuickTime.h>

//Whenever a frame is ready this gets called by the QTVisualContext, usually on a background thread
static void VisualContextFrameCallback(QTVisualContextRef visualContext, const CVTimeStamp *timeStamp, void *refCon)
{
    OSErr err = noErr;
    CVImageBufferRef image;
    err = QTVisualContextCopyImageForTime(visualContext, nil, nil, &image);
    
    if (err == noErr && image)
    {
        //[(HapQuickTimePlayback *)refCon displayFrame:image];
		jit_gl_hap_draw_frame(refCon, image);
        CVBufferRelease(image);
    }
    else if (err != noErr)
    {
        NSLog(@"err %hd at QTVisualContextCopyImageForTime(), %s", err, __func__);
    }
    
    QTVisualContextTask(visualContext);
}

@implementation HapQuickTimePlayback

@synthesize movie;
@synthesize lasterror;

- (id)init
{
	self = [super init];
	if (self) {
		firstload = YES;
	}
	return self;
}

- (void)setJitob:(void*)ob
{
	jitob = ob;
}

- (BOOL)read:(const char *)filePath rcp:(BOOL)rcp
{
	NSString * movieFilePath = [[NSString alloc] initWithCString:filePath encoding:NSUTF8StringEncoding];
	if (movieFilePath) {
		if ([QTMovie canInitWithFile:movieFilePath]) {
			NSURL *url = [NSURL fileURLWithPath:movieFilePath];
			//return [self openMovie:url];
			
			// Stop and release our previous movie
			[self disposeMovie];
			
			// For simplicity we rebuild the visual context every time - you could re-use it if the new movie
			// will use exactly the same kind of context as the previous one.
			QTVisualContextRelease(visualContext);
			visualContext = NULL;
			
			// Set up the new movie and visual context
			lasterror=NULL;
			//movie = [[QTMovie alloc] initWithURL:url error:&lasterror];
			
			NSString *filePath = [url path];
			NSDictionary *dict = [
				NSDictionary dictionaryWithObjectsAndKeys:
				[NSNumber numberWithBool:rcp],QTMovieRateChangesPreservePitchAttribute,
				filePath, QTMovieFileNameAttribute,
				nil
			];

			movie = [[QTMovie alloc] initWithAttributes:dict error:&lasterror];
			
			if(!lasterror) {
				/*[
					[NSNotificationCenter defaultCenter]
					addObserver:self
					selector:@selector(movieLoadStateChanged:)
					name:QTMovieDidEndNotification
					object:movie
				];*/
			
				return YES;
			}			
		}
	}
	return NO;
}

-(void)movieDidEnd:(NSNotification *)notification
{
    QTMovie *m = (QTMovie *)[notification object];    
    if (m) {
		NSLog(@"movie ended");
    }
}

- (BOOL)addMovieToContext
{
	// It's important not to play the movie until it has been attached to a context, otherwise it will start decompression with a non-optimal pixel format
	OSStatus err = noErr;
	
	// Check if the movie has a Hap video track
	if (HapQTMovieHasHapTrackPlayable(movie)) {
		CFDictionaryRef pixelBufferOptions = HapQTCreateCVPixelBufferOptionsDictionary();
		// QT Visual Context attributes
		/*NSDictionary *visualContextOptions = [
			NSDictionary dictionaryWithObjectsAndKeys:[
				NSDictionary dictionaryWithObjectsAndKeys:
					[NSNumber numberWithFloat:320], kQTVisualContextTargetDimensions_WidthKey,
					[NSNumber numberWithFloat:240], kQTVisualContextTargetDimensions_HeightKey,
					nil
			], kQTVisualContextTargetDimensionsKey,
			pixelBufferOptions,	kQTVisualContextPixelBufferAttributesKey,
			nil
		];*/
		
        NSDictionary *visualContextOptions = [
			NSDictionary dictionaryWithObject:(NSDictionary *)pixelBufferOptions
			forKey:(NSString *)kQTVisualContextPixelBufferAttributesKey
		];
		
		CFRelease(pixelBufferOptions);
		err = QTPixelBufferContextCreate(kCFAllocatorDefault, (CFDictionaryRef)visualContextOptions, &visualContext);
	}
	else {
		err = QTOpenGLTextureContextCreate(kCFAllocatorDefault, CGLGetCurrentContext(), CGLGetPixelFormat(CGLGetCurrentContext()), nil, &visualContext);
	}
	
	if (err == noErr) {
		err = SetMovieVisualContext([movie quickTimeMovie],visualContext);
		if (err == noErr) {
			return YES;
		}
	}
	return NO;
}

- (void)getCurFrame
{
    OSErr err = noErr;
    CVImageBufferRef image = NULL;
    err = QTVisualContextCopyImageForTime(visualContext, nil, nil, &image);
    
    if (err == noErr && image) {
		curimage = image;
		jit_gl_hap_draw_frame(jitob, image);
    }
    else if (err != noErr) {
        NSLog(@"err %hd at QTVisualContextCopyImageForTime(), %s", err, __func__);
    }
}

- (void)releaseCurFrame
{    
    if (curimage) {
        CVBufferRelease(curimage);
		curimage = NULL;
	}
    QTVisualContextTask(visualContext);
}

- (void)disposeMovie
{
    if (movie) {
        [movie stop];
        SetMovieVisualContext([movie quickTimeMovie], NULL);
        [movie release];
        movie = nil;
    }
}

- (void)disposeContext
{
    if (movie) {
        [movie stop];
        SetMovieVisualContext([movie quickTimeMovie], NULL);
		QTVisualContextRelease(visualContext);
		visualContext = NULL;
    }
}

- (double)frameRate
{  
  if (movie) {
    QTMedia *media = [[[movie tracksOfMediaType:QTMediaTypeVideo] objectAtIndex:0] media];
    long long sampleCount = [[media attributeForKey:QTMediaSampleCountAttribute] longValue];
    
    QTTime durationQTTime = [[media attributeForKey:QTMediaDurationAttribute] QTTimeValue];
    double duration = (double)durationQTTime.timeValue/durationQTTime.timeScale;
    
    return sampleCount/duration;
  }
  return 0;
}

- (long long)frameCount
{  
  if (movie) {
    QTMedia *media = [[[movie tracksOfMediaType:QTMediaTypeVideo] objectAtIndex:0] media];
	return [[media attributeForKey:QTMediaSampleCountAttribute] longValue];
  }
  return 0;
}
@end

#endif