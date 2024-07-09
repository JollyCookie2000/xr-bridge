#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "cube.hpp"

#include "xrbridge.hpp"

static bool g_running = true;

int main(int argc, char** argv)
{
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitContextVersion(4, 4);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitContextFlags(GLUT_DEBUG);

	glutInit(&argc, argv);

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutInitWindowSize(800, 600);
	const int window = glutCreateWindow("OpenXR Demo");
	glutDisplayFunc([] () {});
	glutCloseFunc([] () {
		g_running = false;
	});

	if (glewInit() != GLEW_OK)
	{
		throw "[ERROR] Failed to initialize GLEW.";
	}

	glEnable(GL_DEPTH_TEST);

	XrBridge::XrBridge xrbridge = XrBridge::XrBridge("XrBridge Demo");

	const glm::mat4 camera_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.5f));
	const Cube cube;

	while (g_running)
	{
		xrbridge.update();

		xrbridge.render([&] (const XrBridge::Eye eye, const glm::mat4 projection_matrix, const glm::mat4 view_matrix) {
			glClearColor(0.22f, 0.36f, 0.42f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			cube.render(
					projection_matrix *
					glm::inverse(view_matrix) *
					glm::inverse(camera_matrix) *
					glm::scale(glm::mat4(1.0f), glm::vec3(0.1f)));
		});

		glutSwapBuffers();
		glutMainLoopEvent();
	}

	glutLeaveMainLoop();

	return 0;
}
