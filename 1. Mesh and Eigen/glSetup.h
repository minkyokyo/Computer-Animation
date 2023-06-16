#pragma once
#ifndef __GL_SETUP_H_
#define __GL_SETUP_H_

#include <GL/glew.h>
#include <GLFW/glfw3.h>

extern bool fullScreen;
extern bool noMenuBar;

extern float screenScale;		// Portion of ther screen when not using full screen, 윈도우를 띄울 때, 모니터의 어느 부분을 차지할 것인가?
extern int screenW, screenH;	// screenScale portion of the sreen
extern int windowW, windowH;	// Framebuffer Size // HIDPI? 우리 실제 화면의 크기과 한 픽셀이 4개의 픽셀로 이루어진 경우가 많다 -> hidpi
								// 그래서 FRAMEbuffer 사이즈와 모니터의 스크린 사이즈가 다를 수도 있다.
	
extern float aspect;			// Aspect Ratio = windowW/windowH
extern float dpiScaling;		// DPI scaling for HIDPI -> window Width : screenWidth 비율.

extern int vsync;				// Vertical sync on/off <- 구글링 하셔유,

extern bool perspectiveView;	// Perspective or orthographic viewing
extern float fovy;				// Filed of view in the y direction
extern float nearDist;			// Near plane
extern float farDist;			// Far plane

GLFWwindow* initializeOpenGL(int argc, char* argv[], GLfloat bg[4],bool modern=false);
void		reshape(GLFWwindow* window, int w, int h);
void		setupProjectionMatrix();

void drawAxes(float l, float w);

#endif // __GL_SETUP_H_