// TODO: In the final API, maybe offer a builder to more easily build an instance.

// https://github.com/KhronosGroup/OpenXR-Tutorials/tree/main

#define _CRT_SECURE_NO_WARNINGS // Disable annoying Visual Studio error about strncpy

#include <iostream>

#include <Windows.h> // This MUST be included BEFORE FreeGLUT.

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>

#include "xrbridge.hpp"

static bool g_running = true;

int main(int argc, char** argv)
{
	// This is supposed to be done by the user.
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitContextVersion(4, 4);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitContextFlags(GLUT_DEBUG);

	glutInit(&argc, argv);

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutInitWindowSize(800, 600);
	const int window = glutCreateWindow("OpenXR Demo");
	glutDisplayFunc([]() {});
	glutCloseFunc([]() {
		g_running = false;
	});

	glutDisplayFunc([] () { });

	if (glewInit() != GLEW_OK)
	{
		throw "[ERROR] Failed to initialize GLEW.";
	}

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);


	const HDC hdc = wglGetCurrentDC();
	const HGLRC hglrc = wglGetCurrentContext();
	if (hdc == NULL || hglrc == NULL)
	{
		std::cerr << "[ERROR] Failed to get native OpenGL context." << std::endl;
		return 1;
	}

	XrBridge::XrBridge xrbridge = XrBridge::XrBridge("XrBridge Demo", {}, { "XR_KHR_opengl_enable", "XR_EXT_debug_utils" }, hdc, hglrc);

	while (g_running)
	{
		xrbridge.update();

		xrbridge.render([&] (const XrBridge::Eye eye, const glm::mat4 projection_matrix, const glm::mat4 view_matrix) {
			if (eye == XrBridge::Eye::LEFT)
			{
				glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
			}
			else if (XrBridge::Eye::RIGHT)
			{
				glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
			}

			glClear(GL_COLOR_BUFFER_BIT);
		});

		glutSwapBuffers();

		glutMainLoopEvent();
	}

	glutLeaveMainLoop();

	return 0;
}
