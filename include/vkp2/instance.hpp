#pragma once
#include <span>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkp
{
	class InstanceBuilder
	{
	public:
		struct ReturnData
		{
			VkInstance instance;
			VkDebugUtilsMessengerEXT debugMessenger;
		};

		using DebugCallback = VkBool32(*)(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT*, void*);

		explicit InstanceBuilder();
		InstanceBuilder& setAppInfo(std::string_view p_ApplicationName, uint32_t p_ApplicationVersion, std::string_view p_EngineName, uint32_t p_EngineVersion);
		InstanceBuilder& enableValidationLayers(DebugCallback p_Callback = nullptr, VkDebugUtilsMessageSeverityFlagsEXT p_Severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
		InstanceBuilder& addLayer(const char* p_Layer);
		InstanceBuilder& addLayers(std::span<const char* const> p_Layers);
		InstanceBuilder& addExtension(const char* p_Extension);
		InstanceBuilder& addExtensions(std::span<const char* const> p_Extensions);
		
		[[nodiscard]] ReturnData build() const;
	private:
		VkInstanceCreateInfo m_createInfo{};
		VkApplicationInfo m_appInfo{};

		VkDebugUtilsMessengerCreateInfoEXT m_DebugCreateInfo{};

		std::vector<const char*> m_Layers{};
		std::vector<const char*> m_Extensions{};
	};

	void destroyInstance(VkInstance p_Instance, VkDebugUtilsMessengerEXT p_DebugMessenger = VK_NULL_HANDLE);
}
