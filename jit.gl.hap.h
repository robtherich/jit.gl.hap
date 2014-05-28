#ifndef __JIT_GL_HAP_H__
#define __JIT_GL_HAP_H__

#include "jit.common.h"
#include "jit.gl.h"
#include "jit.fixmath.h"
#include "HapSupport.h"

#ifdef WIN_VERSION

//#define GL_DRAW_FRAMEBUFFER_BINDING_EXT 0x8CA6
//#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
//#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
//#define GL_READ_FRAMEBUFFER_BINDING_EXT 0x8CAA

#define     kCharacteristicHasVideoFrameRate    FOUR_CHAR_CODE('vfrr')

#define XQT_NewDataReferenceFromMaxPath(path,name,ref,reftype,err) \
{ \
CFStringRef cfs; \
char tmpname[MAX_PATH_CHARS]; \
char pathname[MAX_PATH_CHARS]; \
(*(err)) = -1; \
(*(ref)) = NULL; \
if (!path_topotentialname(path,name,tmpname,FALSE)) { \
	if (!path_nameconform(tmpname,pathname,PATH_STYLE_NATIVE,PATH_TYPE_PATH)) { \
		cfs = CFStringCreateWithCString(kCFAllocatorDefault,pathname,kCFStringEncodingUTF8);\
		if (cfs) { \
			(*(err)) = QTNewDataReferenceFromFullPathCFString(cfs,kQTWindowsPathStyle,0,ref,reftype); \
			if (*(err)) (*(ref))=NULL; \
			CFRelease(cfs); \
		} \
	} \
} \
}

EXTERN_API_C( OSStatus )
QTPixelBufferContextCreate(
  CFAllocatorRef        allocator,                   /* can be NULL */
  CFDictionaryRef       attributes,                  /* can be NULL */
  QTVisualContextRef *  newPixelBufferContext);

#else

#ifdef __OBJC__
#include "HapQuickTimePlayback.h"
#else 
#define HapQuickTimePlayback void
#endif

#endif // WIN_VERSION

#define JIT_GL_HAP_LOOP_OFF			0
#define JIT_GL_HAP_LOOP_ON			1
#define JIT_GL_HAP_LOOP_PALINDROME	2
#define JIT_GL_HAP_LOOP_LIMITS		3

#define JIT_GL_HAP_PF_NONE			0
#define JIT_GL_HAP_PF_HAP			1
#define JIT_GL_HAP_PF_GL			2
#define JIT_GL_HAP_PF_RGB			3
#define JIT_GL_HAP_PF_RGBA			4
#define JIT_GL_HAP_PF_NO_VIDEO		5

typedef struct _t_jit_gl_hap
{
	t_object				ob;
	void					*ob3d;
	
	t_symbol				*file;
	void					*texoutput;	// texture object for output
	void					*hapglsl;	// shader for hap conversion
	//float					rect[4];	// output rectangle (MIN XY, MAX XY)
	t_atom_long				dim[2];		// output dim
	
#ifdef MAC_VERSION
	HapQuickTimePlayback	*hap;
#else
	Movie					movie;
	QTVisualContextRef      visualContext;
	CVImageBufferRef		currentImage;
#endif

	t_uint8				hap_format;
	char				has_video;
	char				useshader;
	char				newfile;
	char				deletetex;
	char				validframe;
	char				movieloaded;
	char				direction;
	char				suppress_loopnotify;
	char				userloop;
	t_atom_long			prevtime;
	char				flipped;
	
	char				adapt;
	float				fps;
	t_atom_long			duration;
	t_atom_long			framecount;
	t_atom_long			timescale;
	char				loop;
	long				loopflags;
	char				autostart;
	float				rate;
	float				vol;
	char				rate_preserves_pitch;
	t_atom_long			looppoints[2];
	char				loopreport;
	char				framereport;
	
	CVPixelBufferRef	buffer;
	GLuint          	texture;
    GLuint				backingHeight;
    GLuint				backingWidth;
    GLuint				roundedWidth;
    GLuint				roundedHeight;
	GLenum				internalFormat;
	GLenum				newInternalFormat;
	GLsizei				newDataLength;
	GLsizei				rowLength;
	GLenum				target;
	GLuint				fboid;
} t_jit_gl_hap;

void jit_gl_hap_new_native(t_jit_gl_hap *x);
void jit_gl_hap_free_native(t_jit_gl_hap *x);
void jit_gl_hap_read_native(t_jit_gl_hap *x, char *fname, short vol);
t_jit_err jit_gl_hap_load_file(t_jit_gl_hap *x);
void jit_gl_hap_dispose(t_jit_gl_hap *x);

void jit_gl_hap_do_loop(t_jit_gl_hap *x);
void jit_gl_hap_do_looppoints(t_jit_gl_hap *x);
void jit_gl_hap_do_set_time(t_jit_gl_hap *x, t_atom_long time);

t_bool jit_gl_hap_getcurrentframe(t_jit_gl_hap *x);
t_atom_long jit_gl_hap_frametotime(t_jit_gl_hap *x, t_atom_long frame);
t_atom_long jit_gl_hap_timetoframe(t_jit_gl_hap *x, t_atom_long time);
t_atom_long jit_gl_hap_do_get_time(t_jit_gl_hap *x);

void jit_gl_hap_draw_frame(void *x, CVImageBufferRef frame);
void jit_gl_hap_releaseframe(t_jit_gl_hap *x);

long jit_gl_hap_do_loadram(t_jit_gl_hap *x, t_atom_long from, t_atom_long to, short unload);

#endif
