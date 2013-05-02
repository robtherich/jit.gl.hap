#include "ext.h"

#if !defined(C74_X64) && defined(WIN_VERSION)

#include "jit.gl.hap.h"

void jit_gl_hap_new_native(t_jit_gl_hap *x)
{
	InitializeQTML(0);                          // Initialize QTML 
	EnterMovies();                              // Initialize QuickTime 

	x->movie = NULL;
	x->visualContext = NULL;
	x->currentImage = NULL;
}

void jit_gl_hap_free_native(t_jit_gl_hap *x)
{
	jit_gl_hap_dispose(x);
	
	if(x->visualContext) {		
		QTVisualContextRelease(x->visualContext);
		x->visualContext = NULL;
	}

	ExitMovies();                               // Terminate QuickTime 
	TerminateQTML();                            // Terminate QTML 
}

void jit_gl_hap_read_native(t_jit_gl_hap *x, char *fname, short vol)
{
	char fpath[MAX_PATH_CHARS];
	char cpath[MAX_PATH_CHARS];
	Handle		dataRef;
	OSType		dataRefType;
	OSErr		err;
	short		resid;

	path_topathname(vol, fname, fpath);
	path_nameconform(fpath, cpath, PATH_STYLE_SLASH, PATH_TYPE_BOOT);
	x->file = gensym(cpath);

	XQT_NewDataReferenceFromMaxPath(vol, fname, &dataRef, &dataRefType, &err);
	err = NewMovieFromDataRef(&x->movie, newMovieActive, &resid, dataRef, dataRefType);
	if(err) {
		jit_object_error((t_object*)x, "unknown error loading quicktime movie");
		if(x->movie) 
			DisposeMovie(x->movie);
	}
	else {
		//if(!HapQTQuickTimeMovieHasHapTrackPlayable(x->movie)) {
			//DisposeMovie(x->movie);
			//jit_object_error((t_object*)x, "non-hap codec movies are unsupported at this time");
			//goto out;
		//}
		//else {
			//MediaHandler mh = NULL;
			Track	track = NULL;
			Media	media = NULL;
			SetMovieDefaultDataRef(x->movie, dataRef, dataRefType);
			if (track = GetMovieIndTrackType(x->movie, 1, kCharacteristicHasVideoFrameRate, movieTrackCharacteristic)) {
				if (media = GetTrackMedia(track)) {
					//if (mh = GetMediaHandler(media)) {
						//err = MediaHasCharacteristic(mh, kCharacteristicIsAnMpegTrack, &isMpeg);
					//}
				}
			}				
			x->newfile = 1;
			//MoviesTask(x->movie, 0);
			x->framecount = GetMediaSampleCount(media);
			x->timescale = GetMovieTimeScale(x->movie);
			x->duration = GetMovieDuration(x->movie);
			x->fps = (float)((double)x->framecount*(double)x->timescale/(double)x->duration);
		//}
	}
//out:
	DisposeHandle(dataRef);
	dataRef = NULL;
}

void jit_gl_hap_dispose(t_jit_gl_hap *x)
{
	if (x->movie) {
		StopMovie(x->movie);
		SetMovieVisualContext(x->movie, NULL);
		DisposeMovie(x->movie);
		x->movie = NULL;
	}
}

t_jit_err jit_gl_hap_dest_closing(t_jit_gl_hap *x)
{
	if(x->movieloaded) {
		StopMovie(x->movie);
		SetMovieVisualContext(x->movie, NULL);
		QTVisualContextRelease(x->visualContext);
		x->visualContext = NULL;
		x->newfile = 1;
	}
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_load_file(t_jit_gl_hap *x)
{
	OSStatus err;
	// Check if the movie has a Hap video track
	Boolean hashap = HapQTQuickTimeMovieHasHapTrackPlayable(x->movie);
	CFDictionaryRef pixelBufferOptions;
	if(hashap) {
		pixelBufferOptions = HapQTCreateCVPixelBufferOptionsDictionary();
		x->hap_format = JIT_GL_HAP_PF_HAP;
	}
	else {
		t_bool hasalpha = HapQTHasAlpha(x->movie);
		pixelBufferOptions = HapQTCreateNonHapCVPixelBufferOptionsDictionary(hasalpha);
		x->hap_format = hasalpha ? JIT_GL_HAP_PF_RGBA : JIT_GL_HAP_PF_RGB;
	}
	{
		const void* key = kQTVisualContextPixelBufferAttributesKey;
		const void* value = pixelBufferOptions;
		CFDictionaryRef visualContextOptions = CFDictionaryCreate(
			kCFAllocatorDefault, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
		);
	
		CFRelease(pixelBufferOptions);
		err = QTPixelBufferContextCreate(kCFAllocatorDefault, (CFDictionaryRef)visualContextOptions, &x->visualContext);
		CFRelease(visualContextOptions);
	}
	//else {
		//err = QTOpenGLTextureContextCreate(kCFAllocatorDefault, CGLGetCurrentContext(), CGLGetPixelFormat(CGLGetCurrentContext()), nil, &visualContext);
		//jit_object_error((t_object*)x, "non-hap codec movies are unsupported at this time");
		//return JIT_ERR_GENERIC;
	//}

	if (err == noErr) {
		err = SetMovieVisualContext(x->movie, x->visualContext);
	}
	if(err != noErr) {
		jit_object_error((t_object*)x, "unknown error adding quicktime movie to context");
		return JIT_ERR_GENERIC;
	}
	
	x->movieloaded = 1;
	x->newfile = 0;
	
	StartMovie(x->movie);
	SetMovieRate(x->movie, DoubleToFixed(x->rate));
	SetMovieVolume(x->movie, (short)(x->vol*255.));
	
	jit_gl_hap_do_looppoints(x);
	jit_attr_user_touch(x, gensym("loopstart"));
	jit_attr_user_touch(x, gensym("loopend"));
	jit_attr_user_touch(x, gensym("looppoints"));
							
	if(!x->autostart) {
		StopMovie(x->movie);
		jit_gl_hap_do_set_time(x, 0);
	}
	return JIT_ERR_NONE;
}

t_bool jit_gl_hap_getcurrentframe(t_jit_gl_hap *x)
{
    OSStatus err = noErr;
    CVImageBufferRef image = NULL;
    err = QTVisualContextCopyImageForTime(x->visualContext, nil, nil, &image);
    
    if (err == noErr && image) {
		x->currentImage = image;
		jit_gl_hap_draw_frame(x, image);
		return 1;
    }
	return 0;
    //else if (err != noErr) {
      //  NSLog(@"err %hd at QTVisualContextCopyImageForTime(), %s", err, __func__);
    //}
}

void jit_gl_hap_releaseframe(t_jit_gl_hap *x)
{
    if (x->currentImage) {
        CVBufferRelease(x->currentImage);
		x->currentImage = NULL;
	}
    QTVisualContextTask(x->visualContext);
	MoviesTask(x->movie, 0);
}

void jit_gl_hap_start(t_jit_gl_hap *x)
{
	if(x->movieloaded) {
		StartMovie(x->movie);
		SetMovieRate(x->movie, DoubleToFixed(x->rate));
	}
}

void jit_gl_hap_stop(t_jit_gl_hap *x)
{
	if(x->movieloaded) {
		StopMovie(x->movie);
	}
}

t_jit_err jit_gl_hap_jump(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av)
{
	if (x->movieloaded) {
		t_atom_long jump = jit_gl_hap_frametotime(x, jit_atom_getlong(av));
		jit_gl_hap_do_set_time(x, GetMovieTime(x->movie,NULL) + jump);
	}
	return JIT_ERR_NONE;
}

void jit_gl_hap_do_set_time(t_jit_gl_hap *x, t_atom_long time)
{
	if (x->movieloaded) {
		TimeBase tb;
		TimeRecord tr;
		
		if (x->loop == JIT_GL_HAP_LOOP_LIMITS)
			CLIP_ASSIGN(time, x->looppoints[0], x->looppoints[1]);

		tb = GetMovieTimeBase(x->movie);
		GetTimeBaseStartTime(tb, GetMovieTimeScale(x->movie), &tr); // fill out struct		
		tr.value.lo = 0;
		SetTimeBaseStartTime(tb, &tr);
		tr.value.lo = GetMovieDuration(x->movie);

		if (tr.value.lo) {
			SetTimeBaseStopTime(tb, &tr);
			SetMovieTimeValue(x->movie, time);		
			x->suppress_loopnotify = 1;
		}
	}
}

t_atom_long jit_gl_hap_do_get_time(t_jit_gl_hap *x)
{
	return GetMovieTime(x->movie,NULL);
}

t_jit_err jit_gl_hap_rate_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av)
		x->rate = (float)jit_atom_getfloat(av);
	if(x->movieloaded)
		SetMovieRate(x->movie, DoubleToFixed(x->rate));
		
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_vol_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av)
		x->vol = (float)(CLAMP(jit_atom_getfloat(av), 0, 1));
	if(x->movieloaded)
		SetMovieVolume(x->movie, (short)(x->vol*255.));
		
	return JIT_ERR_NONE;
}

void jit_gl_hap_do_loop(t_jit_gl_hap *x)
{
	long flags = 0;
	switch(x->loop) {
		case JIT_GL_HAP_LOOP_ON: flags = loopTimeBase; break;
		case JIT_GL_HAP_LOOP_PALINDROME: flags = palindromeLoopTimeBase; break;
		case JIT_GL_HAP_LOOP_LIMITS:
		case JIT_GL_HAP_LOOP_OFF:
		default: flags = 0; break;
	}
	x->loopflags = flags;

	if(x->movieloaded)
		jit_gl_hap_do_looppoints(x);
}

void jit_gl_hap_do_looppoints(t_jit_gl_hap *x)
{
	TimeRecord tr;
	TimeBase tb;
	long start,end;
	long ts;
	
	if(x->looppoints[1]<0)
		x->looppoints[1] = x->duration;

	if(x->looppoints[0]>=x->looppoints[1] && x->looppoints[1]>0)
		x->looppoints[0] = x->looppoints[1]-1;
	
	CLIP_ASSIGN(x->looppoints[1], x->looppoints[0]+1, x->duration);
	CLIP_ASSIGN(x->looppoints[0], 0, x->looppoints[1]-1);

	start = x->looppoints[0];
	end = x->looppoints[1];

	tb = GetMovieTimeBase(x->movie);

	SetMovieSelection(x->movie, 0, x->duration);
	SetMovieActiveSegment(x->movie, 0, x->duration);
	ts = GetMovieTimeScale(x->movie);
	GetTimeBaseTime(tb, ts, &tr);
	tr.value.lo = GetMovieTime(x->movie, NULL);
	SetTimeBaseTime(tb, &tr);
	
	// check loop duration
	if (end - start < (ts * 0.001))
		end = start + (long)(MAX(1, ts * 0.001));
	
	if (x->loop != JIT_GL_HAP_LOOP_OFF) {
		SetTimeBaseFlags(tb,0);	
		GetTimeBaseStartTime(tb, ts, &tr);
		tr.value.lo = start;
		SetTimeBaseStartTime(tb, &tr);
		tr.value.lo = end;
		SetTimeBaseStopTime(tb, &tr);
		SetTimeBaseFlags(tb, x->loopflags);
		if (x->loop == JIT_GL_HAP_LOOP_LIMITS) {
			SetMovieSelection(x->movie, start, (end - start));
			SetMovieActiveSegment(x->movie, start, (end - start));
		}
	} 
	else {
		SetTimeBaseFlags(tb,0);	
		GetTimeBaseStartTime(tb, ts, &tr);
		tr.value.lo = 0;
		SetTimeBaseStartTime(tb, &tr);
		tr.value.lo = x->duration;
		SetTimeBaseStopTime(tb, &tr);
		SetTimeBaseFlags(tb,x->loopflags);
	}
}

#endif