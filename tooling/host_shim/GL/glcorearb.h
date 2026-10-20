#pragma once
#include <cstdint>
typedef unsigned int GLuint;
typedef unsigned int GLenum;
#define GL_NO_ERROR 0
typedef int GLsizei;
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_RGBA8 0x8058
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
extern "C" {
void glGenTextures(GLsizei,GLuint *);
void glBindTexture(GLenum,GLuint);
void glPixelStorei(GLenum,int);
GLenum glGetError(void);
void glTexImage2D(GLenum,int,int,GLsizei,GLsizei,int,GLenum,GLenum,const void *);
void glTexParameteri(GLenum,GLenum,int);
void glDeleteTextures(GLsizei,const GLuint *);
}
