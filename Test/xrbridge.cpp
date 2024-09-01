/* ========== CONFIGURATION ========== */

// The format to be used to generate the FBOs.
// NOTE: SteamVR supports different formats on Windows and Linux.
#define XRBRIDGE_CONFIG_SWAPCHAIN_FORMAT_WINDOWS GL_SRGB8_ALPHA8
#define XRBRIDGE_CONFIG_SWAPCHAIN_FORMAT_LINUX   GL_SRGB8_ALPHA8

// The type of reference space.
// https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrReferenceSpaceType.html
#define XRBRIDGE_CONFIG_SPACE XrReferenceSpaceType::XR_REFERENCE_SPACE_TYPE_STAGE

// The OpenXR version to use.
#define XRBRIDGE_CONFIG_OPENXR_VERSION XR_MAKE_VERSION(1, 0, 0);

/* ========== CONFIGURATION ========== */

#include "xrbridge.hpp"

#include <iostream>

#ifdef _WIN32
	#define XRBRIDGE_PLATFORM_WINDOWS
#else
	#define XRBRIDGE_PLATFORM_X11
#endif

// Platform-specific includes
#ifdef XRBRIDGE_PLATFORM_WINDOWS
	#include <Windows.h> // This MUST be included BEFORE FreeGLUT or the gods will not be happy.
#endif
#ifdef XRBRIDGE_PLATFORM_X11
	#include <GL/freeglut_globals.h>
	#include <GL/glx.h>
	#include <X11/Xlib.h>
#endif

#include <GL/freeglut.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Choose the OpenXR platform.
#ifdef XRBRIDGE_PLATFORM_WINDOWS
	#define XR_USE_PLATFORM_WIN32
#endif
#ifdef XRBRIDGE_PLATFORM_X11
	#define XR_USE_PLATFORM_XLIB
#endif

#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr_platform.h>

// Macro to easily manage errors returned from OpenXR functions.
#define RETURN_FALSE_ON_OXR_ERROR( function, message ) \
	if (check_openxr_result( this->instance, function ) == false) \
	{ \
		XRBRIDGE_ERROR_OUT(message); \
		return false; \
	}

#define XRBRIDGE_CHECK_RENDERING( value ) \
	if (this->is_currently_rendering_flag == value) \
	{ \
		XRBRIDGE_ERROR_OUT("You cannot call this function inside the render function!"); \
		return false; \
	}

#define XRBRIDGE_CHECK_INITIALIZED(value) \
	if (this->is_already_initialized_flag == value) \
	{ \
		XRBRIDGE_ERROR_OUT("This object has not been initialized yet!"); \
		return false; \
	}

#define XRBRIDGE_CHECK_DEINITIALIZED(value) \
	if (this->is_already_deinitialized_flag == value) \
	{ \
		XRBRIDGE_ERROR_OUT("This object has already been de-initialized!"); \
		return false; \
	}

// Convert an OpenXR vector to a GLM vector.
#define XRV_TO_GV( xrv ) glm::vec3(xrv.x, xrv.y, xrv.z)

#ifdef XRBRIDGE_DEBUG
	// Print a debug message to stdout.
	#define XRBRIDGE_DEBUG_OUT( message ) { std::cout << "[XrBridge][DEBUG] " << message << std::endl; }
#else
	#define XRBRIDGE_DEBUG_OUT( message ) ;
#endif

// Print an error message to stderr.
#define XRBRIDGE_ERROR_OUT( message ) { std::cerr << "[XrBridge][ERROR] " << message << std::endl; }

// Print a warning message to the stdout.
#define XRBRIDGE_WARNING_OUT( message ) { std::cout << "[XrBridge][WARNING] " << message << std::endl; }

// This is just to disable some annoying warning about NULL.
#define NULL_FLAG 0

// Choose which swapchain format to use. This is done because, at the moment of writing this,
// SteamVR on Linux supports different formats compared to Windows.
#ifdef XRBRIDGE_PLATFORM_WINDOWS
	#define XRBRIDGE_SWAPCHAIN_FORMAT XRBRIDGE_CONFIG_SWAPCHAIN_FORMAT_WINDOWS
#endif
#ifdef XRBRIDGE_PLATFORM_X11
	#define XRBRIDGE_SWAPCHAIN_FORMAT XRBRIDGE_CONFIG_SWAPCHAIN_FORMAT_LINUX
#endif

// Source: https://openxr-tutorial.com/linux/opengl/_downloads/f4aef9ec726fccc71e105bc0830d4ff3/xr_linear_algebra.h
// XrMatrix4x4f_CreateProjectionFov
// XrMatrix4x4f_CreateProjection
// We have to use a custom projection function because glm::projection does not handle
// fov the way we need it to.
static glm::mat4 create_projection_matrix(const XrFovf fov, const float near_clipping_plane, const float far_clipping_plane)
{
	const float tan_left = std::tanf(fov.angleLeft);
	const float tan_right = std::tanf(fov.angleRight);
	const float tan_down = std::tanf(fov.angleDown);
	const float tan_up = std::tanf(fov.angleUp);
	const float tan_width = tan_right - tan_left;
	const float tan_height = tan_up - tan_down;

	glm::mat4 result = glm::mat4(
		2.0f / tan_width,
		0.0f,
		0.0f,
		0.0f,

		0.0f,
		2.0f / tan_height,
		0.0f,
		0.0f,

		(tan_right + tan_left) / tan_width,
		(tan_up + tan_down) / tan_height,
		-(near_clipping_plane + far_clipping_plane) / (far_clipping_plane - near_clipping_plane),
		-1.0f,

		0.0f,
		0.0f,
		-(2.0f * near_clipping_plane * far_clipping_plane) / (far_clipping_plane - near_clipping_plane),
		0.0f
	);

	return result;
}

static bool check_openxr_result(const XrInstance instance, const XrResult result)
{
	if (result != XrResult::XR_SUCCESS)
	{
		char message[XR_MAX_RESULT_STRING_SIZE] = {};
		xrResultToString(instance, result, message);
		XRBRIDGE_ERROR_OUT("OpenXR error: [" << result << "] " << message);

		return false;
	}

	return true;
}

static std::vector<XrApiLayerProperties> get_available_api_layers()
{
	uint32_t available_api_layers_count = 0;
	if (xrEnumerateApiLayerProperties(0, &available_api_layers_count, nullptr) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_ERROR_OUT("Failed to enumerate OpenXR API layers.");
		return {};
	}

	std::vector<XrApiLayerProperties> available_api_layers(available_api_layers_count, { XR_TYPE_API_LAYER_PROPERTIES });
	if (xrEnumerateApiLayerProperties(available_api_layers_count, &available_api_layers_count, available_api_layers.data()) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_ERROR_OUT("Failed to enumerate OpenXR API layers.");
		return {};
	}

	return available_api_layers;
}

static bool is_api_layer_supported(const std::string& api_layer_name)
{
	for (const XrApiLayerProperties& api_layer : get_available_api_layers())
		if (api_layer_name == api_layer.layerName)
			return true;

	return false;
}

static std::vector<XrExtensionProperties> get_available_extensions()
{
	uint32_t available_extensions_count = 0;
	if (xrEnumerateInstanceExtensionProperties(nullptr, 0, &available_extensions_count, nullptr) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_ERROR_OUT("Failed to enumerate OpenXR extensions.");
		return {};
	}

	std::vector<XrExtensionProperties> available_extensions(available_extensions_count, { XR_TYPE_EXTENSION_PROPERTIES });
	if (xrEnumerateInstanceExtensionProperties(nullptr, available_extensions_count, &available_extensions_count, available_extensions.data()) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_ERROR_OUT("Failed to enumerate OpenXR extensions.");
		return {};
	}

	return available_extensions;
}

static bool is_extension_supported(const std::string& extension_name)
{
	for (const XrExtensionProperties& extensions : get_available_extensions())
	{
		if (extension_name == extensions.extensionName)
			return true;
	}

	return false;
}

XrBridge::XrBridge() :
	is_currently_rendering_flag{ false },
	is_already_initialized_flag{ false },
	is_already_deinitialized_flag{ false },
	near_clipping_plane{ 0.1f },
	far_clipping_plane{ 65'536.0f },
	instance{ XR_NULL_HANDLE },
	system_id{ XR_NULL_SYSTEM_ID },
	session{ XR_NULL_HANDLE },
	session_state{ XrSessionState::XR_SESSION_STATE_UNKNOWN },
	swapchains{ },
	space{ XR_NULL_HANDLE }
{
}

bool XrBridge::init(const std::string& application_name)
{
	XRBRIDGE_CHECK_RENDERING(true);

	XRBRIDGE_CHECK_DEINITIALIZED(true);

	if (this->is_already_initialized_flag)
	{
		XRBRIDGE_ERROR_OUT("This object has already been initialized!");
		return false;
	}

	XRBRIDGE_DEBUG_OUT("OpenXR version: " << XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_MINOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_PATCH(XR_CURRENT_API_VERSION));

	XrApplicationInfo application_info = {};
	std::strncpy(application_info.applicationName, application_name.c_str(), XR_MAX_APPLICATION_NAME_SIZE);
	application_info.applicationVersion = 1;
	std::strncpy(application_info.engineName, "", XR_MAX_ENGINE_NAME_SIZE);
	application_info.engineVersion = 0;
	application_info.apiVersion = XRBRIDGE_CONFIG_OPENXR_VERSION;


	// Load API layers
	// NOTE: Specify here the desired API layers.
	const std::vector<std::string> requested_api_layers = {};
	std::vector<const char*> active_api_layers;
	for (const std::string& requested_api_layer : requested_api_layers)
	{
		// Is the layer available?
		if (is_api_layer_supported(requested_api_layer))
		{
			// Load the layer.
			active_api_layers.push_back(requested_api_layer.c_str());
		}
		else
		{
			XRBRIDGE_ERROR_OUT("API layer \"" << requested_api_layer << "\" is not available");;
			return false;
		}
	}

	// Load extensions
	// NOTE: Specify here the desired OpenXR extensions.
	const std::vector<std::string>& requested_extensions = { "XR_KHR_opengl_enable" };
	std::vector<const char*> active_extensions;
	for (const std::string& requested_extension : requested_extensions)
	{
		// Is the extensions available?
		if (is_extension_supported(requested_extension))
		{
			// Load the extension.
			active_extensions.push_back(requested_extension.c_str());
		}
		else
		{
			XRBRIDGE_ERROR_OUT("Extension \"" << requested_extension << "\" is not available.");
			return false;
		}
	}


	// Create the OpenXR instance.
	XrInstanceCreateInfo instance_create_info = {};
	instance_create_info.type = XrStructureType::XR_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.createFlags = NULL_FLAG;
	instance_create_info.enabledApiLayerCount = static_cast<uint32_t>(active_api_layers.size());
	instance_create_info.enabledApiLayerNames = active_api_layers.data();
	instance_create_info.enabledExtensionCount = static_cast<uint32_t>(active_extensions.size());
	instance_create_info.enabledExtensionNames = active_extensions.data();
	instance_create_info.applicationInfo = application_info;
	RETURN_FALSE_ON_OXR_ERROR(xrCreateInstance(&instance_create_info, &this->instance), "Failed to create OpenXR instance.");


	// Load extension functions. These need to be loaded manually.
	PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
	RETURN_FALSE_ON_OXR_ERROR(xrGetInstanceProcAddr(this->instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetOpenGLGraphicsRequirementsKHR), "Failed to get function pointer.");


	// Print some information about the OpenXR instance.
	XrInstanceProperties instance_properties = {};
	instance_properties.type = XrStructureType::XR_TYPE_INSTANCE_PROPERTIES;
	RETURN_FALSE_ON_OXR_ERROR(xrGetInstanceProperties(this->instance, &instance_properties), "Failed to get instance properties.");
	XRBRIDGE_DEBUG_OUT("OpenXR runtime: " << instance_properties.runtimeName << "(" << XR_VERSION_MAJOR(instance_properties.runtimeVersion) << "." << XR_VERSION_MINOR(instance_properties.runtimeVersion) << "." << XR_VERSION_PATCH(instance_properties.runtimeVersion) << ")");


	// Get the ID of the system.
	// The system is the VR headset / AR device / Cave system.
	XrSystemGetInfo system_info = {};
	system_info.type = XrStructureType::XR_TYPE_SYSTEM_GET_INFO;
	// NOTE: This is the form factor that we desire. This use case only requires a HMD.
	system_info.formFactor = XrFormFactor::XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	RETURN_FALSE_ON_OXR_ERROR(xrGetSystem(this->instance, &system_info, &this->system_id), "Failed to get VR system.");


	// Print the name of the system.
	XrSystemProperties system_properties = {};
	system_properties.type = XrStructureType::XR_TYPE_SYSTEM_PROPERTIES;
	RETURN_FALSE_ON_OXR_ERROR(xrGetSystemProperties(this->instance, this->system_id, &system_properties), "Failed to get VR system properties.");
	XRBRIDGE_DEBUG_OUT("System name: " << system_properties.systemName);

	// Platform-specific code.
	#ifdef XRBRIDGE_PLATFORM_WINDOWS
		XRBRIDGE_DEBUG_OUT("Using platform: Windows (Win32)");

		const HDC hdc = wglGetCurrentDC();
		const HGLRC hglrc = wglGetCurrentContext();
		if (hdc == NULL || hglrc == NULL)
		{
			XRBRIDGE_ERROR_OUT("Failed to get native OpenGL context.");
			return false;
		}

		// Create the OpenGL binding.
		XrGraphicsBindingOpenGLWin32KHR graphics_binding = {};
		graphics_binding.type = XrStructureType::XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
		graphics_binding.hDC = hdc;
		graphics_binding.hGLRC = hglrc;
	#endif
	#ifdef XRBRIDGE_PLATFORM_X11
		XRBRIDGE_DEBUG_OUT("Using platform: X11 (XLIB)");

		int number_of_configs = 0;
		GLXFBConfig* fbconfigs = glXChooseFBConfig(
			glXGetCurrentDisplay(),
			0, // Screen
			freeglut_attributes,
			&number_of_configs
		);

		if (number_of_configs < 1)
		{
			XRBRIDGE_ERROR_OUT("Failed to get FBConfigs.");
			return false;
		}

		GLXFBConfig fbconfig = fbconfigs[0];

		// Create the OpenGL binding.
		XrGraphicsBindingOpenGLXlibKHR graphics_binding = {};
		graphics_binding.type = XrStructureType::XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
		graphics_binding.xDisplay = glXGetCurrentDisplay();
		graphics_binding.visualid = freeglut_visualid;
		graphics_binding.glxFBConfig = fbconfig;
		graphics_binding.glxDrawable = glXGetCurrentDrawable();
		graphics_binding.glxContext = glXGetCurrentContext();
	#endif


	// TODO: Verify OpenGL version requirements.
	XrGraphicsRequirementsOpenGLKHR graphics_requirements = {};
	graphics_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
	xrGetOpenGLGraphicsRequirementsKHR(this->instance, this->system_id, &graphics_requirements);


	// Create an OpenXR session.
	XrSessionCreateInfo session_create_info = {};
	session_create_info.type = XrStructureType::XR_TYPE_SESSION_CREATE_INFO;
	session_create_info.next = &graphics_binding;
	session_create_info.createFlags = NULL_FLAG;
	session_create_info.systemId = this->system_id;
	RETURN_FALSE_ON_OXR_ERROR(xrCreateSession(this->instance, &session_create_info, &this->session), "Failed to create OpenXR session.");

	this->is_already_initialized_flag = true;

	return true;
}

bool XrBridge::free()
{
	XRBRIDGE_CHECK_INITIALIZED(false);

	XRBRIDGE_CHECK_RENDERING(true);

	XRBRIDGE_CHECK_DEINITIALIZED(true);

	this->is_already_deinitialized_flag = true;

	if (this->end_session() == false)
	{
		return false;
	}

	if (xrDestroySession(this->session) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_DEBUG_OUT("Failed to destroy session.");
		return false;
	}

	if (xrDestroyInstance(this->instance) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_DEBUG_OUT("Failed to destroy instance.");
		return false;
	}

	return true;
}

bool XrBridge::update()
{
	XRBRIDGE_CHECK_RENDERING(true);

	XRBRIDGE_CHECK_INITIALIZED(false);

	XRBRIDGE_CHECK_DEINITIALIZED(true);

	while (true)
	{
		XrEventDataBuffer event_buffer = {};
		event_buffer.type = XrStructureType::XR_TYPE_EVENT_DATA_BUFFER;
		const XrResult poll_event_result = xrPollEvent(this->instance, &event_buffer);

		if (poll_event_result == XrResult::XR_SUCCESS)
		{
			// More events are available, continue.
		}
		else if (poll_event_result == XrResult::XR_EVENT_UNAVAILABLE)
		{
			// There are no more events to process, return.
			break;
		}
		else
		{
			XRBRIDGE_ERROR_OUT("There was an error while polling for events.");
			return false;
		}

		switch (event_buffer.type)
		{
			// The event queue has overflown and some events were lost.
		case XrStructureType::XR_TYPE_EVENT_DATA_EVENTS_LOST:
			XRBRIDGE_WARNING_OUT("The event queue has overflown.");
			break;
		case XrStructureType::XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			// https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrEventDataInstanceLossPending.html
			// As per the documentation: "...indicates that the application is
			// about to lose the indicated XrInstance..." and "...typically
			// occurs to make way for a replacement of the underlying runtime...".
			// In this case, we just generate an error; the user should just restart
			// the application.
			XRBRIDGE_ERROR_OUT("The OpenXR instance is about to disconnect.");
			return false;
		case XrStructureType::XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			const XrEventDataSessionStateChanged* session_state_changed = reinterpret_cast<XrEventDataSessionStateChanged*>(&event_buffer);

			if (session_state_changed->session != this->session)
			{
				break;
			}

			this->session_state = session_state_changed->state;

			if (session_state_changed->state == XrSessionState::XR_SESSION_STATE_READY)
			{
				XRBRIDGE_DEBUG_OUT("OpenXR session is beginning.");

				if (this->begin_session() == false)
				{
					XRBRIDGE_ERROR_OUT("Failed to begin OpenXR session.");
					return false;
				}
			}
			else if (session_state_changed->state == XrSessionState::XR_SESSION_STATE_STOPPING)
			{
				XRBRIDGE_DEBUG_OUT("OpenXR session is stopping.");

				if (this->end_session() == false)
				{
					XRBRIDGE_ERROR_OUT("Failed to end OpenXR session.");
					return false;
				}
			}
			else if (session_state_changed->state == XrSessionState::XR_SESSION_STATE_LOSS_PENDING)
			{
				XRBRIDGE_ERROR_OUT("The OpenXR session is about to be lost.");
				return false;
			}
			else if (session_state_changed->state == XrSessionState::XR_SESSION_STATE_EXITING)
			{
				XRBRIDGE_ERROR_OUT("The OpenXR session is about to exit.");
				return false;
			}

			break;
		}
		}
	}

	return true;
}

bool XrBridge::render(const render_function_t render_function)
{
	XRBRIDGE_CHECK_RENDERING(true);

	XRBRIDGE_CHECK_INITIALIZED(false);

	XRBRIDGE_CHECK_DEINITIALIZED(true);

	this->is_currently_rendering_flag = true;

	XrFrameState frame_state = {};
	frame_state.type = XrStructureType::XR_TYPE_FRAME_STATE;
	XrFrameWaitInfo frame_wait_info = {};
	frame_wait_info.type = XrStructureType::XR_TYPE_FRAME_WAIT_INFO;
	// Wait for synchronization with the headset display.
	RETURN_FALSE_ON_OXR_ERROR(xrWaitFrame(this->session, &frame_wait_info, &frame_state), "Faield to wait for frame.");

	XrFrameBeginInfo frame_begin_info = {};
	frame_begin_info.type = XrStructureType::XR_TYPE_FRAME_BEGIN_INFO;
	RETURN_FALSE_ON_OXR_ERROR(xrBeginFrame(this->session, &frame_begin_info), "Failed to begin frame.");

	std::vector<XrCompositionLayerBaseHeader*> layers = {};

	const bool is_session_active =
		this->session_state == XrSessionState::XR_SESSION_STATE_SYNCHRONIZED ||
		this->session_state == XrSessionState::XR_SESSION_STATE_VISIBLE ||
		this->session_state == XrSessionState::XR_SESSION_STATE_FOCUSED;

	bool did_render = false;

	// 3D view
	XrCompositionLayerProjection composition_layer_projection = {};
	composition_layer_projection.type = XrStructureType::XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	composition_layer_projection.layerFlags = NULL_FLAG;
	composition_layer_projection.space = this->space;
	composition_layer_projection.viewCount = 0; // This will be filled up later.
	composition_layer_projection.views = nullptr; // This will be filled up later.

	std::vector<XrCompositionLayerProjectionView> composition_layer_projection_views = {};

	if (is_session_active && frame_state.shouldRender)
	{
		did_render = true;

		// 3D view
		XrViewState view_state = {};
		view_state.type = XrStructureType::XR_TYPE_VIEW_STATE;
		XrViewLocateInfo view_locate_info = {};
		view_locate_info.type = XrStructureType::XR_TYPE_VIEW_LOCATE_INFO;
		view_locate_info.viewConfigurationType = XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		view_locate_info.displayTime = frame_state.predictedDisplayTime;
		view_locate_info.space = this->space;

		uint32_t view_count = 0;
		xrLocateViews(this->session, &view_locate_info, &view_state, 0, &view_count, nullptr);
		std::vector<XrView> views(view_count, { XrStructureType::XR_TYPE_VIEW });
		xrLocateViews(this->session, &view_locate_info, &view_state, view_count, &view_count, views.data());

		// In the case of stereo view, view_index = 0 is the LEFT eye and view_index = 1 is the RIGHT eye.
		for (uint32_t view_index = 0; view_index < views.size(); ++view_index)
		{
			const Swapchain& current_swapchain = this->swapchains.at(view_index);
			const XrView& current_view = views.at(view_index);

			uint32_t image_index = 0;
			XrSwapchainImageAcquireInfo swapchain_image_acquire_info = {};
			swapchain_image_acquire_info.type = XrStructureType::XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
			RETURN_FALSE_ON_OXR_ERROR(xrAcquireSwapchainImage(current_swapchain.swapchain, &swapchain_image_acquire_info, &image_index), "Failed to acquire swapchain image.");

			XrSwapchainImageWaitInfo swapchain_image_wait_info = {};
			swapchain_image_wait_info.type = XrStructureType::XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
			swapchain_image_wait_info.timeout = XR_INFINITE_DURATION;
			RETURN_FALSE_ON_OXR_ERROR(xrWaitSwapchainImage(current_swapchain.swapchain, &swapchain_image_wait_info), "Failed to wait for swapchain image.");

			XrCompositionLayerProjectionView composition_layer_projection_view = {};
			composition_layer_projection_view.type = XrStructureType::XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
			composition_layer_projection_view.pose = current_view.pose;
			composition_layer_projection_view.fov = current_view.fov;
			composition_layer_projection_view.subImage.swapchain = current_swapchain.swapchain;
			composition_layer_projection_view.subImage.imageRect.offset.x = 0;
			composition_layer_projection_view.subImage.imageRect.offset.y = 0;
			composition_layer_projection_view.subImage.imageRect.extent.width = current_swapchain.width;
			composition_layer_projection_view.subImage.imageRect.extent.height = current_swapchain.height;
			composition_layer_projection_view.subImage.imageArrayIndex = 0;
			composition_layer_projection_views.push_back(composition_layer_projection_view);

			// As specified by the OpenXR specification, the left eye has an index of 0 and the right eye an index of 1.
			// https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrViewConfigurationType.html
			const Eye eye = view_index == 0 ? Eye::LEFT : Eye::RIGHT;

			// Create the projection matrix
			const glm::mat4 projection_matrix = create_projection_matrix(
				current_view.fov,
				this->near_clipping_plane,
				this->far_clipping_plane);

			// Create the view matrix
			const glm::quat quaternion = glm::quat(current_view.pose.orientation.w, current_view.pose.orientation.x, current_view.pose.orientation.y, current_view.pose.orientation.z);
			const glm::mat4 rotation_matrix = glm::mat4_cast(quaternion);
			const glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), XRV_TO_GV(current_view.pose.position));
			const glm::mat4 view_matrix = translation_matrix * rotation_matrix;

			// Get the FBO
			const std::shared_ptr<Fbo> fbo = current_swapchain.framebuffers.at(image_index);

			// Call the user-defined render function
			render_function(eye, fbo, projection_matrix, view_matrix, current_swapchain.width, current_swapchain.height);

			XrSwapchainImageReleaseInfo swapchain_image_release_info = {};
			swapchain_image_release_info.type = XrStructureType::XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
			RETURN_FALSE_ON_OXR_ERROR(xrReleaseSwapchainImage(current_swapchain.swapchain, &swapchain_image_release_info), "Failed to release swapchain image.");
		}

		composition_layer_projection.viewCount = static_cast<uint32_t>(composition_layer_projection_views.size());
		composition_layer_projection.views = composition_layer_projection_views.data();
	}

	if (did_render)
	{
		layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&composition_layer_projection));
	}

	XrFrameEndInfo frame_end_info = {};
	frame_end_info.type = XrStructureType::XR_TYPE_FRAME_END_INFO;
	frame_end_info.displayTime = frame_state.predictedDisplayTime;
	// NOTE: This is the blend mode. In the case of AR, choose something like ALPHA or ADDITTIVE to mix the virtual and real world.
	frame_end_info.environmentBlendMode = XrEnvironmentBlendMode::XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	frame_end_info.layerCount = static_cast<uint32_t>(layers.size());
	frame_end_info.layers = layers.data();
	RETURN_FALSE_ON_OXR_ERROR(xrEndFrame(this->session, &frame_end_info), "Failed to end frame.");

	this->is_currently_rendering_flag = false;

	return true;
}

void XrBridge::set_clipping_planes(const float near_clipping_plane, const float far_clipping_plane)
{
	this->near_clipping_plane = near_clipping_plane;
	this->far_clipping_plane = far_clipping_plane;
}

bool XrBridge::begin_session()
{
	XrSessionBeginInfo session_begin_info = {};
	session_begin_info.type = XrStructureType::XR_TYPE_SESSION_BEGIN_INFO;
	// NOTE: This is the view cofiguration type that we desire. This use case only requires stereo (two eyes).
	session_begin_info.primaryViewConfigurationType = XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	RETURN_FALSE_ON_OXR_ERROR(xrBeginSession(this->session, &session_begin_info), "Faield to begin session.");

	// A view more or less equates to a display. In the case of a VR headset, since we have 2 eyes, we will ave 2 views.
	uint32_t view_count = 0;
	RETURN_FALSE_ON_OXR_ERROR(xrEnumerateViewConfigurationViews(this->instance, this->system_id, XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &view_count, nullptr), "Faield to enumerate view configuration views.");
	std::vector<XrViewConfigurationView> view_configuration_views(view_count, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	RETURN_FALSE_ON_OXR_ERROR(xrEnumerateViewConfigurationViews(this->instance, this->system_id, XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, view_count, &view_count, view_configuration_views.data()), "Faield to enumerate view configuration views.");

	if (view_count != 2)
	{
		XRBRIDGE_ERROR_OUT("OpenXR is reporting " << view_count << " views, but only 2 are supported.");
		return false;
	}

	for (const auto& view_configuration_view : view_configuration_views)
	{
		Swapchain swapchain = {};
		swapchain.width = view_configuration_view.recommendedImageRectWidth;
		swapchain.height = view_configuration_view.recommendedImageRectHeight;

		XrSwapchainCreateInfo swapchain_create_info = {};
		swapchain_create_info.type = XrStructureType::XR_TYPE_SWAPCHAIN_CREATE_INFO;
		swapchain_create_info.createFlags = NULL_FLAG;
		// OpenGL does not support usage flags.
		swapchain_create_info.usageFlags = NULL_FLAG;
		swapchain_create_info.format = XRBRIDGE_SWAPCHAIN_FORMAT;
		swapchain_create_info.width = view_configuration_view.recommendedImageRectWidth;
		swapchain_create_info.height = view_configuration_view.recommendedImageRectHeight;
		swapchain_create_info.sampleCount = view_configuration_view.recommendedSwapchainSampleCount;
		swapchain_create_info.faceCount = 1;
		swapchain_create_info.arraySize = 1;
		swapchain_create_info.mipCount = 1;
		RETURN_FALSE_ON_OXR_ERROR(xrCreateSwapchain(this->session, &swapchain_create_info, &swapchain.swapchain), "Failed to create swapchain.");

		// Create the single images inside the swapchain.
		// If the runtime is using double-buffering, there will be 2 images per swapchain. For triple buffering,
		//  the images will be 3.
		uint32_t swapchain_image_count = 0;
		RETURN_FALSE_ON_OXR_ERROR(xrEnumerateSwapchainImages(swapchain.swapchain, 0, &swapchain_image_count, nullptr), "Failed to enumerate swapchain images.");
		std::vector<XrSwapchainImageOpenGLKHR> swapchain_images(swapchain_image_count, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR }); // NOTE: Change this to use another graphics API.
		RETURN_FALSE_ON_OXR_ERROR(xrEnumerateSwapchainImages(swapchain.swapchain, swapchain_image_count, &swapchain_image_count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchain_images.data())), "Failed to enumerate swapchain images.");

		for (const auto& swapchain_image : swapchain_images)
		{
			const std::shared_ptr<Fbo> fbo = this->create_fbo(swapchain_image.image, swapchain.width, swapchain.height);

			if (fbo == nullptr)
			{
				XRBRIDGE_ERROR_OUT("Failed to create FBO.");
				return false;
			}

			swapchain.framebuffers.push_back(fbo);
		}

		this->swapchains.push_back(swapchain);
	}

	// Create the reference space.
	XrReferenceSpaceCreateInfo reference_space_info = {};
	reference_space_info.type = XrStructureType::XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	// https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrReferenceSpaceType.html
	reference_space_info.referenceSpaceType = XRBRIDGE_CONFIG_SPACE;
	// Here we set the offset to the origin of the tracking space. In this case, we keep the origin were it is.
	reference_space_info.poseInReferenceSpace = {
		{ 0.0f, 0.0f, 0.0f, 1.0f, }, // Orientation
		{ 0.0f, 0.0f, 0.0f }, // Position
	};

	RETURN_FALSE_ON_OXR_ERROR(xrCreateReferenceSpace(this->session, &reference_space_info, &this->space), "Failed to create reference space.");

	return true;
}

bool XrBridge::end_session()
{
	// Destroy swapchains.
	for (const auto& swapchain : this->swapchains)
	{
		RETURN_FALSE_ON_OXR_ERROR(xrDestroySwapchain(swapchain.swapchain), "Failed to destroy swapchain.");
	}

	this->swapchains.clear();

	// Destroy space.
	if (this->space != XR_NULL_HANDLE)
	{
		RETURN_FALSE_ON_OXR_ERROR(xrDestroySpace(this->space), "Failed to destroy space.");
		this->space = XR_NULL_HANDLE;
	}

	// End the session.
	RETURN_FALSE_ON_OXR_ERROR(xrEndSession(this->session), "Failed to end session.");

	return true;
}

std::shared_ptr<Fbo> XrBridge::create_fbo(const GLuint color, const GLsizei width, const GLsizei height) const
{
	std::shared_ptr<Fbo> fbo = std::make_shared<Fbo>();

	// Color attacment
	if (fbo->bindTexture(0, Fbo::BIND_COLORTEXTURE, color) == false)
	{
		Fbo::disable();
		return nullptr;
	}

	// Depth attachment
	if (fbo->bindRenderBuffer(1, Fbo::BIND_DEPTHBUFFER, width, height) == false)
	{
		Fbo::disable();
		return nullptr;
	}

	if (fbo->isOk() == false)
	{
		Fbo::disable();
		return nullptr;
	}

	Fbo::disable();
	return fbo;
}
