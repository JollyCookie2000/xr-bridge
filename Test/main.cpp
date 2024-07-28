#include <memory>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "cube.hpp"

// IMPORTANT: You MUST define the platform you are using in your PROJECT settings.
// Define ONE of the following to choose the platform: XRBRIDGE_PLATFORM_WINDOWS, XRBRIDGE_PLATFORM_X11
// You can define XRBRIDGE_DEBUG at the project level to enable debug output for XrBridge.
#include "xrbridge.hpp"

static bool g_running = true;

int main(int argc, char** argv)
{
	// Setup some FreeGLUT stuff.
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitContextVersion(4, 4);
	glutInitContextProfile(GLUT_CORE_PROFILE);

	// Initialize FreeGLUT.
	glutInit(&argc, argv);

	// Create a FreeGLUT window. For this example, the window will be completely empty since
	//  we only render to the VR headset.
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutInitWindowSize(800, 600);
	const int window = glutCreateWindow("OpenXR Demo");

	// This is the callback that gets called when the window needs to be re-rendered.
	// For this example, we don't need it.
	glutDisplayFunc([] () {});

	// This is the callback that will be called when we close the window.
	glutCloseFunc([] () {
		g_running = false;
	});

	// Initialize GLEW.
	if (glewInit() != GLEW_OK)
	{
		throw "[ERROR] Failed to initialize GLEW.";
	}

	// Enable depth for OpenGL.
	glEnable(GL_DEPTH_TEST);

	// Create an instance of XrBridge.
	XrBridge::XrBridge xrbridge;

	// Initialize the XrBridge instance.
	// The string is the name of the application that appears on SteamVR. This is not
	//  really that important. You can put whatever.
	if (xrbridge.init("XrBridge Demo") == false)
	{
		std::cerr << "[ERROR] Failed to initialize XrBridge!" << std::endl;
		return 1;
	}

	// We hard-code a camera at position [0.0, 0.5, 0.5].
	// NOTE: 1 unit = 1 meter
	const glm::mat4 camera_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.5f));

	// Create an example cube.
	const Cube cube;

	while (g_running)
	{
		// Process FreeGLUT events.
		glutMainLoopEvent();

		// Update XrBridge. This should be called once per frame.
		if (xrbridge.update() == false)
		{
			std::cerr << "[ERROR] Failed to update XrBridge!" << std::endl;
			return 1;
		}

		// Render the scene.
		// The render function accepts a user-defined function (in this case a lambda).
		// This user-defined function will be called as many times as necessary
		//  (probably twice, once for each eye; it could also not be called at all)
		//  to render each view.
		const bool did_render = xrbridge.render([&] (const XrBridge::Eye eye, std::shared_ptr<Fbo> fbo, const glm::mat4 projection_matrix, const glm::mat4 view_matrix, const uint32_t width, const uint32_t height) {
			// Bind the FBO and set the viewport. This is not done automatically
			//  by XrBridge, so we must do it ourselves!
			fbo->render();

			// Clear the FBO.
			glClearColor(0.22f, 0.36f, 0.42f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Render the example cube.
			cube.render(
					projection_matrix *
					glm::inverse(view_matrix) *
					glm::inverse(camera_matrix) *
					glm::scale(glm::mat4(1.0f), glm::vec3(0.1f)));

			if (eye == XrBridge::Eye::LEFT)
			{
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				glBlitFramebuffer(0, 0, width, height, 0, 0, 800, 600, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			}
		});

		if (did_render == false)
		{
			std::cerr << "[ERROR] Failed to render!" << std::endl;
			return 1;
		}

		// Swap the buffers.
		glutSwapBuffers();
	}

	// Free up resources and destroy the OpenXR instance.
	// This must be called BEFORE destroying the OpenGL context!
	xrbridge.free();

	// Free up resources and destroy the OpenGL context.
	glutDestroyWindow(window);

	return 0;
}
