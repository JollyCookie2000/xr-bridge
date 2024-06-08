#define _CRT_SECURE_NO_WARNINGS // Disable annoying Visual Studio error about strncpy

#include <algorithm>
#include <iostream>
#include <string>
#include <string.h> // std::strncpy
#include <vector>

#include <Windows.h> // This MUST be included BEFORE FreeGLUT.

#include <GL/freeglut.h>

#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr_platform.h>

void poll_events(const XrInstance& instance, const XrSession& session);
void begin_session(const XrInstance& instance, const XrSession& session);

static bool running = true;
static XrSessionState session_state = XrSessionState::XR_SESSION_STATE_UNKNOWN;

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
	XrSystemId system_id;
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

	std::cout << "[INFO] OpenXR session has ended." << std::endl;
}
