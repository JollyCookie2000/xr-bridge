#include <memory>
#include <vector>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "cube.hpp"
#include "fbo.h"

#include "ovr.h"

static bool g_running = true;

int main(int argc, char** argv)
{
	// Setup some FreeGLUT stuff.
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitContextVersion(4, 4);
	glutInitContextProfile(GLUT_CORE_PROFILE);

	// Initialize FreeGLUT.
	glutInit(&argc, argv);

	// Create a FreeGLUT window.
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutInitWindowSize(800, 600);
	const int window = glutCreateWindow("OpenVR Demo");

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
		std::cerr << "[ERROR] Failed to initialize GLEW." << std::endl;
		return 1;
	}

	// Enable depth for OpenGL.
	glEnable(GL_DEPTH_TEST);

	// Create an instance of OvVR.
	OvVR ovvr;

	// Initialize the OvVR instance.
	if (ovvr.init() == false)
	{
		std::cerr << "[ERROR] Failed to initialize OvVR!" << std::endl;
		return 1;
	}

	// Create one FBO for each eye.
	const unsigned int size_x = ovvr.getHmdIdealHorizRes();
	const unsigned int size_y = ovvr.getHmdIdealVertRes();
	std::vector<std::shared_ptr<Fbo>> eyes_fbo;
	for (unsigned int i = 0; i < 2; ++i)
	{
		GLuint texture_id = 0;
		glGenTextures(1, &texture_id);
		glBindTexture(GL_TEXTURE_2D, texture_id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, size_x, size_y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		std::shared_ptr<Fbo> fbo = std::make_shared<Fbo>();
		fbo->bindTexture(0, Fbo::BIND_COLORTEXTURE, texture_id);
		fbo->bindRenderBuffer(1, Fbo::BIND_DEPTHBUFFER, size_x, size_y);

		if (fbo->isOk() == false)
		{
			std::cerr << "[ERROR] Failed to create FBO!" << std::endl;
			return 1;
		}

		Fbo::disable();

		eyes_fbo.push_back(fbo);
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

		// Update OvVR. This should be called once per frame.
		if (ovvr.update() == false)
		{
			std::cerr << "[ERROR] Failed to update OvVR!" << std::endl;
			return 1;
		}

		// Render the scene.
		const glm::mat4 view_matrix = ovvr.getModelviewMatrix();
		for (unsigned int i = 0; i < 2; ++i)
		{
			const OvVR::OvEye eye = i == 0 ? OvVR::OvEye::EYE_LEFT : OvVR::OvEye::EYE_RIGHT;
			const glm::mat4 projection_matrix = ovvr.getProjMatrix(eye, 0.1f, 65'536.0f);
			const glm::mat4 eye_to_head_matrix = ovvr.getEye2HeadMatrix(eye);
			const std::shared_ptr<Fbo> fbo = eyes_fbo.at(i);

			// Bind the FBO.
			fbo->render();

			// Clear the FBO.
			glClearColor(0.22f, 0.36f, 0.42f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			cube.render(
				projection_matrix *
				glm::inverse(eye_to_head_matrix) *
				glm::inverse(view_matrix) *
				glm::inverse(camera_matrix) *
				glm::scale(glm::mat4(1.0f), glm::vec3(0.1f)));

			ovvr.pass(eye, fbo->getTexture(0));

			if (eye == OvVR::OvEye::EYE_LEFT)
			{
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				glBlitFramebuffer(0, 0, size_x, size_y, 0, 0, 800, 600, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			}
		}

		ovvr.render();

		// Swap the buffers.
		glutSwapBuffers();
	}

	// Free up resources and destroy the OpenVR instance.
	// This must be called BEFORE destroying the OpenGL context!
	ovvr.free();

	// Free up resources and destroy the OpenGL context.
	glutDestroyWindow(window);

	return 0;
}
