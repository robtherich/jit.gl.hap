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
		
	void				*texoutput;	// texture object for output
	void				*hapglsl;	// shader for hap conversion
	
	HapQuickTimePlayback	*hap;
		
	char				useshader;
	char				newfile;
	CVPixelBufferRef	buffer;
	char				validframe;
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

static t_symbol *ps_bind;
static t_symbol *ps_unbind;

// --------------------------------------------------------------------------------

t_jit_err jit_gl_hap_init(void) 
{
	long ob3d_flags = JIT_OB3D_NO_MATRIXOUTPUT; // no matrix output
	void *ob3d;
	
	_jit_gl_hap_class = jit_class_new("jit_gl_hap", 
		(method)jit_gl_hap_new, (method)jit_gl_hap_free,
		sizeof(t_jit_gl_hap),A_DEFSYM,0L);
	
	ob3d = jit_ob3d_setup(_jit_gl_hap_class, calcoffset(t_jit_gl_hap, ob3d), ob3d_flags);

	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_draw, "ob3d_draw", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_dest_closing, "dest_closing", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_dest_changed, "dest_changed", A_CANT, 0L);
	jit_class_addmethod(_jit_gl_hap_class, (method)jit_object_register,	"register", A_CANT, 0L);

	jit_class_addmethod(_jit_gl_hap_class, (method)jit_gl_hap_read,		"read", A_GIMME, 0);

	jit_class_register(_jit_gl_hap_class);

	ps_bind = gensym("bind");
	ps_unbind = gensym("unbind");
	
	return JIT_ERR_NONE;
}


t_jit_gl_hap *jit_gl_hap_new(t_symbol * dest_name)
{
	t_jit_gl_hap *x;

	if (x = (t_jit_gl_hap *)jit_object_alloc(_jit_gl_hap_class)) {
		jit_ob3d_new(x, dest_name);

		x->hap = [[HapQuickTimePlayback alloc] init];
		if(x->hap)
			[x->hap setJitob:x];
		
		x->file = _jit_sym_nothing;
		x->texoutput = jit_object_new(gensym("jit_gl_texture"), dest_name);
		x->hapglsl = jit_object_new(gensym("jit_gl_shader"), dest_name);
		x->buffer = NULL;
		x->useshader = 0;
		x->validframe = 0;
		x->texture = 0;
		x->width = x->height = 0;
		x->backingWidth = x->backingHeight = 0;
		x->roundedWidth = x->roundedHeight = 0;
		x->internalFormat = x->newInternalFormat = 0;
		x->newDataLength = 0;
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
		[x->hap read:(const char *)x->file->s_name];
		x->newfile = 0;
	}
	
	[x->hap getCurFrame];
	
	if(jit_gl_drawinfo_setup(x,&drawinfo)) return result;
	
	if(x->validframe) {	
		
		glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);
		glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
		glEnable(GL_TEXTURE_2D);
				
		GLvoid *baseAddress = CVPixelBufferGetBaseAddress(x->buffer);

		// Create a new texture if our current one isn't adequate
		if (
			!x->texture ||
			(x->roundedWidth > x->backingWidth) ||
			(x->roundedHeight > x->backingHeight) ||
			(x->newInternalFormat != x->internalFormat)
		) {
			if (x->texture != 0) {
				glDeleteTextures(1, &x->texture);
			}
			glGenTextures(1, &x->texture);
			glBindTexture(GL_TEXTURE_2D, x->texture);
			
			// On NVIDIA hardware there is a massive slowdown if DXT textures aren't POT-dimensioned, so we use POT-dimensioned backing
			x->backingWidth = 1;
			while (x->backingWidth < x->roundedWidth) x->backingWidth <<= 1;
			x->backingHeight = 1;
			while (x->backingHeight < x->roundedHeight) x->backingHeight <<= 1;
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_SHARED_APPLE);
			
			// We allocate the texture with no pixel data, then use CompressedTexSubImage to update the content region			
			glTexImage2D(GL_TEXTURE_2D, 0, x->newInternalFormat, x->backingWidth, x->backingHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);        
			
			x->internalFormat = x->newInternalFormat;
		}
		else {
			glBindTexture(GL_TEXTURE_2D, x->texture);
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
		

/////////////////////////////////////////////////////

		//CGLContextObj cgl_ctx = [[self openGLContext] CGLContextObj];
		//CGLLockContext(cgl_ctx);
		//NSRect bounds = self.bounds;

		GLsizei width = 320;
		GLsizei height = 240;
		GLsizei destrectw = 0;
		GLsizei destrecth = 0;
				
		glColor4f(1,1,1,1);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, GL_FASTEST);
        
        glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
		glPushMatrix();
        glLoadIdentity();
        glViewport(0, 0, width, height);
        glOrtho(0, width, 0, height, -1.0, 1.0);
        
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        
        // clear the view if the texture won't fill it
        glClearColor(0.0,0.0,0.0,0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_TEXTURE_2D);
        
		double bAspect = width/height;
		double aAspect = x->width/x->height;
        
        // if the rect i'm trying to fit stuff *into* is wider than the rect i'm resizing
        if (bAspect > aAspect){
            destrecth = height;
            destrectw = destrecth * aAspect;
        }
        // else if the rect i'm resizing is wider than the rect it's going into
        else if (bAspect < aAspect) {
            destrectw = width;
            destrecth = destrectw / aAspect;
        }
        else {
            destrectw = width;
            destrecth = height;
        }
		
        //destRect.origin.x = (bounds.size.width-destRect.size.width)/2.0+bounds.origin.x;
        //destRect.origin.y = (bounds.size.height-destRect.size.height)/2.0+bounds.origin.y;
        
        GLfloat vertices[] =
        {
            0, 0,
            destrectw, 0,
            destrectw, destrecth,
            0,destrecth
        };
        
        GLfloat texCoords[] =
        {
            0.0, x->height,
            x->width, x->height,
            x->width, 0.0,
            0.0, 0.0
        };
        
		texCoords[1] /= (float)x->backingHeight;
		texCoords[3] /= (float)x->backingHeight;
		texCoords[5] /= (float)x->backingHeight;
		texCoords[7] /= (float)x->backingHeight;
		texCoords[2] /= (float)x->backingWidth;
		texCoords[4] /= (float)x->backingWidth;
        
        glBindTexture(GL_TEXTURE_2D,x->texture);
        
        glVertexPointer(2,GL_FLOAT,0,vertices);
        glTexCoordPointer(2,GL_FLOAT,0,texCoords);
        
		if(x->useshader)
			jit_object_method(x->hapglsl, ps_bind, ps_bind, 0, 0);
		
        glDrawArrays(GL_QUADS,0,4);
        jit_gl_report_error("hap_draw draw_arrays");
		
		if(x->useshader)
			jit_object_method(x->hapglsl, ps_unbind, ps_unbind, 0, 0);
		
        glBindTexture(GL_TEXTURE_2D,0);
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		
		jit_gl_report_error("hap_draw disable state");
		
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();

		x->validframe = 0;
	}
	
	[x->hap releaseCurFrame];
	jit_gl_report_error("hap_draw end");		
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
	dest_name = jit_attr_getsym(x, gensym("drawto"));
	if(x->hapglsl) {
		jit_attr_setsym(x->hapglsl, gensym("drawto"), dest_name);
		jit_object_method(x->hapglsl, gensym("readbuffer"), jit_gl_hap_glsl_jxs);
	}
	
	if(x->texoutput)
		jit_attr_setsym(x->texoutput, gensym("drawto"), dest_name);
		
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
		}
    }
	else {
		x->texture = CVOpenGLTextureGetName(frame);
		x->validframe = 1;
	}
}


