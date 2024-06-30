// TODO: Allow using macros to choose the error handling method (exceptions vs is_ready()).
// If a constructor fails, the destructor is not called! Destroy the instance and session in the constructor tehn!

/*
 * For the OpenXR API documentation:
 * 	https://registry.khronos.org/OpenXR/specs/1.1/man/html/FUNCTION_OR_STRUCT.html
 */

#include "queries.hpp"

#include <string>

#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr_platform.h>

namespace XrBridge
{
	class XrBridge
	{
	public:
		XrBridge(const std::string& appication_name, const std::vector<std::string>& requested_api_layers, const std::vector<std::string>>& requested_extensions);
		~XrBridge(void);

		bool is_ready(void) const;
		void update(void) const;
	private:
		bool is_ready_flag;

		XrInstance instance;
		XrSystemId system_id;
		XrSession session;
	};
}

XrBridge::XrBridge::XrBridge(
	const std::string& application_name,
	const std::vector<std::string>& requested_api_layers,
	const std::vector<std::string>>& requested_extensions)
	:
	is_ready_flag = false,
	instance = XR_NULL_HANDLE,
	system_id = XR_NULL_SYSTEM_ID,
	session = XR_NULL_HANDLE,
{
	// Check if application_name is too long.
	if (application_name.size() > XR_MAX_APPLICATION_NAME_SIZE)
	{
		std::cout << "[WARNING] The application name is too long. It will be truncated." << std::endl;
	}

	std::cout << "[INFO] OpenXR version: " << XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_MINOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_PATCH(XR_CURRENT_API_VERSION) << std::endl;

	XrApplicationInfo application_info = {};
	std::strncpy(application_info.applicationName, application_name.c_str(), XR_MAX_APPLICATION_NAME_SIZE);
	application_info.applicationVersion = 1;
	std::strncpy(application_info.engineName, "", XR_MAX_ENGINE_NAME_SIZE);
	application_info.engineVersion = 0;
	application_info.apiVersion = XR_API_VERSION_1_0; // NOTE: This is the OpenXR version to use.


	// Load API layers
	std::vector<const char*> active_api_layers;
	for (const std::string& requested_api_layer : requested_api_layers)
	{
		// Is the layer available?
		if (is_api_layer_supported(requested_api_layer))
		{
			// Load the layer.
			active_api_layers.push_back(requested_api_layer);
		}
		else
		{
			// TODO: Handle errors.
			std::cerr << "[ERROR] API layer \"" << requested_api_layer << "\" is not available." << std::endl;
			return;
		}
	}

	// Load extensions
	std::vector<const char*> active_extensions;
	for (const std::string& requested_extension : requested_extensions)
	{
		// Is the extensions available?
		if (is_extension_supported(requested_extension))
		{
			// Load the extension.
			active_extensions.push_back(requested_extension);
		}
		else
		{
			// TODO: Handle errors.
			std::cerr << "[ERROR] Extension \"" << requested_extension << "\" is not available." << std::endl;
			return;
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
	// TODO: Handle errors.
	xrCreateInstance(&instance_create_info, &this->instance);


	// Load extension functions.
	// Apparently we have to load these manually.
	PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
	// TODO: Handle errors.
	xrGetInstanceProcAddr(this->instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*) &xrGetOpenGLGraphicsRequirementsKHR);


	// Print some information about the OpenXR instance.
	XrInstanceProperties instance_properties = {};
	instance_properties.type = XrStructureType::XR_TYPE_INSTANCE_PROPERTIES;
	// TODO: Handle errors.
	xrGetInstanceProperties(this->instance, &instance_properties);
	std::cout << "[INFO] OpenXR runtime: " << instance_properties.runtimeName << "(" << XR_VERSION_MAJOR(instance_properties.runtimeVersion) << "." << XR_VERSION_MINOR(instance_properties.runtimeVersion) << "." << XR_VERSION_PATCH(instance_properties.runtimeVersion) << ")" << std::endl;


	// Get the ID of the system.
	// The system is the VR headset / AR device / Cave system.
	XrSystemGetInfo system_info = {};
	system_info.type = XrStructureType::XR_TYPE_SYSTEM_GET_INFO;
	// NOTE: This is the for factor that we desire. This use case only requires a HMD.
	system_info.formFactor = XrFormFactor::XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	// TODO: Handle errors.
	xrGetSystem(this->instance, &system_info, &this->system_id);


	// Print the name of the system.
	XrSystemProperties system_properties = {};
	system_properties.type = XrStructureType::XR_TYPE_SYSTEM_PROPERTIES;
	// TODO: Handle errors.
	xrGetSystemProperties(this->instance, this->system_id, &system_properties);
	std::cout << "[INFO] System name: " << system_properties.systemName << std::endl;


	// Create the OpenGL binding.
	// NOTE: Change this to use another graphics API.
	// TODO: Also support Linux.
	XrGraphicsBindingOpenGLWin32KHR graphics_binding = {};
	graphics_binding.type = XrStructureType::XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
	graphics_binding.hDC = hdc;
	graphics_binding.hGLRC = hglrc;


	// TODO: Verify OpenGL version requirements.


	// Create an OpenXR ession.
	XrSessionCreateInfo session_create_info = {};
	session_create_info.type = XrStructureType::XR_TYPE_SESSION_CREATE_INFO;
	session_create_info.next = &graphics_binding;
	session_create_info.createFlags = NULL;
	session_create_info.systemId = this->system_id;
	// TODO: Handle errors.
	xrCreateSession(this->instance, &session_create_info, &this->session);


	this->is_ready_flag = true;
}

XrBridge::XrBridge::~XrBridge()
{
	xrDestroySession(this->session);
	xrDestroyInstance(this->instance);
}

bool XrBridge::XrBridge::is_ready() const
{
	return this->is_ready_flag;
}

void XrBridge::XrBridge::update() const
{
	if (this->is_ready() == false)
	{
		std::cout << "[WARNING] update() has been called while the this instance is not ready." << std::endl;
		return;
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
			xrDestroyInstance(this->instance);
			this->instance = XR_NULL_HANDLE;
			break;
		}
	}
}

/*void render()
 {
	// If the rendering cannot happen (session not running or any other reason),
	//  this binds a dummy FBO.
xr->use_eye(XrBridge::Eye::LEFT);
	//xr->use_eye(XrBridge::Eye::RIGHT);
}*/
