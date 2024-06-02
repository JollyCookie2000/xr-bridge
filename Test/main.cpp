#define _CRT_SECURE_NO_WARNINGS // Disable annoying Visual Studio error about strncpy

#include <iostream>

#include <openxr/openxr.h>

int main()
{
	std::cout << "[INFO] OpenXR version: " << XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_MINOR(XR_CURRENT_API_VERSION) << "." << XR_VERSION_PATCH(XR_CURRENT_API_VERSION) << std::endl;

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrApplicationInfo.html
	XrApplicationInfo application_info = {};
	std::strncpy(application_info.applicationName, "Hello OpenXR", XR_MAX_APPLICATION_NAME_SIZE);
	application_info.applicationVersion = 1;
	std::strncpy(application_info.engineName, "Hello, OpenXR!", XR_MAX_ENGINE_NAME_SIZE);
	application_info.engineVersion = 0;
	application_info.apiVersion = XR_API_VERSION_1_0;

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrInstanceCreateInfo.html
	XrInstanceCreateInfo instance_create_info = {};
	instance_create_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.createFlags = NULL;
	instance_create_info.enabledApiLayerCount = 0;
	instance_create_info.enabledExtensionCount = 0;
	instance_create_info.applicationInfo = application_info;

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrInstance.html
	XrInstance instance = {};

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrCreateInstance.html
	XrResult create_instance_result = xrCreateInstance(&instance_create_info, &instance);
	if (create_instance_result != XrResult::XR_SUCCESS)
	{
		std::cerr << "[ERROR] Failed to create OpenXR instance. XrResult = " << create_instance_result << "." << std::endl;

		char message[64];
		xrResultToString(instance, create_instance_result, message);
		std::cerr << message << std::endl;

		return 1;
	}


	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrInstanceProperties.html
	XrInstanceProperties instance_properties = {};
	instance_properties.type = XR_TYPE_INSTANCE_PROPERTIES;
	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrGetInstanceProperties.html
	// TODO: Error handling.
	xrGetInstanceProperties(instance, &instance_properties);
	std::cout << "[INFO] Runtime name: " <<	instance_properties.runtimeName << std::endl;
	std::cout << "[INFO] Runtime version: " << XR_VERSION_MAJOR(instance_properties.runtimeVersion) << "." << XR_VERSION_MINOR(instance_properties.runtimeVersion) << "." << XR_VERSION_PATCH(instance_properties.runtimeVersion) << std::endl;



	std::cout << std::endl << "Application stuff is happening!!!" << std::endl;

	

	// https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrDestroyInstance.html
	XrResult destroy_instance_result = xrDestroyInstance(instance);
	if (destroy_instance_result != XrResult::XR_SUCCESS)
	{
		std::cerr << "[ERROR] Failed to destroy OpenXR instance. XrResult = " << destroy_instance_result << "." << std::endl;

		char message[64];
		xrResultToString(instance, destroy_instance_result, message);
		std::cerr << message << std::endl;

		return 1;
	}

	return 0;
}
