# XrBridge

A simple OpenXR wrapper for virtual reality applications using OpenGL.

XrBridge is a C++ library that wraps OpenXR in an easy-to-use API with the
purpose of simplifying the interfacing between VR headsets and OpenGL
applications.

The purpose of this library is to replace an existing implementation (OvVR)
that utilizes OpenVR instead of OpenXR, which has been used in the virtual
reality course at SUPSI for some time and was developed by a professor.

For simplicty, the library is provided as two HPP and two CPP files that you
can simply copy-paste into any project.

> XrBridge only supports head mounted displays (HMD / 2 eyes).

> The main documentation is located in `XrBridge.pdf`.

## Repository structure

* `/Test OvVR/`: A demo application with a simple cube that uses OvVR (OpenVR).
* `/Test/`: A demo application with a simple cube that uses XrBridge (OpenXR).
* `/blog/`: A series of blogs that contain random thoughts I had during the
  development of this project.
* `/deps/`: All the Windows dependencies required to compile the demo
  application plus a patched version of FreeGLUT necessary for compiling on
  Linux.
* `/html/`: The Doxygen documentation for the XrBridge API.
* `/rapporto/`: The LaTeX source for the documentation of the whole project.
* `Doxyfile`: The Doxygen configuration file.
* `The OpenXR 1.1.37 Specification.pdf`: The OpenXR specification used as
  reference during the development of this project.
* `XrBridge.pdf`: The documentation for the whole project.
* `XrBridge Description.pdf`: The document describing the purpose of the
  project.
* `demo.webm`: A short video comparing OvVR (left) and XrBridge (right).
* `/frameBufferObjectAndXrBridge.zip`: The professor's example application
  rewritten to use XrBridge.

## Dependencies

XrBridge has the following dependencies:

* FreeGLUT (3.6.0)
* GLEW (2.1.0)
* GLM (1.0.1)
* OpenXR Loader (1.1.37)

> There are no pre-compiled binaries of the OpenXR Loader for Windows. You will
> have to compile them from source yourself: [github.com/KhronosGroup/OpenXR-SDK/releases/tag/release-1.1.37](https://github.com/KhronosGroup/OpenXR-SDK/releases/tag/release-1.1.37)

> **IMPORTANT**: XrBridge on Linux requires a modified FreeGLUT (provided in
> the `/deps/freeglut-patched/` directory) that exposes some internal
> parameters that are not usually accessible.

You will also need an OpenXR runtime and a VR headset. This project was
designed with Valve's SteamVR in mind. It can be downloaded from
[store.steampowered.com/app/250820/SteamVR/](https://store.steampowered.com/app/250820/SteamVR/).
For the VR headset, you can use [ALVR](https://github.com/alvr-org/ALVR) and
[PhoneVR](https://github.com/PhoneVR-Developers/PhoneVR). They allow you to use
and Android device as an headset.

The `/deps/` directory contains the required dependencies for Windows, in
addition to the modified FreeGLUT (called `freeglut-patched`) that is required
on Linux.

## Getting started

The easiest way of getting getting started with XrBridge is by modifying the
demo project inside the `/Test/` directory. There's a Visual Studio solution
and a Code::Blocks workspace pre-configured and ready to be used. You can just
modify the source code of the demo.

Alternatively, you can create a new project:

1. Create the project with any IDE or build system you want.
2. Copy the following files to the new project: `xrbridge.cpp`, `xrbridge.hpp`,
   `fbo.h` and `fbo.cpp` located in the `/Test/` directory.
3. Install and configure the dependencies.
4. Define a project-level macro depending on the platform:
   `XRBRIDGE_PLATFORM_WINDOWS` when compiling on Windows or
   `XRBRIDGE_PLATFORM_X11` when compiling for Linux and X11 (should also work
   on Wayland through XWayland).

## Example

```C++
#include <memory>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// You MUST define the platform you are using in your PROJECT settings.
// Define ONE of the following to choose the platform:
// * XRBRIDGE_PLATFORM_WINDOWS for Windows
// * XRBRIDGE_PLATFORM_X11 for Linux (should work through XWayland)
#include "xrbridge.hpp"

static bool g_running = true;

int main(int argc, char** argv) {
	// Initialize FreeGLUT.
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitContextVersion(4, 4);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInit(&argc, argv);

	// Create the FreeGLUT window.
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutInitWindowSize(800, 600);
	const int window = glutCreateWindow("XrBridge Demo");

	// This is the callback that gets called when the window needs to be re-rendered.
	// For this example, we don't need it.
	glutDisplayFunc([] () {});

	// This is the callback that will be called when we close the window.
	glutCloseFunc([] () {
		g_running = false;
	});

	// Initialize GLEW.
	if (glewInit() != GLEW_OK) {
		std::cerr << "[ERROR] Failed to initialize GLEW." << std::endl;
		return 1;
	}

	// Enable depth for OpenGL.
	glEnable(GL_DEPTH_TEST);

	// Create an instance of XrBridge.
	XrBridge xrbridge;

	// Initialize the XrBridge instance.
	if (xrbridge.init("XrBridge Demo") == false) {
		std::cerr << "[ERROR] Failed to initialize XrBridge." << std::endl;
		return 1;
	}

	xrbridge.set_clipping_planes(0.1f, 65'536.0f);

	// We hard-code a camera at position [0.0, 0.5, 0.5].
	// NOTE: 1 unit = 1 meter
	const glm::mat4 camera_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.5f));

	while (g_running) {
		// Process FreeGLUT events.
		glutMainLoopEvent();

		// Update XrBridge. This should be called once per frame.
		if (xrbridge.update() == false) {
			std::cerr << "[ERROR] Failed to update XrBridge." << std::endl;
			return 1;
		}

		// Render the scene.
		// The render function accepts a user-defined function (in this case a lambda).
		// This user-defined function will be called as many times as necessary
		//  (probably twice, once for each eye; it could also not be called at all)
		//  to render each view.
		const bool did_render = xrbridge.render([&] (
				const XrBridge::Eye eye,
				std::shared_ptr<Fbo> fbo,
				const glm::mat4 projection_matrix,
				const glm::mat4 view_matrix,
				const uint32_t width,
				const uint32_t height) {
			// Bind the FBO and set the viewport. This is not done automatically
			//  by XrBridge, so we must do it ourselves!
			fbo->render();

			// Clear the FBO.
			glClearColor(0.22f, 0.36f, 0.42f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Render the scene ...

			if (eye == XrBridge::Eye::LEFT) {
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				glBlitFramebuffer(0, 0, width, height, 0, 0, 800, 600, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			}
		});

		if (did_render == false) {
			std::cerr << "[ERROR] Failed to render." << std::endl;
			return 1;
		}

		// Swap the buffers.
		glutSwapBuffers();
	}

	// Free up resources and destroy the OpenXR instance.
	// This must be called BEFORE destroying the OpenGL context!
	if (xrbridge.free() == false) {
		std::cerr << "[ERROR] Failed to free XrBridge resources." << std::endl;
		return 1;
	}

	// Free up resources and destroy the OpenGL context.
	glutDestroyWindow(window);

	return 0;
}
```
