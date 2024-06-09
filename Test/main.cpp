// TODO: Improve error handling.
// TODO: Correctly destroy resources at the end.
// TODO: In the final API, maybe offer a builder to more easily build an instance.
// TODO: Use a namespace.

#define _CRT_SECURE_NO_WARNINGS // Disable annoying Visual Studio error about strncpy

#include <algorithm>
#include <iostream>
#include <string>
#include <string.h> // std::strncpy
#include <vector>

#include <Windows.h> // This MUST be included BEFORE FreeGLUT.

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr_platform.h>

void poll_events(const XrInstance& instance, const XrSession& session);
void begin_session(const XrInstance& instance, const XrSession& session);
GLuint create_fbo(const XrSwapchainImageOpenGLKHR& image);

static bool running = true;

static XrSessionState session_state = XrSessionState::XR_SESSION_STATE_UNKNOWN;
static XrSystemId system_id = 0;
static std::vector<GLuint> left_eye;
static std::vector<GLuint> right_eye;

int main(int argc, char** argv)
{
	// This is supposed to be done by the user.
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);

	glutInitContextVersion(4, 4);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitContextFlags(GLUT_DEBUG);

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutInitWindowSize(800, 600);
	const int window = glutCreateWindow("OpenXR Demo");
	glutDisplayFunc([]() {});
	glutCloseFunc([]() {
		running = false;
	});

	if (glewInit() != GLEW_OK)
	{
		throw "[ERROR] Failed to initialize GLEW.";
	}



	// TODO: Check that these are not NULL.
	const HDC hdc = wglGetCurrentDC();
	const HGLRC hglrc = wglGetCurrentContext();
	if (hdc == NULL || hglrc == NULL)
	{
		std::cerr << "[ERROR] Failed to get native OpenGL context." << std::endl;
		return 1;
	}

	std::cout << "[INFO] OpenXR version: " << XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_MINOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_PATCH(XR_CURRENT_API_VERSION) << std::endl;

	/*GLint major = 0;
	GLint minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	std::cout << "[INFO] OpenGL version: " << major << "." << minor << std::endl;*/

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrApplicationInfo.html
	XrApplicationInfo application_info = {};
	std::strncpy(application_info.applicationName, "Hello OpenXR", XR_MAX_APPLICATION_NAME_SIZE);
	application_info.applicationVersion = 1;
	std::strncpy(application_info.engineName, "Hello, OpenXR!", XR_MAX_ENGINE_NAME_SIZE);
	application_info.engineVersion = 0;
	application_info.apiVersion = XR_API_VERSION_1_0;

	// Not sure this is needed
	uint32_t available_api_layers_count = 0;
	xrEnumerateApiLayerProperties(0, &available_api_layers_count, nullptr);
	std::vector<XrApiLayerProperties> available_api_layers;
	available_api_layers.resize(available_api_layers_count, { XR_TYPE_API_LAYER_PROPERTIES });
	xrEnumerateApiLayerProperties(available_api_layers_count, &available_api_layers_count, available_api_layers.data());
	std::cout << "Available layers:" << std::endl;
	for (const auto &api_layer : available_api_layers)
	{
		std::cout << " - " << api_layer.layerName << std::endl;
	}

	// List available extensions
	uint32_t available_extensions_count = 0;
	xrEnumerateInstanceExtensionProperties(nullptr, 0, &available_extensions_count, nullptr);
	std::vector<XrExtensionProperties> available_extensions;
	available_extensions.resize(available_extensions_count, { XR_TYPE_EXTENSION_PROPERTIES });
	xrEnumerateInstanceExtensionProperties(nullptr, available_extensions_count, &available_extensions_count, available_extensions.data());
	std::cout << "Available extensions:" << std::endl;
	for (const auto& extension : available_extensions)
	{
		std::cout << " - " << extension.extensionName << std::endl;
	}

	// Configure extensions
	std::vector<const char*> extensions;
	extensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
	extensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
	std::cout << "Enabled extensions:" << std::endl;
	for (const auto& extension : extensions)
	{
		std::cout << " - " << extension << std::endl;
	}

	// Make sure the needed extensions are actually available
	for (const std::string enabled_extension : extensions)
	{
		if (std::find_if(available_extensions.begin(), available_extensions.end(), [&enabled_extension] (const XrExtensionProperties &available_extension) { return enabled_extension == available_extension.extensionName; }) == available_extensions.end())
		{
			std::cerr << "[ERROR] Extension \"" << enabled_extension << "\" is not available." << std::endl;
			return 1;
		}
	}

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrInstanceCreateInfo.html
	XrInstanceCreateInfo instance_create_info = {};
	instance_create_info.type = XrStructureType::XR_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.createFlags = NULL;
	instance_create_info.enabledApiLayerCount = 0;
	instance_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instance_create_info.enabledExtensionNames = extensions.data();
	instance_create_info.applicationInfo = application_info;

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrInstance.html
	XrInstance instance = {};

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrCreateInstance.html
	const XrResult create_instance_result = xrCreateInstance(&instance_create_info, &instance);
	if (create_instance_result != XrResult::XR_SUCCESS)
	{
		std::cerr << "[ERROR] Failed to create OpenXR instance. XrResult = " << create_instance_result << "." << std::endl;

		char message[64];
		xrResultToString(instance, create_instance_result, message);
		std::cerr << message << std::endl;

		return 1;
	}

	// Load extension functions.
	// Apparently the loader does not load extensions, so we have to do it manually.
	PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
	xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*) &xrGetOpenGLGraphicsRequirementsKHR);
	if (xrGetOpenGLGraphicsRequirementsKHR == nullptr)
	{
		std::cerr << "[ERROR] Failed to get function pointer to xrGetOpenGLGraphicsRequirementsKHR" << std::endl;
		return 1;
	}

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrInstanceProperties.html
	XrInstanceProperties instance_properties = {};
	instance_properties.type = XrStructureType::XR_TYPE_INSTANCE_PROPERTIES;
	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrGetInstanceProperties.html
	// TODO: Add error handling.
	xrGetInstanceProperties(instance, &instance_properties);
	std::cout << "[INFO] Runtime name: " <<	instance_properties.runtimeName << std::endl;
	std::cout << "[INFO] Runtime version: " << XR_VERSION_MAJOR(instance_properties.runtimeVersion) << "." << XR_VERSION_MINOR(instance_properties.runtimeVersion) << "." << XR_VERSION_PATCH(instance_properties.runtimeVersion) << std::endl;

	// Get the System ID
	XrSystemGetInfo system_info = {};
	system_info.type = XrStructureType::XR_TYPE_SYSTEM_GET_INFO;
	system_info.formFactor = XrFormFactor::XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	// TODO: Add error handling.
	xrGetSystem(instance, &system_info, &system_id);

	// Get system properties
	XrSystemProperties system_properties = {};
	system_properties.type = XrStructureType::XR_TYPE_SYSTEM_PROPERTIES;;
	// TODO: Add error handling.
	xrGetSystemProperties(instance, system_id, &system_properties);
	std::cout << "[INFO] System name: " << system_properties.systemName << std::endl;

	// Create an OpenGL binding (this only works for Win32!)
	XrGraphicsBindingOpenGLWin32KHR binding_opengl = {};
	binding_opengl.type = XrStructureType::XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
	binding_opengl.hDC = hdc;
	binding_opengl.hGLRC = hglrc;

	// TODO: Verify that the OpenGL version requirement is satisfied.
	XrGraphicsRequirementsOpenGLKHR opengl_requirements = {};
	opengl_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
	xrGetOpenGLGraphicsRequirementsKHR(instance, system_id, &opengl_requirements);

	std::cout << "[INFO] Minimum OpenGL version: " << XR_VERSION_MAJOR(opengl_requirements.minApiVersionSupported) << "." << XR_VERSION_MINOR(opengl_requirements.minApiVersionSupported) << "." << XR_VERSION_PATCH(opengl_requirements.minApiVersionSupported) << std::endl;
	std::cout << "[INFO] Maximum OpenGL version: " << XR_VERSION_MAJOR(opengl_requirements.maxApiVersionSupported) << "." << XR_VERSION_MINOR(opengl_requirements.maxApiVersionSupported) << "." << XR_VERSION_PATCH(opengl_requirements.maxApiVersionSupported) << std::endl;

	// Create a session
	XrSessionCreateInfo session_create_info = {};
	session_create_info.type = XrStructureType::XR_TYPE_SESSION_CREATE_INFO;
	session_create_info.next = &binding_opengl;
	session_create_info.createFlags = NULL;
	session_create_info.systemId = system_id;
	XrSession session = XR_NULL_HANDLE;
	const XrResult create_session_result = xrCreateSession(instance, &session_create_info, &session);
	if (create_session_result != XrResult::XR_SUCCESS)
	{
		std::cerr << "[ERROR] Failed to create OpenXR session. XrResult = " << create_session_result << "." << std::endl;

		char message[64];
		xrResultToString(instance, create_session_result, message);
		std::cerr << message << std::endl;

		return 1;
	}



	std::cout << std::endl << "Application stuff is happening!!!" << std::endl;
	while (running)
	{
		poll_events(instance, session);

		glutMainLoopEvent();
	}

	

	// TODO: Add error handling.
	xrDestroySession(session);
	
	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrDestroyInstance.html
	const XrResult destroy_instance_result = xrDestroyInstance(instance);
	if (destroy_instance_result != XrResult::XR_SUCCESS)
	{
		std::cerr << "[ERROR] Failed to destroy OpenXR instance. XrResult = " << destroy_instance_result << "." << std::endl;

		char message[64];
		xrResultToString(instance, destroy_instance_result, message);
		std::cerr << message << std::endl;

		return 1;
	}

	glutLeaveMainLoop();

	return 0;
}

// Process all the events since the last frame. There could be multiple events per frame.
// TODO: There's more tuff in the tutorial that I'm ignoring, like session state handling and events. Maybe come back to it later.
void poll_events(const XrInstance& instance, const XrSession& session)
{
	while (true)
	{
		XrEventDataBuffer event_buffer = {};
		event_buffer.type = XrStructureType::XR_TYPE_EVENT_DATA_BUFFER;

		const XrResult poll_event_result = xrPollEvent(instance, &event_buffer);
		if (poll_event_result == XrResult::XR_EVENT_UNAVAILABLE)
		{
			// There are no more events to process, return.
			break;
		}
		else if (poll_event_result == XrResult::XR_SUCCESS)
		{
			// More events are availabel, continue.
		}
		else
		{
			std::cerr << "[ERROR] There was an error while polling for events. XrResult = " << poll_event_result << "." << std::endl;

			char message[64];
			xrResultToString(instance, poll_event_result, message);
			std::cerr << message << std::endl;

			break;
		}

		switch (event_buffer.type)
		{
			// The event queue has overflown and some events were lost.
			case XrStructureType::XR_TYPE_EVENT_DATA_EVENTS_LOST:
				std::cout << "[WARNING] The event queue has overflown." << std::endl;
				break;
			case XrStructureType::XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
				std::cout << "XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING" << std::endl;
				break;
			case XrStructureType::XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
				std::cout << "XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED" << std::endl;
				break;
			case XrStructureType::XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
				std::cout << "XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING" << std::endl;
				break;
			// The session state has changed.
			case XrStructureType::XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				// https://openxr-tutorial.com/windows/opengl/_images/openxr-session-life-cycle.svg
				std::cout << "XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED" << std::endl;

				const XrEventDataSessionStateChanged* new_session_state = reinterpret_cast<XrEventDataSessionStateChanged*>(&event_buffer);
				if (new_session_state->session != session)
				{
					// The state has changed for another session, IDK how or why this would happen,
					// but let's ignore it. That's what the tutorial does anyway.
					break;
				}

				session_state = new_session_state->state;

				switch (new_session_state->state)
				{
					case XrSessionState::XR_SESSION_STATE_READY:
						std::cout << "[INFO] Session state changed to READY." << std::endl;
						// The session is now ready, start doing VR stuff.
						begin_session(instance, session);
						break;
					case XrSessionState::XR_SESSION_STATE_STOPPING:
						std::cout << "[INFO] Session state changed to STOPPING." << std::endl;
						// The session is ending, stop doing VR stuff.
						// This is different from exiting. Stopping means that the session could restart again.
						// TODO: End session.
						break;
					case XrSessionState::XR_SESSION_STATE_LOSS_PENDING:
						std::cout << "[INFO] Session state changed to LOSS PENDING." << std::endl;
						// I think this is similar to EXITING, but unexpected. Just treat this as the application exiting.
					case XrSessionState::XR_SESSION_STATE_EXITING:
						std::cout << "[INFO] Session state changed to EXITING." << std::endl;
						// The session is ending, stop doing VR stuff.
						// This is different from stopping. Exiting means that the application is closing.
						// TODO: End session.
						break;
					default:
						// Ignore the other states.
						break;
				}

				break;
			}
			default:
				break;
		}
	}
}

void begin_session(const XrInstance& instance, const XrSession& session)
{
	XrSessionBeginInfo session_begin_info = {};
	session_begin_info.type = XrStructureType::XR_TYPE_SESSION_BEGIN_INFO;
	session_begin_info.primaryViewConfigurationType = XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; // We have two eyes.

	const XrResult session_begin_result = xrBeginSession(session, &session_begin_info);
	if (session_begin_result != XrResult::XR_SUCCESS)
	{
		std::cerr << "[ERROR] Failed to begin session. XrResult = " << session_begin_result << "." << std::endl;

		char message[64];
		xrResultToString(instance, session_begin_result, message);
		std::cerr << message << std::endl;

		exit(1);
	}



	// Create swapchains (one for each eye)
	XrSwapchainCreateInfo swapchain_create_info = {};
	swapchain_create_info.type = XrStructureType::XR_TYPE_SWAPCHAIN_CREATE_INFO;
	swapchain_create_info.createFlags = NULL;
	// TODO: Is XR_SWAPCHAIN_USAGE_SAMPLED_BIT actually necessary?
	// TODO: Apparently, OpenGL ignores these. If so, remove them.
	swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	// https://steamcommunity.com/app/250820/discussions/8/3121550424355682585/
	swapchain_create_info.format = GL_SRGB8; // TODO: Improve this.
	swapchain_create_info.width = 1512; // TODO: Do not make this hard-coded.
	swapchain_create_info.height = 1680; // TODO: Do not make this hard-coded.
	swapchain_create_info.sampleCount = 1; // TODO: Do not make this hard-coded.
	swapchain_create_info.faceCount = 1;
	swapchain_create_info.arraySize = 1;
	swapchain_create_info.mipCount = 1;



	// Debug stuff.
	// These are the recommended view configurations offered by the runtime.
	uint32_t count = 0; // This should always be 2.
	xrEnumerateViewConfigurationViews(instance, system_id, XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr);
	std::vector<XrViewConfigurationView> stuff;
	stuff.resize(count, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	xrEnumerateViewConfigurationViews(instance, system_id, XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count, stuff.data());
	std::cout << "Stuff:" << std::endl;
	for (const auto& thing : stuff)
	{
		std::cout << " - Resolution: " << thing.recommendedImageRectWidth << "x" << thing.recommendedImageRectHeight << "; Sample count: " << thing.recommendedSwapchainSampleCount << std::endl;
	}



	XrSwapchain swapchain_left_eye;
	XrSwapchain swapchain_right_eye;
	const XrResult create_swapchain_result = xrCreateSwapchain(session, &swapchain_create_info, &swapchain_left_eye);
	if (create_swapchain_result != XrResult::XR_SUCCESS)
	{
		std::cerr << "[ERROR] Failed to create swapchain. XrResult = " << create_swapchain_result << "." << std::endl;

		char message[64];
		xrResultToString(instance, create_swapchain_result, message);
		std::cerr << message << std::endl;

		exit(1);
	}
	// If the first one succeeded, the second one also will (probably).
	xrCreateSwapchain(session, &swapchain_create_info, &swapchain_right_eye);

	// Left eye
	uint32_t swapchain_left_image_count = 0;
	xrEnumerateSwapchainImages(swapchain_left_eye, 0, &swapchain_left_image_count, nullptr);
	std::vector<XrSwapchainImageOpenGLKHR> images_left_eye;
	images_left_eye.resize(swapchain_left_image_count, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
	xrEnumerateSwapchainImages(swapchain_left_eye, swapchain_left_image_count, &swapchain_left_image_count, reinterpret_cast<XrSwapchainImageBaseHeader*>(images_left_eye.data()));
	for (const auto& image_left_eye : images_left_eye)
	{
		left_eye.push_back(create_fbo(image_left_eye));
	}

	// Right eye
	uint32_t swapchain_right_image_count = 0;
	xrEnumerateSwapchainImages(swapchain_right_eye, 0, &swapchain_right_image_count, nullptr);
	std::vector<XrSwapchainImageOpenGLKHR> images_right_eye;
	images_right_eye.resize(swapchain_right_image_count, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
	xrEnumerateSwapchainImages(swapchain_right_eye, swapchain_right_image_count, &swapchain_right_image_count, reinterpret_cast<XrSwapchainImageBaseHeader*>(images_right_eye.data()));
	for (const auto& image_right_eye : images_right_eye)
	{
		right_eye.push_back(create_fbo(image_right_eye));
	}

	// Reference space
	XrReferenceSpaceCreateInfo reference_space_info = {};
	reference_space_info.type = XrStructureType::XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	reference_space_info.referenceSpaceType = XrReferenceSpaceType::XR_REFERENCE_SPACE_TYPE_LOCAL;
	// Here we set the offset to the origin of the tracking space. In this case, we keep the origin were it is.
	reference_space_info.poseInReferenceSpace = {
		{ 0.0f, 0.0f, 0.0f, 1.0f, }, // Orientation
		{ 0.0f, 0.0f, 0.0f }, // Position
	};
	// TODO: Add error handling.
	XrSpace space;
	xrCreateReferenceSpace(session, &reference_space_info, &space);
	// TODO: Destroy space.

	std::cout << "[INFO] OpenXR session has begun." << std::endl;
}

void end_session(const XrInstance& instance, const XrSession& session)
{
	const XrResult session_end_result = xrEndSession(session);
	if (session_end_result != XrResult::XR_SUCCESS)
	{
		std::cerr << "[ERROR] Failed to end session. XrResult = " << session_end_result << "." << std::endl;

		char message[64];
		xrResultToString(instance, session_end_result, message);
		std::cerr << message << std::endl;

		exit(1);
	}


	// TODO: Destroy the swapchains. https://openxr-tutorial.com//android/vulkan/3-graphics.html#destroy-the-swapchain
	// TODO: Destroy the frambuffers.

	std::cout << "[INFO] OpenXR session has ended." << std::endl;
}

GLuint create_fbo(const XrSwapchainImageOpenGLKHR& image)
{
	// Create the framebuffer
	GLuint framebuffer_id;
	glGenFramebuffers(1, &framebuffer_id);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);

	// Bind the texture
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image.image, 0);


	if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "[ERROR] Failed to create framebuffer." << std::endl;
		exit(1);
	}


	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return framebuffer_id;
}
