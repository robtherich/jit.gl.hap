#include "jit.gl.hap.h"

#if !defined(C74_X64) && defined(MAC_VERSION)

void jit_gl_hap_new_native(t_jit_gl_hap *x)
{
	x->hap = [[HapQuickTimePlayback alloc] init];
	if(x->hap)
		[x->hap setJitob:x];

}

void jit_gl_hap_free_native(t_jit_gl_hap *x)
{
	[x->hap disposeContext];
	[x->hap disposeMovie];
	[x->hap release];
}

void jit_gl_hap_read_native(t_jit_gl_hap *x, char *fname, short vol)
{
	char fpath[MAX_PATH_CHARS];
	char cpath[MAX_PATH_CHARS];
	
	path_topathname(vol, fname, fpath);
	path_nameconform(fpath, cpath, PATH_STYLE_SLASH, PATH_TYPE_BOOT);
	x->file = gensym(cpath);
	if(![x->hap read:(const char *)x->file->s_name rcp:x->rate_preserves_pitch]) {
		if([x->hap lasterror]) {
			NSString * description = [[x->hap lasterror] localizedDescription];
			jit_object_error((t_object*)x, "error loading quicktime movie: %s", [description UTF8String]);
		}
		else {
			jit_object_error((t_object*)x, "unknown error loading quicktime movie");
		}
	}
	else {
		QTTime duration = [[x->hap movie] duration];
		x->has_video = [x->hap hasVideoTrack];
		x->newfile = 1;		
		x->timescale = duration.timeScale;
		x->duration = duration.timeValue;
		if(x->has_video) {
			x->fps = [x->hap frameRate];
			x->framecount = [x->hap frameCount];
		}
		else {
			x->fps = 0;
			x->framecount = 0;
		}
	}
}

void jit_gl_hap_dispose(t_jit_gl_hap *x)
{
	if(x->hap){
		[x->hap disposeMovie];
	}
}

t_jit_err jit_gl_hap_dest_closing(t_jit_gl_hap *x)
{
	if(x->movieloaded) {
		[x->hap disposeContext];
		x->newfile = 1;
	}
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_load_file(t_jit_gl_hap *x)
{
	if(x->has_video) {
		x->hap_format = [x->hap addMovieToContext];
		if(x->hap_format==JIT_GL_HAP_PF_NONE) {
			jit_object_error((t_object*)x, "unknown error adding quicktime movie to context");
			return JIT_ERR_GENERIC;
		}
	}
	else {
		x->hap_format = JIT_GL_HAP_PF_NO_VIDEO;
	}
	
	x->movieloaded = 1;
	
	@try {
		[[x->hap movie] play];
	}
	@catch (NSException * e) {
		jit_object_error((t_object*)x, "error playing movie: %s %s", [[e name]UTF8String], [[e reason]UTF8String]);
		[x->hap disposeMovie];
		x->movieloaded = 0;
	}
	@finally {
		x->newfile = 0;
		if(x->movieloaded) {
			[[x->hap movie] setRate:x->rate];
			[[x->hap movie] setVolume:x->vol];
			jit_gl_hap_do_looppoints(x);
			jit_attr_user_touch(x, gensym("loopstart"));
			jit_attr_user_touch(x, gensym("loopend"));
			jit_attr_user_touch(x, gensym("looppoints"));
							
			if(!x->autostart) {
				[[x->hap movie] stop];
				jit_gl_hap_do_set_time(x, 0);
			}
		}
	}
	return JIT_ERR_NONE;
}

t_bool jit_gl_hap_getcurrentframe(t_jit_gl_hap *x)
{
	if(x->has_video)
		[x->hap getCurFrame];
	return 1;
}

void jit_gl_hap_releaseframe(t_jit_gl_hap *x)
{
	if(x->has_video)
		[x->hap releaseCurFrame];
}

void jit_gl_hap_start(t_jit_gl_hap *x)
{
	if(x->movieloaded) {
		[[x->hap movie] play];
		[[x->hap movie] setRate:x->rate];
	}
}

void jit_gl_hap_stop(t_jit_gl_hap *x)
{
	if(x->movieloaded) {
		[[x->hap movie] stop];
	}
}

t_jit_err jit_gl_hap_jump(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av)
{
	if (x->movieloaded) {
		QTTime time = [[x->hap movie] currentTime];
		t_atom_long jump = jit_gl_hap_frametotime(x, jit_atom_getlong(av));
		jit_gl_hap_do_set_time(x, time.timeValue + jump);
	}
	return JIT_ERR_NONE;
}

void jit_gl_hap_do_set_time(t_jit_gl_hap *x, t_atom_long time)
{
	if (x->movieloaded) {
		if (x->loop == JIT_GL_HAP_LOOP_LIMITS)
			CLIP_ASSIGN(time, x->looppoints[0], x->looppoints[1]);
			
		QTTime duration = [[x->hap movie] duration];
		if (duration.timeValue) {
			CLIP_ASSIGN(time, 0, duration.timeValue);
			duration.timeValue = time;
			[[x->hap movie] setCurrentTime: duration];
			x->suppress_loopnotify = 1;
		}
	}
}

t_atom_long jit_gl_hap_do_get_time(t_jit_gl_hap *x)
{
	QTTime time = [[x->hap movie] currentTime];
	return (t_atom_long)(time.timeValue);
}

t_jit_err jit_gl_hap_rate_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av)
		x->rate = jit_atom_getfloat(av);
	if(x->movieloaded)
		[[x->hap movie] setRate:x->rate];
		
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_vol_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av)
		x->vol = CLAMP(jit_atom_getfloat(av), 0, 1);
	if(x->movieloaded)
		[[x->hap movie] setVolume:x->vol];
		
	return JIT_ERR_NONE;
}

void jit_gl_hap_do_loop(t_jit_gl_hap *x)
{
	if(x->loop==JIT_GL_HAP_LOOP_OFF) {
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:NO] forKey:QTMoviePlaysSelectionOnlyAttribute];
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:NO] forKey:QTMovieLoopsAttribute];
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:NO] forKey:QTMovieLoopsBackAndForthAttribute];
	}
	else if(x->loop==JIT_GL_HAP_LOOP_ON) {
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:YES] forKey:QTMoviePlaysSelectionOnlyAttribute];
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:YES] forKey:QTMovieLoopsAttribute];
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:NO] forKey:QTMovieLoopsBackAndForthAttribute];
	}
	else if(x->loop==JIT_GL_HAP_LOOP_PALINDROME) {
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:YES] forKey:QTMoviePlaysSelectionOnlyAttribute];
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:NO] forKey:QTMovieLoopsAttribute];
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:YES] forKey:QTMovieLoopsBackAndForthAttribute];
	}
	else if(x->loop==JIT_GL_HAP_LOOP_LIMITS) {
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:YES] forKey:QTMoviePlaysSelectionOnlyAttribute];
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:NO] forKey:QTMovieLoopsAttribute];
		[[x->hap movie] setAttribute:[NSNumber numberWithBool:NO] forKey:QTMovieLoopsBackAndForthAttribute];
	}
}

void jit_gl_hap_do_looppoints(t_jit_gl_hap *x)
{
	QTTime time = [[x->hap movie] duration];
	QTTimeRange range;

	if(x->looppoints[1]<0)
		x->looppoints[1] = x->duration;

	if(x->looppoints[0]>=x->looppoints[1] && x->looppoints[1]>0)
		x->looppoints[0] = x->looppoints[1]-1;
	
	CLIP_ASSIGN(x->looppoints[1], x->looppoints[0]+1, x->duration);
	CLIP_ASSIGN(x->looppoints[0], 0, x->looppoints[1]-1);
		
	time.timeValue = x->looppoints[0];
	range.time = time;
	time.timeValue = x->looppoints[1]-x->looppoints[0];
	range.duration = time;
	
	[[x->hap movie] setSelection:range];
}

long jit_gl_hap_do_loadram(t_jit_gl_hap *x, t_atom_long from, t_atom_long to, short unload)
{
	t_atom_long duration = [[x->hap movie] duration].timeValue;
	
	if(to)
		CLIP_ASSIGN(to, 0, duration);
	else
		to = duration;
		
	CLIP_ASSIGN(from, 0, duration);
		
	if (from > to) {
		t_atom_long temp = to;
		to = from;
		from = temp;
	}
	
	return LoadMovieIntoRam([[x->hap movie] quickTimeMovie], from, to - from, (unload ? unkeepInRam : keepInRam));
}

#endif	// MAC_VERSION