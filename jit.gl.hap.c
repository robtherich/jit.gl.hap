/*
 *  jit.gl.hap.c - A jitter external to provide native HAP playback
 *	by Rob Ramirez, rob@robtherich.org 2013
 *
 **********************************************************
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------
 *
 * Requires the max 6.1 (or later) SDK from cycling '74
 * http://cycling74.com/products/sdk/
 * The SDK folder is assumed to be at the same level as the jit.gl.hap project folder.
 * This can be changed by editing the included maxmspsdk.xconfig file.
 *
 * Based on the Hap Quicktime Playback Demo
 * https://github.com/vidvox/hap-quicktime-playback-demo
 *
 * Credit due to Brian Chasalow, for the Texture to FBO functionality found in jit.bc.qtkit
 * https://github.com/brianchasalow/jit.BC.QTKit
 *
 * Many thanks to Tom Butterworth and Vidvox for the HAP project!
 * http://vdmx.vidvox.net/blog/hap
 */


#include "ext.h"
#ifndef C74_X64

#include "HapSupport.h"
#include "jit.gl.hap.glsl.h"
#include "jit.gl.hap.h"


void *_jit_gl_hap_class;

t_jit_err jit_gl_hap_init(void);
t_jit_gl_hap *jit_gl_hap_new(t_symbol * dest_name);
void jit_gl_hap_free(t_jit_gl_hap *x);
void jit_gl_hap_read(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av);

// ob3d
t_jit_err jit_gl_hap_draw(t_jit_gl_hap *x);
t_jit_err jit_gl_hap_dest_closing(t_jit_gl_hap *x);
t_jit_err jit_gl_hap_dest_changed(t_jit_gl_hap *x);
t_bool jit_gl_hap_getcurrentframe(t_jit_gl_hap *x);
t_bool jit_gl_hap_draw_begin(t_jit_gl_hap *x, GLuint texid, GLuint width, GLuint height);
void jit_gl_hap_dodraw(t_jit_gl_hap *x, GLuint width, GLuint height);
void jit_gl_hap_draw_end(t_jit_gl_hap *x);
void jit_gl_hap_submit_texture(t_jit_gl_hap *x);
void jit_gl_hap_submit_nonhap_texture(t_jit_gl_hap *x);
void jit_gl_hap_do_report(t_jit_gl_hap *x);

// movie control
void jit_gl_hap_start(t_jit_gl_hap *x);
void jit_gl_hap_stop(t_jit_gl_hap *x);
void jit_gl_hap_clear_looppoints(t_jit_gl_hap *x);
t_jit_err jit_gl_hap_frame(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av);
t_jit_err jit_gl_hap_jump(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av);
t_jit_err jit_gl_hap_loadram(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av);

// attributes

t_jit_err jit_gl_hap_time_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av);
t_jit_err jit_gl_hap_time_get(t_jit_gl_hap *x, void *attr, long *ac, t_atom **av);
t_jit_err jit_gl_hap_rate_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av);
t_jit_err jit_gl_hap_vol_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av);
t_jit_err jit_gl_hap_loop_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av);

t_jit_err jit_gl_hap_loopstart(t_jit_gl_hap *x, void *attr, long ac, t_atom *av);
t_jit_err jit_gl_hap_loopend(t_jit_gl_hap *x, void *attr, long ac, t_atom *av);
t_jit_err jit_gl_hap_looppoints(t_jit_gl_hap *x, void *attr, long ac, t_atom *av);

// texture
void jit_gl_hap_sendoutput(t_jit_gl_hap *x, t_symbol *s, int argc, t_atom *argv);
t_jit_err jit_gl_hap_getattr_out_name(t_jit_gl_hap *x, void *attr, long *ac, t_atom **av);
t_jit_err jit_gl_hap_setattr_out_name(t_jit_gl_hap *x, void *attr, long ac, t_atom *av);

static t_symbol *ps_bind;
static t_symbol *ps_unbind;
static t_symbol *ps_width;
static t_symbol *ps_height;
static t_symbol *ps_glid;
static t_symbol *ps_draw;

// --------------------------------------------------------------------------------

t_jit_err jit_gl_hap_init(void) 
{
	void *ob3d;
	void *attr;
	long attrflags=0;
	long ob3d_flags = JIT_OB3D_NO_ROTATION_SCALE;
	ob3d_flags |= JIT_OB3D_NO_POLY_VARS;
	ob3d_flags |= JIT_OB3D_NO_BLEND;
	ob3d_flags |= JIT_OB3D_NO_TEXTURE;
	ob3d_flags |= JIT_OB3D_NO_MATRIXOUTPUT;
	ob3d_flags |= JIT_OB3D_AUTO_ONLY;
	ob3d_flags |= JIT_OB3D_NO_DEPTH;
	ob3d_flags |= JIT_OB3D_NO_ANTIALIAS;
	ob3d_flags |= JIT_OB3D_NO_FOG;
	ob3d_flags |= JIT_OB3D_NO_LIGHTING_MATERIAL;
	ob3d_flags |= JIT_OB3D_NO_SHADER;
	ob3d_flags |= JIT_OB3D_NO_BOUNDS;
	ob3d_flags |= JIT_OB3D_NO_COLOR;	
	
	_jit_gl_hap_class = jit_class_new("jit_gl_hap", 
		(method)jit_gl_hap_new, (method)jit_gl_hap_free,
		sizeof(t_jit_gl_hap),A_DEFSYM,0L);
	
	ob3d = jit_ob3d_setup(_jit_gl_hap_class, calcoffset(t_jit_gl_hap, ob3d), ob3d_flags);

	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_draw,			"ob3d_draw", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_dest_closing, "dest_closing", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_dest_changed, "dest_changed", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_object_register,		"register", A_CANT, 0L);

	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_read,			"read", A_GIMME, 0);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_sendoutput,	"sendoutput", A_DEFER_LOW, 0L);
	
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_dispose,	"dispose",	A_USURP_LOW, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_start,	"start",	A_DEFER_LOW, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_stop,		"stop",		A_DEFER_LOW, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_jump,		"jump",		A_DEFER_LOW, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_frame,	"frame",	A_DEFER_LOW, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_clear_looppoints,	"clear_looppoints", A_DEFER_LOW, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_loadram,	"loadram",	A_DEFER_LOW, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_loadram,	"unloadram",A_DEFER_LOW, 0L);
	
	attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
	
	//attr = jit_object_new(_jit_sym_jit_attr_offset,"adapt",_jit_sym_char,attrflags,
	//	(method)0L,(method)0L,calcoffset(t_jit_gl_hap,adapt));
	//jit_class_addattr(_jit_gl_hap_class,attr);
	//object_addattr_parse(attr,"style",_jit_sym_symbol,0,"onoff");
	
	attr = jit_object_new(_jit_sym_jit_attr_offset_array,"dim",_jit_sym_long,2,attrflags,
		(method)0L,(method)0L,0,calcoffset(t_jit_gl_hap,dim));
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	//attr = jit_object_new(_jit_sym_jit_attr_offset_array,"rect",_jit_sym_float32,4,attrflags,
	//	(method)0L,(method)0L,0,calcoffset(t_jit_gl_hap,rect));
	//jit_class_addattr(_jit_gl_hap_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"time",_jit_sym_long,attrflags,
		(method)jit_gl_hap_time_get,(method)jit_gl_hap_time_set,0L);
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"loop",_jit_sym_char,attrflags,
		(method)0L,(method)jit_gl_hap_loop_set,calcoffset(t_jit_gl_hap,loop));
	jit_class_addattr(_jit_gl_hap_class,attr);
	object_addattr_parse(attr,"style",_jit_sym_symbol,0,"enumindex");
	object_addattr_parse(attr,"enumvals",_jit_sym_atom,0,"off normal palindrome playback-limits");
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"autostart",_jit_sym_char,attrflags,
		(method)0L,(method)0L,calcoffset(t_jit_gl_hap,autostart));
	jit_class_addattr(_jit_gl_hap_class,attr);
	object_addattr_parse(attr,"style",_jit_sym_symbol,0,"onoff");
		
	attr = jit_object_new(_jit_sym_jit_attr_offset,"rate",_jit_sym_float32,attrflags,
		(method)0L,(method)jit_gl_hap_rate_set,calcoffset(t_jit_gl_hap,rate));
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"vol",_jit_sym_float32,attrflags,
		(method)0L,(method)jit_gl_hap_vol_set,calcoffset(t_jit_gl_hap,vol));
	jit_class_addattr(_jit_gl_hap_class,attr);	
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"rate_preserves_pitch",_jit_sym_char,attrflags,
		(method)0L,(method)0L,calcoffset(t_jit_gl_hap,rate_preserves_pitch));
	jit_class_addattr(_jit_gl_hap_class,attr);
	object_addattr_parse(attr,"style",_jit_sym_symbol,0,"onoff");
		
	attr = jit_object_new(_jit_sym_jit_attr_offset,"loopstart",_jit_sym_long,attrflags,
		(method)0L,(method)jit_gl_hap_loopstart,calcoffset(t_jit_gl_hap,looppoints[0]));
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"loopend",_jit_sym_long,attrflags,
		(method)0L,(method)jit_gl_hap_loopend,calcoffset(t_jit_gl_hap,looppoints[1]));
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset_array,"looppoints",_jit_sym_long,2,attrflags,
		(method)0L,(method)jit_gl_hap_looppoints,0,calcoffset(t_jit_gl_hap,looppoints));
	jit_class_addattr(_jit_gl_hap_class,attr);
		
	attr = jit_object_new(_jit_sym_jit_attr_offset,"loopreport",_jit_sym_char,attrflags,
		(method)0L,(method)0L,calcoffset(t_jit_gl_hap,loopreport));
	jit_class_addattr(_jit_gl_hap_class,attr);
	object_addattr_parse(attr,"style",_jit_sym_symbol,0,"onoff");
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"framereport",_jit_sym_char,attrflags,
		(method)0L,(method)0L,calcoffset(t_jit_gl_hap, framereport));
	jit_class_addattr(_jit_gl_hap_class,attr);
	object_addattr_parse(attr,"style",_jit_sym_symbol,0,"onoff");
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"out_name",_jit_sym_symbol, attrflags,
		(method)jit_gl_hap_getattr_out_name,(method)jit_gl_hap_setattr_out_name,0);
	jit_class_addattr(_jit_gl_hap_class,attr);	

	attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_OPAQUE_USER;
	attr = jit_object_new(_jit_sym_jit_attr_offset,"fps",_jit_sym_float32,attrflags,
		(method)0L,(method)0L,calcoffset(t_jit_gl_hap,fps));
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"duration",_jit_sym_long,attrflags,
		(method)0L,(method)0L,calcoffset(t_jit_gl_hap,duration));
	jit_class_addattr(_jit_gl_hap_class,attr);
			
	attr = jit_object_new(_jit_sym_jit_attr_offset,"timescale",_jit_sym_long,attrflags,
		(method)0L,(method)0L,calcoffset(t_jit_gl_hap,timescale));
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset,"framecount",_jit_sym_long,attrflags,
		(method)0L,(method)0L,calcoffset(t_jit_gl_hap,framecount));
	jit_class_addattr(_jit_gl_hap_class,attr);
				
	// hide default ob3d attrs that aren't used
	attrflags = JIT_ATTR_GET_OPAQUE_USER | JIT_ATTR_SET_OPAQUE_USER;
	attr = jit_object_new(_jit_sym_jit_attr_offset_array,"position",_jit_sym_float32,3,attrflags,
		(method)0L,(method)0L,0,0);
	jit_class_addattr(_jit_gl_hap_class,attr);
	attr = jit_object_new(_jit_sym_jit_attr_offset_array,"anchor",_jit_sym_float32,3,attrflags,
		(method)0L,(method)0L,0,0);
	jit_class_addattr(_jit_gl_hap_class,attr);
	attr = jit_object_new(_jit_sym_jit_attr_offset,"anim",_jit_sym_symbol, attrflags,
		(method)0L,(method)0L,0);
	jit_class_addattr(_jit_gl_hap_class,attr);
	attr = jit_object_new(_jit_sym_jit_attr_offset,"animmode",_jit_sym_symbol, attrflags,
		(method)0L,(method)0L,0);
	jit_class_addattr(_jit_gl_hap_class,attr);
	attr = jit_object_new(_jit_sym_jit_attr_offset,"filterclass",_jit_sym_symbol, attrflags,
		(method)0L,(method)0L,0);
	jit_class_addattr(_jit_gl_hap_class,attr);
	attr = jit_object_new(_jit_sym_jit_attr_offset,"layer",_jit_sym_long,attrflags,
		(method)0L,(method)0L,0L);
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	jit_class_register(_jit_gl_hap_class);

	ps_bind = gensym("bind");
	ps_unbind = gensym("unbind");
	ps_width = gensym("width");
	ps_height = gensym("height");
	ps_glid = gensym("glid");	
	ps_draw = gensym("draw");

	return JIT_ERR_NONE;
}

t_jit_gl_hap *jit_gl_hap_new(t_symbol * dest_name)
{
	t_jit_gl_hap *x;
	
	if (x = (t_jit_gl_hap *)jit_object_alloc(_jit_gl_hap_class)) {
		jit_ob3d_new(x, dest_name);

		jit_gl_hap_new_native(x);
		x->has_video = 0;
		x->file = _jit_sym_nothing;
		x->texoutput = jit_object_new(gensym("jit_gl_texture"), dest_name);
		jit_attr_setsym(x->texoutput,gensym("defaultimage"),gensym("black"));
		jit_attr_setsym(x->texoutput,_jit_sym_name,jit_symbol_unique());
		jit_attr_setlong(x->texoutput, gensym("flip"), 0);
		
		x->hapglsl = jit_object_new(gensym("jit_gl_shader"), dest_name);
		
		x->buffer = NULL;
		x->hap_format = JIT_GL_HAP_PF_NONE;
		x->useshader = 0;
		x->validframe = 0;
		x->movieloaded = 0;
		x->deletetex = 0;
		x->texture = 0;
		x->dim[0] = x->dim[1] = 0;
		x->backingWidth = x->backingHeight = 0;
		x->roundedWidth = x->roundedHeight = 0;
		x->internalFormat = x->newInternalFormat = 0;
		x->newDataLength = x->rowLength = 0;
		x->target = 0;
		x->fboid = 0;
		
		x->direction = 1;
		x->suppress_loopnotify=0;
		x->userloop = 0;
		x->prevtime = 0;
		x->flipped = 0;
		
		x->adapt = 1;
		x->fps = 0;
		x->duration = 0;
		x->timescale = 0;
		x->framecount = 0;
		x->loop = JIT_GL_HAP_LOOP_ON;
		x->loopflags = 0;
		x->autostart = 1;
		x->rate = 1;
		x->vol = 1;
		x->rate_preserves_pitch = 1;
		x->looppoints[0] = x->looppoints[1] = -1;
		x->loopreport = 0;
		x->framereport = 0;
	}
	else {
		x = NULL;
	}	
	return x;
}

void jit_gl_hap_free(t_jit_gl_hap *x)
{
	if(x->fboid)
		glDeleteFramebuffersEXT(1, &x->fboid);
	
	if(x->texoutput) {
		jit_object_free(x->texoutput);
	}
	if(x->hapglsl) {
		jit_object_free(x->hapglsl);
	}
	
	jit_gl_hap_free_native(x);
	
	jit_ob3d_free(x);
}

void jit_gl_hap_notify_atomarray_prep(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av)
{
	if (ac && av) {
		t_object *arr;
		t_atom *a;
		
		if (a = (t_atom *)sysmem_newptr(sizeof(t_atom) * (ac + 1))) {
			jit_atom_setsym(a, s);
			sysmem_copyptr(av, a + 1, sizeof(t_atom) * ac);
			arr = (t_object *)object_new(gensym("nobox"), gensym("atomarray"), ac + 1, a);
			jit_object_notify(x, gensym("typedmess"), arr);
			freeobject(arr);
			sysmem_freeptr(a);
		}
	}
	else jit_object_notify(x, s, NULL);
}

void jit_gl_hap_read(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av)
{
	t_atom a[2];
	char fname[MAX_FILENAME_CHARS] = "";
	short vol;
	t_fourcc type;
	short ret;
	
	x->newfile = 0;
	
	if (ac && av) {
		strcpy(fname, atom_getsym(av)->s_name);
		ret = locatefile_extended(fname, &vol, &type, NULL, 0);			
	} else {
		ret = open_dialog(fname, &vol, &type, NULL, 0); // limit to movie files?
	}

	if (!ret) {
		jit_gl_hap_read_native(x, fname, vol);
		
		if(x->newfile) {
			jit_attr_user_touch(x, gensym("fps"));
			jit_attr_user_touch(x, gensym("duration"));
			jit_attr_user_touch(x, gensym("timescale"));
			jit_attr_user_touch(x, gensym("framecount"));
				
			// if user didn't modify looppoints, reset them
			//if(!x->userloop) {
				x->looppoints[0] = x->looppoints[1] = -1;
			//}
			// set attributes here
			jit_gl_hap_do_loop(x);
			// rate must init after playback starts
		}
	}
	jit_atom_setsym(a, x->file);
	jit_atom_setlong(a+1, x->newfile ? 1 : 0);	
	defer_low(x, (method)jit_gl_hap_notify_atomarray_prep, s, 2, a);	
}

#pragma mark -
#pragma mark open gl
#pragma mark -

t_jit_err jit_gl_hap_dest_changed(t_jit_gl_hap *x)
{
	t_symbol *dest_name = _jit_sym_nothing;
	t_jit_gl_drawinfo drawinfo;
	
	dest_name = jit_attr_getsym(x, gensym("drawto"));
	
	if(x->hapglsl) {
		jit_attr_setsym(x->hapglsl, gensym("drawto"), dest_name);
		jit_object_method(x->hapglsl, gensym("readbuffer"), jit_gl_hap_glsl_jxs);
	}
	
	if(x->texoutput)
		jit_attr_setsym(x->texoutput, gensym("drawto"), dest_name);
		
	if(x->fboid != 0) {
		glDeleteFramebuffersEXT(1, &x->fboid);
		x->fboid = 0;
	}
	
	jit_object_method(x->texoutput, gensym("set_rebuild"));

	// our texture has to be bound in the new context before we can use it
	// http://cycling74.com/forums/topic.php?id=29197
	jit_gl_drawinfo_setup(x, &drawinfo);
	jit_gl_bindtexture(&drawinfo, jit_attr_getsym(x->texoutput, _jit_sym_name), 0);
	jit_gl_unbindtexture(&drawinfo, jit_attr_getsym(x->texoutput, _jit_sym_name), 0);

	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_draw(t_jit_gl_hap *x)
{
	t_jit_err result = JIT_ERR_NONE;
	t_jit_gl_drawinfo drawinfo;

	if(x->newfile) {
		if(x->texture && x->deletetex) {
			glDeleteTextures(1, &x->texture);
			x->deletetex = 0;
		}
		
		x->texture = 0;
		jit_gl_hap_load_file(x);
	}
	
	if(!x->movieloaded)
		return JIT_ERR_NONE;
		
	if(jit_gl_drawinfo_setup(x,&drawinfo))
		return JIT_ERR_GENERIC;
		
	jit_gl_hap_getcurrentframe(x);
	
	if(x->validframe) {

		GLint previousFBO;	// make sure we pop out to the right FBO
#ifdef MAC_VERSION
		GLint previousReadFBO;
		GLint previousDrawFBO;
#endif		
		// We are going to bind our FBO to our internal jit.gl.texture as COLOR_0 attachment
		// We need the ID, width/height.
		GLuint texid = jit_attr_getlong(x->texoutput,ps_glid);
		GLuint width = jit_attr_getlong(x->texoutput,ps_width);
		GLuint height = jit_attr_getlong(x->texoutput,ps_height);

		glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previousFBO);
#ifdef MAC_VERSION
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &previousReadFBO);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &previousDrawFBO);
#endif
		
		if(width!=x->dim[0] || height!=x->dim[1]) {
			width = x->dim[0];
			height = x->dim[1];
			jit_attr_setlong_array(x->texoutput, gensym("dim"), 2, x->dim);
			jit_attr_user_touch(x, gensym("dim"));
		}
		
		if(x->hap_format == JIT_GL_HAP_PF_HAP) {
			jit_gl_hap_submit_texture(x);
			jit_gl_report_error("jit.gl.hap submit texture");
		}
		else if(x->hap_format != JIT_GL_HAP_PF_GL) {
			jit_gl_hap_submit_nonhap_texture(x);
			jit_gl_report_error("jit.gl.hap submit non-hap texture");
		}
		
		if(jit_gl_hap_draw_begin(x, texid, width, height)) {
			jit_gl_report_error("jit.gl.hap draw begin");
			jit_gl_hap_dodraw(x, width, height);
			jit_gl_report_error("jit.gl.hap draw texture to FBO");
			jit_gl_hap_draw_end(x);
			jit_gl_report_error("jit.gl.hap draw end");
		}
		else {
			jit_object_error((t_object*)x, "could not bind to FBO");
		}

		glPopClientAttrib();
		glPopAttrib();
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, previousFBO);
#ifdef MAC_VERSION
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, previousReadFBO);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, previousDrawFBO);
#endif

		x->validframe = 0;
		
		jit_object_notify(x, ps_draw, NULL);
		jit_gl_hap_do_report(x);
	}
	
	jit_gl_hap_releaseframe(x);

	return result;
}

t_bool jit_gl_hap_draw_begin(t_jit_gl_hap *x, GLuint texid, GLuint width, GLuint height)
{
	GLenum status;

	if(x->fboid == 0)
		glGenFramebuffersEXT(1, &x->fboid);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, x->fboid);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, texid, 0);
	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

	// moving this to after bindframebuffer seems to fix problems on windows
	// http://www.opengl.org/discussion_boards/showthread.php/165636-Invalid-Operation-glDrawBuffer-glPopAttrib
	glPushAttrib(GL_VIEWPORT_BIT|GL_COLOR_BUFFER_BIT);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glViewport(0, 0,  width, height);
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glLoadIdentity();
		
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0, width,  0.0,  height, -1, 1);		
		
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		
		glColor4f(0.0, 0.0, 0.0, 1.0);
		// do not need blending if we use black border for alpha and replace env mode, saves a buffer wipe
		// we can do this since our image draws over the complete surface of the FBO, no pixel goes untouched.
		glDisable(GL_BLEND);
		glDisable(GL_LIGHTING);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);	
		glTexParameterf( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameterf( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameterf( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		
		return 1;
	}
	return 0;
}

void jit_gl_hap_dodraw(t_jit_gl_hap *x, GLuint width, GLuint height)
{
	GLfloat verts[] = {
		0.0,(GLfloat)height,
		(GLfloat)width,(GLfloat)height,
		(GLfloat)width,0.0,
		0.0,0.0
	};
	
	GLfloat tex_coords[] = {
		0.0, (GLfloat)height,
		(GLfloat)width, (GLfloat)height,
		(GLfloat)width, 0.0,
		0.0, 0.0
	};
	
	if(x->target == GL_TEXTURE_2D) {
		tex_coords[1] /= (float)x->backingHeight;
		tex_coords[3] /= (float)x->backingHeight;
		tex_coords[5] /= (float)x->backingHeight;
		tex_coords[7] /= (float)x->backingHeight;
		tex_coords[2] /= (float)x->backingWidth;
		tex_coords[4] /= (float)x->backingWidth;
	}
	
	glEnable(x->target);
	glBindTexture(x->target,x->texture);
	
	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer(2, GL_FLOAT, 0, tex_coords );
	glEnableClientState(GL_VERTEX_ARRAY);		
	glVertexPointer(2, GL_FLOAT, 0, verts );
	
	if(x->useshader)
		jit_object_method(x->hapglsl, ps_bind, ps_bind, 0, 0);
	
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
	
	if(x->useshader)
		jit_object_method(x->hapglsl, ps_unbind, ps_unbind, 0, 0);
}

void jit_gl_hap_draw_end(t_jit_gl_hap *x)
{
	glBindTexture(x->target,0);
	glDisable(x->target);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
}

void jit_gl_hap_submit_texture(t_jit_gl_hap *x)
{
	GLenum type;
	GLvoid *baseAddress = CVPixelBufferGetBaseAddress(x->buffer);
	
	glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);
	glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
		
	// Create a new texture if our current one isn't adequate
	if (
		!x->texture ||
		(x->roundedWidth > x->backingWidth) ||
		(x->roundedHeight > x->backingHeight) ||
		(x->newInternalFormat != x->internalFormat)
	) {
		glEnable(x->target);
		
		if (x->texture != 0) {
			glDeleteTextures(1, &x->texture);
		}
		glGenTextures(1, &x->texture);
		glBindTexture(x->target, x->texture);
		x->deletetex = 1;
		
		// On NVIDIA hardware there is a massive slowdown if DXT textures aren't POT-dimensioned, so we use POT-dimensioned backing
		x->backingWidth = 1;
		while (x->backingWidth < x->roundedWidth) x->backingWidth <<= 1;
		x->backingHeight = 1;
		while (x->backingHeight < x->roundedHeight) x->backingHeight <<= 1;
		
		glTexParameteri(x->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(x->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(x->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(x->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef MAC_VERSION
		glTexParameteri(x->target, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_SHARED_APPLE);
		type = GL_UNSIGNED_INT_8_8_8_8_REV;
#else
		type = GL_UNSIGNED_BYTE;
#endif

		// We allocate the texture with no pixel data, then use CompressedTexSubImage to update the content region			
		glTexImage2D(x->target, 0, x->newInternalFormat, x->backingWidth, x->backingHeight, 0, GL_BGRA, type, NULL);
		
		x->internalFormat = x->newInternalFormat;
	}
	else {
		glBindTexture(x->target, x->texture);
	}

#ifdef MAC_VERSION
	glTextureRangeAPPLE(GL_TEXTURE_2D, x->newDataLength, baseAddress);
	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
#else
// not sure what this should be, so leaving as default for now
//	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif

	glCompressedTexSubImage2D(GL_TEXTURE_2D,
							  0,
							  0,
							  0,
							  x->roundedWidth,
							  x->roundedHeight,
							  x->newInternalFormat,
							  x->newDataLength,
							  baseAddress);

	glPopClientAttrib();
	glPopAttrib();

}

void jit_gl_hap_submit_nonhap_texture(t_jit_gl_hap *x)
{
	GLenum type = GL_UNSIGNED_BYTE;
	GLenum format = (x->hap_format==JIT_GL_HAP_PF_RGB ? GL_RGB : GL_RGBA);
	GLvoid *baseAddress = CVPixelBufferGetBaseAddress(x->buffer);

	glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);
	glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
		
	// Create a new texture if our current one isn't adequate
	if (
		!x->texture ||
		(x->dim[0] > x->backingWidth) ||
		(x->dim[1] > x->backingHeight) ||
		(x->newInternalFormat != x->internalFormat)
	) {
		
		glEnable(x->target);
		
		if (x->texture != 0) {
			glDeleteTextures(1, &x->texture);
		}
		glGenTextures(1, &x->texture);
		glBindTexture(x->target, x->texture);
		x->deletetex = 1;
		
		x->backingWidth = x->dim[0];
		x->backingHeight = x->dim[1];
		
		glTexParameteri(x->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(x->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(x->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(x->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);		

		glTexImage2D(x->target, 0, x->newInternalFormat, x->backingWidth, x->backingHeight, 0, format, type, NULL);
		x->internalFormat = x->newInternalFormat;
	}
	else {
		glBindTexture(x->target, x->texture);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, x->rowLength);
	glTexSubImage2D(x->target, 0,0,0, x->roundedWidth, x->roundedHeight, format, type, baseAddress);

	glPopClientAttrib();
	glPopAttrib();

}
#pragma mark -
#pragma mark movie controls
#pragma mark -

void jit_gl_hap_clear_looppoints(t_jit_gl_hap *x)
{
	x->looppoints[0] = x->looppoints[1] = -1;
	if(x->movieloaded) {
		jit_gl_hap_do_looppoints(x);
		jit_attr_user_touch(x, gensym("loopstart"));
		jit_attr_user_touch(x, gensym("loopend"));
		jit_attr_user_touch(x, gensym("looppoints"));
	}
}

t_atom_long jit_gl_hap_frametotime(t_jit_gl_hap *x, t_atom_long frame)
{
	double spf = 1./x->fps;
	return (t_atom_long)((double)frame*spf*(double)x->timescale);
}

t_atom_long jit_gl_hap_timetoframe(t_jit_gl_hap *x, t_atom_long time)
{
	double spu = 1./x->timescale;
	return (t_atom_long)((double)time*spu*(double)x->fps);
}

t_jit_err jit_gl_hap_frame(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av)
{
	if (x->movieloaded && ac && av) {
		jit_gl_hap_do_set_time(x, jit_gl_hap_frametotime(x, jit_atom_getlong(av)));
	}
	return JIT_ERR_NONE;
}

void jit_gl_hap_looper_notify(t_jit_gl_hap *x)
{
	jit_object_notify(x, gensym("loopreport"), 0L);
}

void jit_gl_hap_do_report(t_jit_gl_hap *x)
{
	if(x->movieloaded) {
		t_atom_long curtime = jit_gl_hap_do_get_time(x);
		
		if(x->framereport) {
			t_atom a;
			t_atom_long frame = jit_gl_hap_timetoframe(x, curtime);
			jit_atom_setlong(&a, frame);
			jit_gl_hap_notify_atomarray_prep(x, gensym("framereport"), 1, &a);
		}
		
		if(x->loopreport&&!x->suppress_loopnotify) {					
			if(x->loop == JIT_GL_HAP_LOOP_PALINDROME) {
				if(x->direction) {
					if(curtime<=x->prevtime) {
						jit_gl_hap_looper_notify(x);
					}
				}
				else {
					if(curtime>x->prevtime) {
						jit_gl_hap_looper_notify(x);
					}
				}
			}
			else if(x->loop == JIT_GL_HAP_LOOP_ON){
				if (x->rate>0) {
					if(curtime<x->prevtime) {
						jit_gl_hap_looper_notify(x);
					}
				} else {
					if(curtime>x->prevtime) {
						jit_gl_hap_looper_notify(x);
					}
				}
			}
			else if(x->loop == JIT_GL_HAP_LOOP_OFF) {
				if (x->rate>0) {
					if ((curtime == x->duration) && (curtime>x->prevtime)) {
						jit_gl_hap_looper_notify(x);
					}
				} else {
					if ((curtime == 0) && (curtime<x->prevtime)) {
						jit_gl_hap_looper_notify(x);
					}
				}
			}
			else if(x->loop == JIT_GL_HAP_LOOP_LIMITS) {
				if (x->rate>0) {
					if ((curtime >= x->looppoints[1]) && (curtime>x->prevtime)) {
						jit_gl_hap_looper_notify(x);
					}
				} else {
					if ((curtime <= x->looppoints[0]) && (curtime<x->prevtime)) {
						jit_gl_hap_looper_notify(x);
					}
				}
			}
		}
		
		if(x->loop == JIT_GL_HAP_LOOP_PALINDROME)
			x->direction = curtime > x->prevtime;
		else
			x->direction = 1;
		
		//jit_attr_user_touch(x, ps_time);
		x->suppress_loopnotify = 0;
		x->prevtime = curtime;
	}
}

t_jit_err jit_gl_hap_loadram(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av)
{
	t_atom_long al;
	t_atom_long from = 0, to = 0;
	short unload = (s == gensym("unloadram"));
	t_atom a;
	jit_atom_setlong(&a, 0);
	
	// 1 args end
	// 2 args = start, end
	if (x->movieloaded) {
		switch(ac) {
		case 1:
			if (!jit_atom_arg_getlong(&al, 0, ac, av)) {
				to = (t_atom_long) al;
			}
		case 2:
			if (!jit_atom_arg_getlong(&al, 0, ac, av)) {
				from = (t_atom_long) al; 
			}
			if (!jit_atom_arg_getlong(&al, 1, ac, av)) {
				to = (t_atom_long) al; 
			}
			break;
		default:
			break;
		}
		
		if(!jit_gl_hap_do_loadram(x, from, to, unload)) {
			jit_atom_setlong(&a, 1);
		}
	}
	
	jit_gl_hap_notify_atomarray_prep(x,s,1,&a);
	
	return JIT_ERR_NONE;
}

#pragma mark -
#pragma mark texture
#pragma mark -

void jit_gl_hap_sendoutput(t_jit_gl_hap *x, t_symbol *s, int argc, t_atom *argv)
{
	if (x->texoutput) {
		s = jit_atom_getsym(argv);
		
		argc--;
		if (argc) {
			argv++;
		}
		else {
			argv = NULL;
		}
		object_method_typed(x->texoutput,s,argc,argv,NULL);
	}
}

t_jit_err jit_gl_hap_getattr_out_name(t_jit_gl_hap *x, void *attr, long *ac, t_atom **av)
{
	if ((*ac)&&(*av)) {
		//memory passed in, use it
	} else {
		//otherwise allocate memory
		*ac = 1;
		if (!(*av = jit_getbytes(sizeof(t_atom)*(*ac)))) {
			*ac = 0;
			return JIT_ERR_OUT_OF_MEM;
		}
	}
	
	jit_atom_setsym(*av,jit_attr_getsym(x->texoutput,_jit_sym_name));
	
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_setattr_out_name(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av) {
		jit_attr_setsym(x->texoutput, _jit_sym_name, jit_atom_getsym(av));
	}
	return JIT_ERR_NONE;
}

	
#pragma mark -
#pragma mark attributes
#pragma mark -

t_jit_err jit_gl_hap_time_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av) 
{
	if (ac && av && x->movieloaded) {
		t_atom_long time = jit_atom_getlong(av);
		jit_gl_hap_do_set_time(x, time);
	}
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_time_get(t_jit_gl_hap *x, void *attr, long *ac, t_atom **av) 
{
	if ((*ac)&&(*av)) {
		//memory passed in, use it
	} else {
		//otherwise allocate memory
		*ac = 1;
		if (!(*av = jit_getbytes(sizeof(t_atom)*(*ac)))) {
			*ac = 0;
			return JIT_ERR_OUT_OF_MEM;
		}
	}
	jit_atom_setlong(*av,0);
	
	if (x->movieloaded) {
		(*av)->a_w.w_long = jit_gl_hap_do_get_time(x);
	}
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_loop_set(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av)
		x->loop = CLAMP((char)jit_atom_getlong(av), 0, 3);

	if(x->newfile || x->movieloaded)
		jit_gl_hap_do_loop(x);
		
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_loopstart(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av) {
		x->looppoints[0] = jit_atom_getlong(av);
		x->userloop = 1;
	}
	if(x->movieloaded)
		jit_gl_hap_do_looppoints(x);
		
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_loopend(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av) {
		x->looppoints[1] = jit_atom_getlong(av);
		x->userloop = 1;
	}
	if(x->movieloaded)
		jit_gl_hap_do_looppoints(x);
		
	return JIT_ERR_NONE;
}

t_jit_err jit_gl_hap_looppoints(t_jit_gl_hap *x, void *attr, long ac, t_atom *av)
{
	if (ac && av) {
		x->looppoints[0] = jit_atom_getlong(av);
		x->looppoints[1] = (ac>1 ? jit_atom_getlong(av+1) : 0);
		x->userloop = 1;
	}
	if(x->movieloaded)
		jit_gl_hap_do_looppoints(x);
		
	return JIT_ERR_NONE;
}

#define kHapPixelFormatTypeRGB_DXT1 'DXt1'
#define kHapPixelFormatTypeRGBA_DXT5 'DXT5'
#define kHapPixelFormatTypeYCoCg_DXT5 'DYt5'

#ifdef WIN_VERSION
enum CVPixelBufferLockFlags {
	kCVPixelBufferLock_ReadOnly = 0x00000001,
};
#endif

void jit_gl_hap_draw_frame(void *jitob, CVImageBufferRef frame)
{
	t_jit_gl_hap * x = (t_jit_gl_hap*)jitob;
	CFTypeID imageType = CFGetTypeID(frame);
	OSType newPixelFormat;

	if(x->validframe)
		return;
		
	if (imageType == CVPixelBufferGetTypeID()) {
        
        // Update the texture
        CVBufferRetain(frame);
		
		if(x->buffer) {
			CVPixelBufferUnlockBaseAddress(x->buffer, kCVPixelBufferLock_ReadOnly);
			CVBufferRelease(x->buffer);
		}
		
		x->buffer = frame;
		CVPixelBufferLockBaseAddress(x->buffer, kCVPixelBufferLock_ReadOnly);
		
		x->dim[0] = CVPixelBufferGetWidth(x->buffer);
		x->dim[1] = CVPixelBufferGetHeight(x->buffer);

		newPixelFormat = CVPixelBufferGetPixelFormatType(x->buffer);

		if(x->buffer && x->hap_format==JIT_GL_HAP_PF_HAP) {
			size_t extraRight, extraBottom;
			unsigned int bitsPerPixel;
			size_t bytesPerRow;
			size_t actualBufferSize;

			CVPixelBufferGetExtendedPixels(x->buffer, NULL, &extraRight, NULL, &extraBottom);
			x->roundedWidth = x->dim[0] + extraRight;
			x->roundedHeight = x->dim[1] + extraBottom;
			if (x->roundedWidth % 4 != 0 || x->roundedHeight % 4 != 0) {
				x->validframe = 0;
				return;
			}			

			switch (newPixelFormat) {
				case kHapPixelFormatTypeRGB_DXT1:
					x->newInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
					bitsPerPixel = 4;
					break;
				case kHapPixelFormatTypeRGBA_DXT5:
				case kHapPixelFormatTypeYCoCg_DXT5:
					x->newInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
					bitsPerPixel = 8;
					break;
				default:
					// we don't support non-DXT pixel buffers
					x->validframe = 0;
					return;
					break;
			}
			x->useshader = (newPixelFormat == kHapPixelFormatTypeYCoCg_DXT5);
			
			bytesPerRow = (x->roundedWidth * bitsPerPixel) / 8;
			x->newDataLength = bytesPerRow * x->roundedHeight; // usually not the full length of the buffer
			actualBufferSize = CVPixelBufferGetDataSize(x->buffer);
			
			// Check the buffer is as large as we expect it to be
			if (x->newDataLength > actualBufferSize) {
				x->validframe = 0;
				return;
			}

			// If we got this far we're good to go
			x->validframe = 1;
			x->target = GL_TEXTURE_2D;
			if(!x->flipped) {
				jit_attr_setlong(x->texoutput, gensym("flip"), 1);
				x->flipped = 1;
			}
			//x->drawhap = 1;
		}
		else if(x->buffer) {// && x->hap_format==JIT_GL_HAP_PF_HAP) {
			if( newPixelFormat == k24RGBPixelFormat )
				x->newInternalFormat = GL_RGB8;
			else if( newPixelFormat == k32BGRAPixelFormat )
				x->newInternalFormat = GL_RGBA8;
			else {
				x->validframe = 0;
				return;
			}

			x->roundedWidth = x->dim[0];
			x->roundedHeight = x->dim[1];
			x->newDataLength = CVPixelBufferGetDataSize(x->buffer);
			x->rowLength = CVPixelBufferGetBytesPerRow( x->buffer ) / (x->hap_format==JIT_GL_HAP_PF_RGB ? 3 : 4);
			x->target = GL_TEXTURE_RECTANGLE_EXT;
			
			if(!x->flipped) {
				jit_attr_setlong(x->texoutput, gensym("flip"), 1);
				x->flipped = 1;
			}
			x->validframe = 1;
		}
    }
	else {
#ifdef MAC_VERSION
		CGSize imageSize = CVImageBufferGetEncodedSize(frame);
		bool flipped = CVOpenGLTextureIsFlipped(frame);
		x->texture = CVOpenGLTextureGetName(frame);
		x->useshader = 0;
		x->dim[0] = (t_atom_long)imageSize.width;
		x->dim[1] = (t_atom_long)imageSize.height;
		x->validframe = 1;
		x->target = GL_TEXTURE_RECTANGLE_ARB;
		if(x->flipped!=flipped) {
			jit_attr_setlong(x->texoutput, gensym("flip"), flipped);
			x->flipped = flipped;			
		}
#endif
	}
}
#endif