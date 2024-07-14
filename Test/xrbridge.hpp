#pragma once

/*
 * For the OpenXR API documentation:
 * 	https://registry.khronos.org/OpenXR/specs/1.1/man/html/FUNCTION_OR_STRUCT.html
 */

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <GL/glew.h>

#include <glm/glm.hpp>

#include <openxr/openxr.h>

#include "fbo.h"

namespace XrBridge
{
	// TODO: This should not be accessible from outside.
	// This is used to easily tie together swapchains with their framebuffer IDs and sizes.
	struct Swapchain
	{
		XrSwapchain swapchain;
		std::vector<std::shared_ptr<Fbo>> framebuffers;
		uint32_t width;
		uint32_t height;
	};

	enum class Eye { LEFT, RIGHT };

	// NOTE: Change here the signature of the user-provided render function.
	typedef std::function<void(const Eye eye, const std::shared_ptr<Fbo> fbo, const glm::mat4 projection_matrix, const glm::mat4 view_matrix)> render_function_t;

	class XrBridge
	{
	public:
		XrBridge(const std::string& appication_name);
		~XrBridge(void);

		bool is_ready(void) const;

		void update(void);
		void render(const render_function_t render_function);
	private:
		void begin_session(void);
		void end_session(void);

		std::shared_ptr<Fbo> create_fbo(const GLuint color, const GLsizei width, const GLsizei height) const;

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
