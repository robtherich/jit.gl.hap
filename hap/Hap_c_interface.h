//
//  Hap_c_interface.h
//  jit.gl.hap
//
//  Created by Robert Ramirez on 3/5/13.
//
//

#ifndef jit_gl_hap_Hap_c_interface_h
#define jit_gl_hap_Hap_c_interface_h

//#include <CoreVideo/CVImageBuffer.h>

// The four-character-codes used to describe the pixel-formats of DXT frames emitted by the Hap QuickTime codec.
#define kHapPixelFormatTypeRGB_DXT1 'DXt1'
#define kHapPixelFormatTypeRGBA_DXT5 'DXT5'
#define kHapPixelFormatTypeYCoCg_DXT5 'DYt5'

#ifdef __cplusplus
extern "C" {
#endif
	
	void *hapQT_new(void *jitob);
	void hapQT_free(void *instance);
	void hapQT_read(void *instance, const char *filePath);
	void hapQT_getCurFrame(void *instance);
	void hapQT_releaseCurFrame(void *instance);
	
	void jit_gl_hap_draw_frame(void *x, CVImageBufferRef frame);
	
#ifdef __cplusplus
}
#endif

#endif
