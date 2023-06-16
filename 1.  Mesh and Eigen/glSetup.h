#pragma once
#ifndef __GL_SETUP_H_
#define __GL_SETUP_H_

#include <GL/glew.h>
#include <GLFW/glfw3.h>

extern bool fullScreen;
extern bool noMenuBar;

extern float screenScale;		// Portion of ther screen when not using full screen, �����츦 ��� ��, ������� ��� �κ��� ������ ���ΰ�?
extern int screenW, screenH;	// screenScale portion of the sreen
extern int windowW, windowH;	// Framebuffer Size // HIDPI? �츮 ���� ȭ���� ũ��� �� �ȼ��� 4���� �ȼ��� �̷���� ��찡 ���� -> hidpi
								// �׷��� FRAMEbuffer ������� ������� ��ũ�� ����� �ٸ� ���� �ִ�.
	
extern float aspect;			// Aspect Ratio = windowW/windowH
extern float dpiScaling;		// DPI scaling for HIDPI -> window Width : screenWidth ����.

extern int vsync;				// Vertical sync on/off <- ���۸� �ϼ���,

extern bool perspectiveView;	// Perspective or orthographic viewing
extern float fovy;				// Filed of view in the y direction
extern float nearDist;			// Near plane
extern float farDist;			// Far plane

GLFWwindow* initializeOpenGL(int argc, char* argv[], GLfloat bg[4],bool modern=false);
void		reshape(GLFWwindow* window, int w, int h);
void		setupProjectionMatrix();

void drawAxes(float l, float w);

#endif // __GL_SETUP_H_