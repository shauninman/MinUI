#ifndef GL_HEADERS_H
#define GL_HEADERS_H

#ifdef USE_GL
#    //pragma message("Compiling with OpenGL")
#    include <OpenGL/gl3.h>
#    include <OpenGL/gl3ext.h>
#    define WHICH_GL "OpenGL"
#else
#    //pragma message("Compiling with OpenGL ES")
#    define GL_GLEXT_PROTOTYPES 1
#    include <GLES3/gl3.h>
#    define WHICH_GL "GLESv2"
#endif

#endif // GL_HEADERS_H