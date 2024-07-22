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
	/**
	 * This is an internal struct and **must not** be used!
	 *
	 * This struct ties together a swapchain with its FBOs and view sizes for easy access.
	 */
	struct Swapchain
	{
		/**
		 * The OpenXR swapchain.
		 */
		XrSwapchain swapchain;

		/**
		 * All of the FBOs assigned to this swapchain.
		 *
		 * In the case of double-buffering, the framebuffers will be 2. In case of
		 * triple-buffering, the framebuffers will be 3.
		 */
		std::vector<std::shared_ptr<Fbo>> framebuffers;

		/**
		 * The width in pixels of the view.
		 */
		uint32_t width;

		/**
		 * The height in pixels of the view.
		 */
		uint32_t height;
	};

	/**
	 * An enum representing an eye.
	 */
	enum class Eye { LEFT, RIGHT };

	// NOTE: Change here the signature of the user-provided render function.
	typedef std::function<void(const Eye eye, const std::shared_ptr<Fbo> fbo, const glm::mat4 projection_matrix, const glm::mat4 view_matrix)> render_function_t;

	/**
	 * A simple OpenXR wrapper to easily develop VR applications.
	 */
	class XrBridge
	{
	public:
		/**
		 * Default constructor.
		 *
		 * Simply initializes some internal values. Does **not** initialize an OpenXR
		 * session. For that, use the `init()` method.
		 */
		XrBridge();

		// Since we are managing resources, we should prevent the user from creating copies of this object!
		XrBridge(const XrBridge&) = delete;
		XrBridge& operator=(const XrBridge&) = delete;

		/**
		 * Initialize the OpenXR session.
		 *
		 * This method **must** be called before calling any other method of this object.
		 *
		 * @param application_name The title of the application. This is simply the name
		 * that the runtime will display. It's not that important.
		 * @return `true` if the session has been initialized correctly, `false`
		 * otherwise.
		 */
		bool init(const std::string& application_name);

		/**
		 * Terminate the OpenXR session.
		 *
		 * You **must** call this method when you wish to terminate the OpenXR session.
		 * You **must not** call any other method of this object after.
		 */
		void free(void);

		/**
		 * Handle OpenXR events.
		 *
		 * This method **must** be called immediately before each call to the `render()`
		 * method.
		 *
		 * @return `true` if no error occurred, `false` otherwise.
		 */
		bool update(void);

		/**
		 * Render the scene to the VR headset.
		 *
		 * If the `render_function` function is too slow to execute, there could be
		 * visual glitches in the VR headset.
		 *
		 * You **must not** throw exceptions inside the `render_function` function.
		 * Doing so will leave this object in an invalid state.
		 *
		 * @param render_function A user-provided render function. This function will be
		 * called automatically for each view and will receive the correct parameters
		 * to render the scene. Refer to the example project for the correct usage.
		 * `render_function` **must** have the following parameters
		 * in the specified order and return `void`:
		 * 1. `const XrBridge::Eye eye`: The eye that is currently being rendered.
		 * 2. `std::shared_ptr<Fbo> fbo`: A shared pointer to the current OpenGL FBO.
		 * You **must not** store this pointer outside of the `render_function`!
		 * 3. `const glm::mat4 projection_matrix`: The projection matrix to be used for rendering.
		 * 4. `const glm::mat4 view_matrix`: The view matrix to be used for rendering.
		 *
		 * Example using a lambda:
		 * ```CPP
		 * xrbridge.render([&] (const XrBridge::Eye eye, std::shared_ptr<Fbo> fbo, const glm::mat4 projection_matrix, const glm::mat4 view_matrix) {
		 *         // Render stuff here.
		 * }
		 * ```
		 */
		void render(const render_function_t render_function);
	private:
		void begin_session(void);
		void end_session(void);

		std::shared_ptr<Fbo> create_fbo(const GLuint color, const GLsizei width, const GLsizei height) const;

		// This prevents the user from calling other methods on this object inside
		// the user-provide render function (`render_function` parameter of the `render`)
		// method.
		bool is_currently_rendering_flag;

		// This prevents the user from initializing this object multiple times or from calling
		// other methods without first calling init().
		bool is_already_initialized_flag;

		// This prevents the user from re-using this object after free() was called.
		bool is_already_deinitialized_flag;

		XrInstance instance;
		XrSystemId system_id;
		XrSession session;
		XrSessionState session_state;
		std::vector<Swapchain> swapchains;
		XrSpace space;
	};
}
