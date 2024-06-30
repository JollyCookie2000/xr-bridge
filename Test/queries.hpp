#include <string>
#include <vector>

#include <openxr/openxr.h>

// TODO: Consider moving all of these definitions to xrbridge.hpp and mark them as static. Also remove the namespace.
namespace XrBridge
{
	std::vector<XrApiLayerProperties> get_available_api_layers()
	{
		uint32_t available_api_layers_count = 0;
		// TODO: Handle errors.
		xrEnumerateApiLayerProperties(0, &available_api_layers_count, nullptr);
		std::vector<XrApiLayerProperties> available_api_layers(available_api_layers_count, { XR_TYPE_API_LAYER_PROPERTIES });
		// TODO: Handle errors.
		xrEnumerateApiLayerProperties(available_api_layers_count, &available_api_layers_count, available_api_layers.data());

		return available_api_layers;
	}

	bool is_api_layer_supported(const std::string& api_layer_name)
	{
		for (const XrApiLayerProperties& api_layer : get_available_api_layers())
			if (api_layer_name == api_layer.layerName)
				return true;

		return false;
	}

	std::vector<XrExtensionProperties> get_available_extensions()
	{
		uint32_t available_extensions_count = 0;
		// TODO: Handle errors.
		xrEnumerateInstanceExtensionProperties(nullptr, 0, &available_extensions_count, nullptr);
		std::vector<XrExtensionProperties> available_extensions(available_extensions_count, { XR_TYPE_EXTENSION_PROPERTIES });
		// TODO: Handle errors.
		xrEnumerateInstanceExtensionProperties(nullptr, available_extensions_count, &available_extensions_count, available_extensions.data());

		return available_extensions;
	}

	bool is_extension_supported(const std::string& extension_name)
	{
		for (const XrExtensionProperties& extensions : get_available_extensions())
		{
			if (extension_name == extensions.extensionName)
				return true;
		}

		return false;
	}
}
