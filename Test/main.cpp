#define _CRT_SECURE_NO_WARNINGS // Disable annoying Visual Studio error about strncpy

#include <algorithm>
#include <iostream>
#include <string>
#include <string.h> // std::strncpy
#include <vector>

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
	extensions.push_back("XR_KHR_opengl_enable");
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
	instance_create_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.createFlags = NULL;
	instance_create_info.enabledApiLayerCount = 0;
	instance_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instance_create_info.enabledExtensionNames = extensions.data();
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



	// Get the System ID




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
