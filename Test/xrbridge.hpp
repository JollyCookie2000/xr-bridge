#pragma once

// TODO: Allow using macros to choose the error handling method (exceptions vs is_ready()).
// If a constructor fails, the destructor is not called! Destroy the instance and session in the constructor tehn!

/*
 * For the OpenXR API documentation:
 * 	https://registry.khronos.org/OpenXR/specs/1.1/man/html/FUNCTION_OR_STRUCT.html
 */

#include <functional>
#include <string>
#include <vector>

#include <Windows.h>

#include <GL/glew.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr_platform.h>

namespace XrBridge
{
	// TODO: This should not be accessible from outside.
	// This is used to easily tie together swapchains with their framebuffer IDs and sizes.
	struct Swapchain
	{
		XrSwapchain swapchain;
		std::vector<GLuint> framebuffers;
		uint32_t width;
		uint32_t height;
	};

	enum Eye { LEFT, RIGHT };

	typedef std::function<void(const Eye eye, const glm::mat4 projection_matrix, const glm::mat4 view_matrix)> render_function_t;

	class XrBridge
	{
	public:
		XrBridge(const std::string& appication_name, const std::vector<std::string>& requested_api_layers, const std::vector<std::string>& requested_extensions, const HDC hdc, const HGLRC hglrc);
		~XrBridge(void);

		bool is_ready(void) const;

		void update(void);
		void render(const render_function_t render_function);
	private:
		void begin_session(void);
		void end_session(void);
		GLuint create_fbo(const GLuint color, const GLsizei width, const GLsizei height) const;
		void destroy_fbo(const GLuint id) const;

		bool is_ready_flag;
		bool is_currently_rendering_flag;

		XrInstance instance;
		XrSystemId system_id;
		XrSession session;
		XrSessionState session_state;
		std::vector<Swapchain> swapchains;
		XrSpace space;
	};
}
