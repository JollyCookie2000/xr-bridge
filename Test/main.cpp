// TODO: In the final API, maybe offer a builder to more easily build an instance.

// https://github.com/KhronosGroup/OpenXR-Tutorials/tree/main

#include <iostream>

#include <Windows.h> // This MUST be included BEFORE FreeGLUT.

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>

#include "xrbridge.hpp"

#include "cube.hpp"

#define INTEROCULAR_DISTANCE 0.06f // Expressed in METERS

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

	glEnable(GL_DEPTH_TEST);

	const HDC hdc = wglGetCurrentDC();
	const HGLRC hglrc = wglGetCurrentContext();
	if (hdc == NULL || hglrc == NULL)
	{
		std::cerr << "[ERROR] Failed to get native OpenGL context." << std::endl;
		return 1;
	}

	XrBridge::XrBridge xrbridge = XrBridge::XrBridge("XrBridge Demo", {}, { "XR_KHR_opengl_enable" }, hdc, hglrc);

	const glm::mat4 camera_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.5f));
	const Cube cube;

	while (g_running)
	{
		xrbridge.update();

		xrbridge.render([&] (const XrBridge::Eye eye, const glm::mat4 projection_matrix, const glm::mat4 view_matrix) {
			glClearColor(0.22f, 0.36f, 0.42f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			const float eye_shift = eye == XrBridge::Eye::LEFT ? INTEROCULAR_DISTANCE / -2.0f : INTEROCULAR_DISTANCE / 2.0f;
			const glm::mat4 eye_shift_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(eye_shift, 0.0f, 0.0f));

			cube.render(projection_matrix * glm::inverse(eye_shift_matrix) * glm::inverse(view_matrix) * glm::inverse(camera_matrix) * glm::scale(glm::mat4(1.0f), glm::vec3(0.1f)));
		});

		glutSwapBuffers();
		glutMainLoopEvent();
	}

	glutLeaveMainLoop();

	return 0;
}
