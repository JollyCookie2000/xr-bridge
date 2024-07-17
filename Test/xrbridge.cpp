#include "xrbridge.hpp"

#include <iostream>

#include <Windows.h> // This MUST be included BEFORE FreeGLUT or the gods will not be happy.

#include <GL/freeglut.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr_platform.h>

#define OXR( function ) handle_openxr_errors( this->instance, function );
#define XRV_TO_GV( xrv ) glm::vec3(xrv.x, xrv.y, xrv.z)

#ifdef XRBRIDGE_DEBUG
#define XRBRIDGE_DEBUG_OUT( message ) { std::cout << "[XrBridge][DEBUG] " << message << std::endl; }
#else
#define XRBRIDGE_DEBUG_OUT( message ) ;
#endif

static void handle_openxr_errors(const XrInstance instance, const XrResult result)
{
	if (result != XrResult::XR_SUCCESS)
	{
		char message[64];
		xrResultToString(instance, result, message);
		std::cout << "[ERROR] OpenXR error: " << message << std::endl;

		throw message;
	}
}

static std::vector<XrApiLayerProperties> get_available_api_layers()
{
	uint32_t available_api_layers_count = 0;
	if (xrEnumerateApiLayerProperties(0, &available_api_layers_count, nullptr) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_DEBUG_OUT("Failed to enumerate OpenXR API layers.");
		return {};
	}

	std::vector<XrApiLayerProperties> available_api_layers(available_api_layers_count, { XR_TYPE_API_LAYER_PROPERTIES });
	if (xrEnumerateApiLayerProperties(available_api_layers_count, &available_api_layers_count, available_api_layers.data()) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_DEBUG_OUT("Failed to enumerate OpenXR API layers.");
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
		XRBRIDGE_DEBUG_OUT("Failed to enumerate OpenXR extensions.");
		return {};
	}

	std::vector<XrExtensionProperties> available_extensions(available_extensions_count, { XR_TYPE_EXTENSION_PROPERTIES });
	if (xrEnumerateInstanceExtensionProperties(nullptr, available_extensions_count, &available_extensions_count, available_extensions.data()) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_DEBUG_OUT("Failed to enumerate OpenXR extensions.");
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

XrBridge::XrBridge::XrBridge() :
	is_currently_rendering_flag{ false },
	instance{ XR_NULL_HANDLE },
	system_id{ XR_NULL_SYSTEM_ID },
	session{ XR_NULL_HANDLE },
	session_state{ XrSessionState::XR_SESSION_STATE_UNKNOWN },
	swapchains{ },
	space{ XR_NULL_HANDLE }
{
}

// TODO: Handle errors.
// TODO: Prevent calling this method if is_currently_rendering_flag is true.
// TODO: Prevent calling this method if the object has already been initialized.
bool XrBridge::XrBridge::init(const std::string& application_name)
{
	std::cout << "[INFO] OpenXR version: " << XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_MINOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_PATCH(XR_CURRENT_API_VERSION) << std::endl;

	XrApplicationInfo application_info = {};
	std::strncpy(application_info.applicationName, application_name.c_str(), XR_MAX_APPLICATION_NAME_SIZE);
	application_info.applicationVersion = 1;
	std::strncpy(application_info.engineName, "", XR_MAX_ENGINE_NAME_SIZE);
	application_info.engineVersion = 0;
	application_info.apiVersion = XR_API_VERSION_1_0; // NOTE: This is the OpenXR version to use.


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
			std::cerr << "[ERROR] API layer \"" << requested_api_layer << "\" is not available." << std::endl;
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
			std::cerr << "[ERROR] Extension \"" << requested_extension << "\" is not available." << std::endl;
			return false;
		}
	}


	// Create the OpenXR instance.
	XrInstanceCreateInfo instance_create_info = {};
	instance_create_info.type = XrStructureType::XR_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.createFlags = NULL;
	instance_create_info.enabledApiLayerCount = static_cast<uint32_t>(active_api_layers.size());
	instance_create_info.enabledApiLayerNames = active_api_layers.data();
	instance_create_info.enabledExtensionCount = static_cast<uint32_t>(active_extensions.size());
	instance_create_info.enabledExtensionNames = active_extensions.data();
	instance_create_info.applicationInfo = application_info;
	OXR(xrCreateInstance(&instance_create_info, &this->instance));


	// Load extension functions.
	// Apparently we have to load these manually.
	PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
	OXR(xrGetInstanceProcAddr(this->instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetOpenGLGraphicsRequirementsKHR));


	// Print some information about the OpenXR instance.
	XrInstanceProperties instance_properties = {};
	instance_properties.type = XrStructureType::XR_TYPE_INSTANCE_PROPERTIES;
	OXR(xrGetInstanceProperties(this->instance, &instance_properties));
	std::cout << "[INFO] OpenXR runtime: " << instance_properties.runtimeName << "(" << XR_VERSION_MAJOR(instance_properties.runtimeVersion) << "." << XR_VERSION_MINOR(instance_properties.runtimeVersion) << "." << XR_VERSION_PATCH(instance_properties.runtimeVersion) << ")" << std::endl;


	// Get the ID of the system.
	// The system is the VR headset / AR device / Cave system.
	XrSystemGetInfo system_info = {};
	system_info.type = XrStructureType::XR_TYPE_SYSTEM_GET_INFO;
	// NOTE: This is the form factor that we desire. This use case only requires a HMD.
	system_info.formFactor = XrFormFactor::XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	OXR(xrGetSystem(this->instance, &system_info, &this->system_id));


	// Print the name of the system.
	XrSystemProperties system_properties = {};
	system_properties.type = XrStructureType::XR_TYPE_SYSTEM_PROPERTIES;
	OXR(xrGetSystemProperties(this->instance, this->system_id, &system_properties));
	std::cout << "[INFO] System name: " << system_properties.systemName << std::endl;

	const HDC hdc = wglGetCurrentDC();
	const HGLRC hglrc = wglGetCurrentContext();
	if (hdc == NULL || hglrc == NULL)
	{
		std::cerr << "[ERROR] Failed to get native OpenGL context." << std::endl;
		return false;
	}

	// Create the OpenGL binding.
	// NOTE: Change this to use another graphics API.
	// TODO: Also support Linux.
	XrGraphicsBindingOpenGLWin32KHR graphics_binding = {};
	graphics_binding.type = XrStructureType::XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
	graphics_binding.hDC = hdc;
	graphics_binding.hGLRC = hglrc;


	// TODO: Verify OpenGL version requirements.
	XrGraphicsRequirementsOpenGLKHR graphics_requirements = {};
	graphics_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
	xrGetOpenGLGraphicsRequirementsKHR(this->instance, this->system_id, &graphics_requirements);


	// Create an OpenXR session.
	XrSessionCreateInfo session_create_info = {};
	session_create_info.type = XrStructureType::XR_TYPE_SESSION_CREATE_INFO;
	session_create_info.next = &graphics_binding;
	session_create_info.createFlags = NULL;
	session_create_info.systemId = this->system_id;
	OXR(xrCreateSession(this->instance, &session_create_info, &this->session));

	return true;
}

// TODO: Handle errors.
// TODO: Prevent calling this method if is_currently_rendering_flag is true.
// TODO: Prevent calling this method if resources have already been freed.
bool XrBridge::XrBridge::free()
{
	this->end_session();

	if (xrDestroySession(this->session) != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_DEBUG_OUT("Failed to destroy session.");
	}

	if (xrDestroyInstance(this->instance); != XrResult::XR_SUCCESS)
	{
		XRBRIDGE_DEBUG_OUT("Failed to destroy instance.");
	}

	return true;
}

// TODO: Maybe return false if the application should quit.
// TODO: Prevent calling this method if the object has not been initialized yet.
void XrBridge::XrBridge::update()
{
	if (this->is_currently_rendering_flag)
	{
		// TODO: Handle errors.
		std::cerr << "[ERROR] Another method was called inside the rendering method. This is not allowed!" << std::endl;
		throw "";
	}

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
			// TODO: Handle errors.
			std::cerr << "[ERROR] There was an error while polling for events." << std::endl;
			break;
		}

		switch (event_buffer.type)
		{
			// The event queue has overflown and some events were lost.
		case XrStructureType::XR_TYPE_EVENT_DATA_EVENTS_LOST:
			std::cout << "[WARNING] The event queue has overflown." << std::endl;
			break;
		case XrStructureType::XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			// https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrEventDataInstanceLossPending.html
			// TODO: Should this case be handled? Or can I just stop working?
			// The OpenXR instance is about to disconnect. This should not normally happen.
			std::cout << "[WARNING] The OpenXR instance is about to be lost. Recovery from this is not supported." << std::endl;
			OXR(xrDestroyInstance(this->instance));
			this->instance = XR_NULL_HANDLE;
			break;
		case XrStructureType::XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
		{
			// TODO: Should this be supported?
			const XrEventDataReferenceSpaceChangePending* reference_space_change = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&event_buffer);
			if (reference_space_change->session != this->session)
			{
				break;
			}

			std::cout << "[DEBUG] XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING" << std::endl;

			break;
		}
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
				std::cout << "[DEBUG] Session state has changed: XR_SESSION_STATE_READY." << std::endl;
				this->begin_session();
			}
			else if (session_state_changed->state == XrSessionState::XR_SESSION_STATE_STOPPING)
			{
				this->end_session();
			}
			else if (session_state_changed->state == XrSessionState::XR_SESSION_STATE_LOSS_PENDING)
			{
				// TODO: Exit the application.
			}
			else if (session_state_changed->state == XrSessionState::XR_SESSION_STATE_EXITING)
			{
				// TODO: Exit the application.
			}

			break;
		}
		}
	}
}

// TODO: Prevent calling this method if the object has not been initialized yet.
void XrBridge::XrBridge::render(const render_function_t render_function)
{
	if (this->is_currently_rendering_flag)
	{
		// TODO: Handle errors.
		std::cerr << "[ERROR] Another method was called inside the rendering method. This is not allowed!" << std::endl;
		throw "";
	}

	this->is_currently_rendering_flag = true;

	XrFrameState frame_state = {};
	frame_state.type = XrStructureType::XR_TYPE_FRAME_STATE;
	XrFrameWaitInfo frame_wait_info = {};
	frame_wait_info.type = XrStructureType::XR_TYPE_FRAME_WAIT_INFO;
	// Wait for synchronization with the headset display.
	OXR(xrWaitFrame(this->session, &frame_wait_info, &frame_state));

	XrFrameBeginInfo frame_begin_info = {};
	frame_begin_info.type = XrStructureType::XR_TYPE_FRAME_BEGIN_INFO;
	OXR(xrBeginFrame(this->session, &frame_begin_info));

	std::vector<XrCompositionLayerBaseHeader*> layers = {};

	const bool is_session_active =
		this->session_state == XrSessionState::XR_SESSION_STATE_SYNCHRONIZED ||
		this->session_state == XrSessionState::XR_SESSION_STATE_VISIBLE ||
		this->session_state == XrSessionState::XR_SESSION_STATE_FOCUSED;

	bool did_render = false;

	// 3D view
	XrCompositionLayerProjection composition_layer_projection = {};
	composition_layer_projection.type = XrStructureType::XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	composition_layer_projection.layerFlags = NULL;
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
			OXR(xrAcquireSwapchainImage(current_swapchain.swapchain, &swapchain_image_acquire_info, &image_index));

			XrSwapchainImageWaitInfo swapchain_image_wait_info = {};
			swapchain_image_wait_info.type = XrStructureType::XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
			swapchain_image_wait_info.timeout = XR_INFINITE_DURATION;
			OXR(xrWaitSwapchainImage(current_swapchain.swapchain, &swapchain_image_wait_info));

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
			const float aspect_ratio = static_cast<float>(current_swapchain.width) / static_cast<float>(current_swapchain.height);

			// TODO: Make the clipping planes configurable.
			const glm::mat4 projection_matrix = glm::perspective(current_view.fov.angleUp - current_view.fov.angleDown, aspect_ratio, 0.1f, 100'000.0f);
			glm::quat quaternion(current_view.pose.orientation.w, current_view.pose.orientation.x, current_view.pose.orientation.y, current_view.pose.orientation.z);
			const glm::mat4 view_matrix = glm::translate(glm::mat4(1.0f), XRV_TO_GV(current_view.pose.position)) * glm::mat4_cast(quaternion);

			const std::shared_ptr<Fbo> fbo = current_swapchain.framebuffers.at(image_index);

			render_function(eye, fbo, projection_matrix, view_matrix);

			XrSwapchainImageReleaseInfo swapchain_image_release_info = {};
			swapchain_image_release_info.type = XrStructureType::XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
			OXR(xrReleaseSwapchainImage(current_swapchain.swapchain, &swapchain_image_release_info));
		}

		Fbo::disable();

		composition_layer_projection.viewCount = static_cast<uint32_t>(composition_layer_projection_views.size());
		composition_layer_projection.views = composition_layer_projection_views.data();

		// NOTE: You can add more layers here.
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
	OXR(xrEndFrame(this->session, &frame_end_info));

	this->is_currently_rendering_flag = false;
}

void XrBridge::XrBridge::begin_session()
{
	XrSessionBeginInfo session_begin_info = {};
	session_begin_info.type = XrStructureType::XR_TYPE_SESSION_BEGIN_INFO;
	// NOTE: This is the view cofiguration type that we desire. This use case only requires stereo (two eyes).
	session_begin_info.primaryViewConfigurationType = XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	// TODO: Handle errors.
	OXR(xrBeginSession(this->session, &session_begin_info));

	// A view more or less equates to a display. In the case of a VR headset, since we have 2 eyes, we will ave 2 views.
	uint32_t view_count = 0;
	OXR(xrEnumerateViewConfigurationViews(this->instance, this->system_id, XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &view_count, nullptr));
	std::vector<XrViewConfigurationView> view_configuration_views(view_count, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	OXR(xrEnumerateViewConfigurationViews(this->instance, this->system_id, XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, view_count, &view_count, view_configuration_views.data()));

	// TODO: Return an error if the number of views is not 2.

	for (const auto& view_configuration_view : view_configuration_views)
	{
		Swapchain swapchain = {};
		swapchain.width = view_configuration_view.recommendedImageRectWidth;
		swapchain.height = view_configuration_view.recommendedImageRectHeight;

		XrSwapchainCreateInfo swapchain_create_info = {};
		swapchain_create_info.type = XrStructureType::XR_TYPE_SWAPCHAIN_CREATE_INFO;
		swapchain_create_info.createFlags = NULL;
		// TODO: Is XR_SWAPCHAIN_USAGE_SAMPLED_BIT actually necessary?
		// TODO: Apparently, OpenGL ignores these. If so, remove them.
		swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		// https://steamcommunity.com/app/250820/discussions/8/3121550424355682585/
		swapchain_create_info.format = GL_RGBA16F; // TODO: Allow the user to choose this.
		swapchain_create_info.width = view_configuration_view.recommendedImageRectWidth;
		swapchain_create_info.height = view_configuration_view.recommendedImageRectHeight;
		swapchain_create_info.sampleCount = view_configuration_view.recommendedSwapchainSampleCount;
		swapchain_create_info.faceCount = 1;
		swapchain_create_info.arraySize = 1;
		swapchain_create_info.mipCount = 1;
		// TODO: Handle errors.
		OXR(xrCreateSwapchain(this->session, &swapchain_create_info, &swapchain.swapchain));

		// Create the single images inside the swapchain.
		// If the runtime is using double-buffering, there will be 2 images per swapchain. For triple buffering,
		//  the images will be 3.
		uint32_t swapchain_image_count = 0;
		OXR(xrEnumerateSwapchainImages(swapchain.swapchain, 0, &swapchain_image_count, nullptr));
		std::vector<XrSwapchainImageOpenGLKHR> swapchain_images(swapchain_image_count, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR }); // NOTE: Change this to use another graphics API.
		OXR(xrEnumerateSwapchainImages(swapchain.swapchain, swapchain_image_count, &swapchain_image_count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchain_images.data())));

		for (const auto& swapchain_image : swapchain_images)
		{
			const std::shared_ptr<Fbo> fbo = this->create_fbo(swapchain_image.image, swapchain.width, swapchain.height);

			swapchain.framebuffers.push_back(fbo);
		}

		this->swapchains.push_back(swapchain);
	}

	// Create the reference space.
	XrReferenceSpaceCreateInfo reference_space_info = {};
	reference_space_info.type = XrStructureType::XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	// NOTE: The LOCAL reference space refers to the user in the sitting position.
	//  More info: https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrReferenceSpaceType.html
	// TODO: Make this configurable.
	//reference_space_info.referenceSpaceType = XrReferenceSpaceType::XR_REFERENCE_SPACE_TYPE_STAGE;
	reference_space_info.referenceSpaceType = XrReferenceSpaceType::XR_REFERENCE_SPACE_TYPE_LOCAL;
	// Here we set the offset to the origin of the tracking space. In this case, we keep the origin were it is.
	reference_space_info.poseInReferenceSpace = {
		{ 0.0f, 0.0f, 0.0f, 1.0f, }, // Orientation
		{ 0.0f, 0.0f, 0.0f }, // Position
	};

	OXR(xrCreateReferenceSpace(this->session, &reference_space_info, &this->space));
}

void XrBridge::XrBridge::end_session()
{
	// Destroy swapchains.
	for (const auto& swapchain : this->swapchains)
	{
		// TODO: Handle errors.
		OXR(xrDestroySwapchain(swapchain.swapchain));
	}

	this->swapchains.clear();

	// Destroy space.
	if (this->space != XR_NULL_HANDLE)
	{
		OXR(xrDestroySpace(this->space));
		this->space = XR_NULL_HANDLE;
	}

	// End the session.
	xrEndSession(this->session);
}

std::shared_ptr<Fbo> XrBridge::XrBridge::create_fbo(const GLuint color, const GLsizei width, const GLsizei height) const
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
