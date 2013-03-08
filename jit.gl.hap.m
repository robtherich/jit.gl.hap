// jit.gl.hap.c

#include "jit.common.h"
#include "jit.gl.h"

#include "HapQuickTimePlayback.h"
#include "jit.gl.hap.glsl.h"

typedef struct _jit_gl_hap 
{
	t_object			ob;
	void				*ob3d;
	t_symbol			*file;
		
	void					*texoutput;	// texture object for output
	void					*hapglsl;	// shader for hap conversion
	float					rect[4];	// output rectangle (MIN XY, MAX XY)
	t_atom_long				dim[2];		// output dim
	HapQuickTimePlayback	*hap;
		
	char				drawhap;
	char				useshader;
	char				newfile;
	char				deletetex;
	CVPixelBufferRef	buffer;
	char				validframe;
	char				movieloaded;
	GLuint          	texture;
	long				width;
	long				height;
    GLuint				backingHeight;
    GLuint				backingWidth;
    GLuint				roundedWidth;
    GLuint				roundedHeight;
	GLenum				internalFormat;
	GLenum				newInternalFormat;
	GLsizei				newDataLength;
	GLenum				target;
} t_jit_gl_hap;

void *_jit_gl_hap_class;

t_jit_err jit_gl_hap_init(void);
t_jit_gl_hap *jit_gl_hap_new(t_symbol * dest_name);
void jit_gl_hap_free(t_jit_gl_hap *x);

t_jit_err jit_gl_hap_draw(t_jit_gl_hap *x);
t_jit_err jit_gl_hap_dest_closing(t_jit_gl_hap *x);
t_jit_err jit_gl_hap_dest_changed(t_jit_gl_hap *x);
t_jit_err build_simple_chunk(t_jit_gl_hap *x);

void jit_gl_hap_read(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av);
void jit_gl_hap_sendoutput(t_jit_gl_hap *x, t_symbol *s, int argc, t_atom *argv);

t_bool jit_gl_hap_draw_begin(t_jit_gl_hap *x, GLuint texid, GLuint width, GLuint height);
void jit_gl_hap_dodraw(t_jit_gl_hap *x, GLuint width, GLuint height);
void jit_gl_hap_draw_end(t_jit_gl_hap *x);
void jit_gl_hap_submit_texture(t_jit_gl_hap *x);

t_jit_err jit_gl_hap_getattr_out_name(t_jit_gl_hap *x, void *attr, long *ac, t_atom **av);

static t_symbol *ps_bind;
static t_symbol *ps_unbind;
static t_symbol *ps_width;
static t_symbol *ps_height;
static t_symbol *ps_glid;

static GLuint tempFBO = 0;

// --------------------------------------------------------------------------------

t_jit_err jit_gl_hap_init(void) 
{
	void *ob3d;
	void *attr;
	long attrflags=0;
	long ob3d_flags = JIT_OB3D_NO_ROTATION_SCALE;
	ob3d_flags |= JIT_OB3D_NO_POLY_VARS;
	ob3d_flags |= JIT_OB3D_NO_FOG;
	ob3d_flags |= JIT_OB3D_NO_MATRIXOUTPUT;
	ob3d_flags |= JIT_OB3D_NO_LIGHTING_MATERIAL;
	//ob3d_flags |= JIT_OB3D_IS_SLAB;
	ob3d_flags |= JIT_OB3D_NO_BOUNDS;
	
	_jit_gl_hap_class = jit_class_new("jit_gl_hap", 
		(method)jit_gl_hap_new, (method)jit_gl_hap_free,
		sizeof(t_jit_gl_hap),A_DEFSYM,0L);
	
	ob3d = jit_ob3d_setup(_jit_gl_hap_class, calcoffset(t_jit_gl_hap, ob3d), ob3d_flags);

	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_draw, "ob3d_draw", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_dest_closing, "dest_closing", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_dest_changed, "dest_changed", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_object_register,	"register", A_CANT, 0L);

	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_read,		"read", A_GIMME, 0);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_sendoutput, "sendoutput", A_DEFER_LOW, 0L);
	
	attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
	
	attr = jit_object_new(_jit_sym_jit_attr_offset_array,"dim",_jit_sym_long,2,attrflags,
		(method)0L,(method)0L,0,calcoffset(t_jit_gl_hap,dim));
	jit_class_addattr(_jit_gl_hap_class,attr);
	
	//attr = jit_object_new(_jit_sym_jit_attr_offset_array,"rect",_jit_sym_float32,4,attrflags,
	//	(method)0L,(method)0L,0,calcoffset(t_jit_gl_hap,rect));
	//jit_class_addattr(_jit_gl_hap_class,attr);
	
	attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_OPAQUE_USER;
	attr = jit_object_new(_jit_sym_jit_attr_offset,"out_name",_jit_sym_symbol, attrflags,
		(method)jit_gl_hap_getattr_out_name,(method)0L,0);	
	jit_class_addattr(_jit_gl_hap_class,attr);	
		
	jit_class_register(_jit_gl_hap_class);

	ps_bind = gensym("bind");
	ps_unbind = gensym("unbind");
	ps_width = gensym("width");
	ps_height = gensym("height");
	ps_glid = gensym("glid");	
	
	return JIT_ERR_NONE;
}

t_jit_gl_hap *jit_gl_hap_new(t_symbol * dest_name)
{
	t_jit_gl_hap *x;
	t_atom av[4];
	
	if (x = (t_jit_gl_hap *)jit_object_alloc(_jit_gl_hap_class)) {
		jit_ob3d_new(x, dest_name);

		x->hap = [[HapQuickTimePlayback alloc] init];
		if(x->hap)
			[x->hap setJitob:x];
		
		x->file = _jit_sym_nothing;
		x->texoutput = jit_object_new(gensym("jit_gl_texture"), dest_name);
		jit_attr_setsym(x->texoutput,gensym("defaultimage"),gensym("black"));
		jit_attr_setsym(x->texoutput,_jit_sym_name,jit_symbol_unique());
		
		x->hapglsl = jit_object_new(gensym("jit_gl_shader"), dest_name);
		
		x->buffer = NULL;
		x->drawhap = 1;
		x->useshader = 0;
		x->validframe = 0;
		x->movieloaded = 0;
		x->deletetex = 0;
		x->texture = 0;
		x->width = x->height = 0;
		x->backingWidth = x->backingHeight = 0;
		x->roundedWidth = x->roundedHeight = 0;
		x->internalFormat = x->newInternalFormat = 0;
		x->newDataLength = 0;
		x->target = 0;
		
		// set color to white
		jit_atom_setfloat(av,1.);
		jit_atom_setfloat(av+1,1.);
		jit_atom_setfloat(av+2,1.);
		jit_atom_setfloat(av+3,1.);
		jit_object_method(x,gensym("color"),4,av);		
	}
	else {
		x = NULL;
	}	
	return x;
}

void jit_gl_hap_free(t_jit_gl_hap *x)
{
	if(x->texoutput) {
		jit_object_free(x->texoutput);
	}
	if(x->hapglsl) {
		jit_object_free(x->hapglsl);
	}
	[x->hap release];
	jit_ob3d_free(x);
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
		
		if(![x->hap read:(const char *)x->file->s_name]) {
			if([x->hap lasterror]) {
				NSString * description = [[x->hap lasterror] localizedDescription];
				jit_object_error((t_object*)x, "error loading quicktime movie: %s", [description UTF8String]);
			}
			else {
				jit_object_error((t_object*)x, "unknown error loading quicktime movie");
			}
			return JIT_ERR_GENERIC;
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
		}
	}
	
	if(!x->movieloaded)
		return JIT_ERR_NONE;
		
	if(jit_gl_drawinfo_setup(x,&drawinfo))
		return JIT_ERR_GENERIC;
		
	[x->hap getCurFrame];
	
	if(x->validframe) {

		GLint previousFBO;	// make sure we pop out to the right FBO
		GLint previousReadFBO;
		GLint previousDrawFBO;
		GLint previousMatrixMode;
		
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previousFBO);
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &previousReadFBO);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &previousDrawFBO);
		glGetIntegerv(GL_MATRIX_MODE, &previousMatrixMode);
				
		// We are going to bind our FBO to our internal jit.gl.texture as COLOR_0 attachment
		// We need the ID, width/height.
		GLuint texid = jit_attr_getlong(x->texoutput,ps_glid);
		GLuint width = jit_attr_getlong(x->texoutput,ps_width);
		GLuint height = jit_attr_getlong(x->texoutput,ps_height);
		
		if(width!=x->width || height!=x->height) {
			long dim[2];
			dim[0] = width = x->width;
			dim[1] = height = x->height;
			jit_attr_setlong_array(x->texoutput, gensym("dim"), 2, dim);
		}
		
		if(x->drawhap) {
			jit_gl_hap_submit_texture(x);
			jit_gl_report_error("jit.gl.hap submit texture");
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
		
		glBindFramebufferEXT(GL_FRAMEBUFFER, previousFBO);	
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER, previousReadFBO);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, previousDrawFBO);

		x->validframe = 0;
	}
	
	[x->hap releaseCurFrame];
	
	return result;
}

t_jit_err jit_gl_hap_dest_closing(t_jit_gl_hap *x)
{
	// nothing in this object to free. 
	return JIT_ERR_NONE;
}

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
		
	if(tempFBO != 0) {
		glDeleteFramebuffers(1, &tempFBO);
		tempFBO = 0;
	}
	
	// our texture has to be bound in the new context before we can use it
	// http://cycling74.com/forums/topic.php?id=29197
	jit_gl_drawinfo_setup(x, &drawinfo);
	jit_gl_bindtexture(&drawinfo, jit_attr_getsym(x->texoutput, _jit_sym_name), 0);
	jit_gl_unbindtexture(&drawinfo, jit_attr_getsym(x->texoutput, _jit_sym_name), 0);

	return JIT_ERR_NONE;
}

void jit_gl_hap_read(t_jit_gl_hap *x, t_symbol *s, long ac, t_atom *av)
{
	Boolean rv;
	
	if (x->hap) {
		char fname[MAX_FILENAME_CHARS] = "";
		short vol;
		t_fourcc type;
		short ret;
		
		if (ac && av) {
			strcpy(fname, atom_getsym(av)->s_name);
			ret = locatefile_extended(fname, &vol, &type, NULL, 0);			
		} else {
			ret = open_dialog(fname, &vol, &type, NULL, 0); // limit to movie files?
		}
		if (!ret) {
			char fpath[MAX_PATH_CHARS];
			char cpath[MAX_PATH_CHARS];
			
			path_topathname(vol, fname, fpath);
			path_nameconform(fpath, cpath, PATH_STYLE_SLASH, PATH_TYPE_BOOT);
			
			//rv = x->gmi->loadFromFile((const char *)cpath);
			x->file = gensym(cpath);
			x->newfile = 1;
			//hapQT_read(x->hap, (const char *)cpath);

			//post("loadFromFile returned %d", rv);
			//if (rv) {
			//	char filepath[MAX_PATH_CHARS] = "";
				//x->gmi->getLoadedMovieLocation(filepath, MAX_PATH_CHARS);
			//	post("movie loaded from %s", filepath);
			//}
		}		
	}
}

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
	// jit_object_post((t_object *)x,"jit.gl.slab: sending output: %s", JIT_SYM_SAFECSTR(jit_attr_getsym(x->output,_jit_sym_name)));
	
	return JIT_ERR_NONE;
}

t_bool jit_gl_hap_draw_begin(t_jit_gl_hap *x, GLuint texid, GLuint width, GLuint height)
{
	// save texture state, client state, etc.
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	// FBO generation/attachment to texture
	if(tempFBO == 0)
		glGenFramebuffers(1, &tempFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, tempFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, texid, 0);
	
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status == GL_FRAMEBUFFER_COMPLETE) {
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
		0.0,height,
		width,height,
		width,0.0,
		0.0,0.0
	};
	
	GLfloat tex_coords[] = {
		0.0, height,
		width, height,
		width, 0.0,
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
	
	glDisableClientState(GL_VERTEX_ARRAY);
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
		glTexParameteri(x->target, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_SHARED_APPLE);
		
		// We allocate the texture with no pixel data, then use CompressedTexSubImage to update the content region			
		glTexImage2D(x->target, 0, x->newInternalFormat, x->backingWidth, x->backingHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);        
		
		x->internalFormat = x->newInternalFormat;
	}
	else {
		glBindTexture(x->target, x->texture);
	}

	glTextureRangeAPPLE(GL_TEXTURE_2D, x->newDataLength, baseAddress);
	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

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

#define kHapPixelFormatTypeRGB_DXT1 'DXt1'
#define kHapPixelFormatTypeRGBA_DXT5 'DXT5'
#define kHapPixelFormatTypeYCoCg_DXT5 'DYt5'

void jit_gl_hap_draw_frame(void *jitob, CVImageBufferRef frame)
{
	t_jit_gl_hap * x = (t_jit_gl_hap*)jitob;
	CFTypeID imageType = CFGetTypeID(frame);
	
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
		
		x->width = CVPixelBufferGetWidth(x->buffer);
		x->height = CVPixelBufferGetHeight(x->buffer);

		if(x->buffer) {
			size_t extraRight, extraBottom;

			CVPixelBufferGetExtendedPixels(x->buffer, NULL, &extraRight, NULL, &extraBottom);
			x->roundedWidth = x->width + extraRight;
			x->roundedHeight = x->height + extraBottom;
			if (x->roundedWidth % 4 != 0 || x->roundedHeight % 4 != 0) {
				x->validframe = 0;
				return;
			}
			
			OSType newPixelFormat = CVPixelBufferGetPixelFormatType(x->buffer);
			unsigned int bitsPerPixel;

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
			
			size_t bytesPerRow = (x->roundedWidth * bitsPerPixel) / 8;
			x->newDataLength = bytesPerRow * x->roundedHeight; // usually not the full length of the buffer
			size_t actualBufferSize = CVPixelBufferGetDataSize(x->buffer);
			
			// Check the buffer is as large as we expect it to be
			if (x->newDataLength > actualBufferSize) {
				x->validframe = 0;
				return;
			}

			// If we got this far we're good to go
			x->validframe = 1;
			x->target = GL_TEXTURE_2D;
			if(!x->drawhap) {
				jit_attr_setlong(x->texoutput, gensym("flip"), 1);
				x->drawhap = 1;
			}
		}
    }
	else {
		CGSize imageSize = CVImageBufferGetEncodedSize(frame);
		x->texture = CVOpenGLTextureGetName(frame);
		x->useshader = 0;
		x->width = (long)imageSize.width;
		x->height = (long)imageSize.height;
		x->validframe = 1;
		x->target = GL_TEXTURE_RECTANGLE_ARB;
		if(x->drawhap) {
			jit_attr_setlong(x->texoutput, gensym("flip"), 0);
			x->drawhap = 0;
		}
	}
}

