#include "instance.hpp"

#include <volk.h>

#include "base.hpp"

static VkResult createDebugUtilsMessengerEXT(const VkInstance p_Instance, const VkDebugUtilsMessengerCreateInfoEXT* p_CreateInfo, const VkAllocationCallbacks* p_Allocator, VkDebugUtilsMessengerEXT* p_DebugMessenger)
{
	const auto l_Func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(p_Instance, "vkCreateDebugUtilsMessengerEXT"));
	if (l_Func != nullptr)
	{
		return l_Func(p_Instance, p_CreateInfo, p_Allocator, p_DebugMessenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void destroyDebugUtilsMessengerEXT(const VkInstance p_Instance, const VkDebugUtilsMessengerEXT p_DebugMessenger, const VkAllocationCallbacks* p_Allocator)
{
	const auto l_Func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(p_Instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (l_Func != nullptr)
	{
		l_Func(p_Instance, p_DebugMessenger, p_Allocator);
	}
}

namespace vkp
{

	InstanceBuilder::InstanceBuilder(const uint32_t p_ApiVersion)
	{
		m_appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		m_appInfo.pApplicationName = "Vulkan App";
		m_appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		m_appInfo.pEngineName = "No Engine";
		m_appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		m_appInfo.apiVersion = p_ApiVersion;

		m_createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		m_createInfo.pApplicationInfo = &m_appInfo;
	}

	InstanceBuilder& InstanceBuilder::setAppInfo(const std::string_view p_ApplicationName, const uint32_t p_ApplicationVersion, const std::string_view p_EngineName, const uint32_t p_EngineVersion)
	{
		m_appInfo.pApplicationName = p_ApplicationName.data();
		m_appInfo.applicationVersion = p_ApplicationVersion;
		m_appInfo.pEngineName = p_EngineName.data();
		m_appInfo.engineVersion = p_EngineVersion;
		return *this;
	}

	InstanceBuilder& InstanceBuilder::enableValidationLayers(const DebugCallback p_Callback, const VkDebugUtilsMessageSeverityFlagsEXT p_Severity)
	{
		addLayer("VK_LAYER_KHRONOS_validation");
		if (p_Callback)
		{
			m_DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			m_DebugCreateInfo.messageSeverity = p_Severity;
			m_DebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			m_DebugCreateInfo.pfnUserCallback = p_Callback;
			m_DebugCreateInfo.pUserData = nullptr;
			m_createInfo.pNext = &m_DebugCreateInfo;
			addExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		return *this;
	}

	InstanceBuilder& InstanceBuilder::addLayer(const char* p_Layer)
	{
		m_Layers.push_back(p_Layer);
		m_createInfo.enabledLayerCount = static_cast<uint32_t>(m_Layers.size());
		m_createInfo.ppEnabledLayerNames = m_Layers.data();
		return *this;
	}

	InstanceBuilder& InstanceBuilder::addLayers(const std::span<const char* const> p_Layers)
	{
		for (const char* l_Layer : p_Layers)
			addLayer(l_Layer);
		return *this;
	}

	InstanceBuilder& InstanceBuilder::addExtension(const char* p_Extension)
	{
		m_Extensions.push_back(p_Extension);
		m_createInfo.enabledExtensionCount = static_cast<uint32_t>(m_Extensions.size());
		m_createInfo.ppEnabledExtensionNames = m_Extensions.data();
		return *this;
	}

	InstanceBuilder& InstanceBuilder::addExtensions(std::span<const char* const> p_Extensions)
	{
		for (const char* l_Extension : p_Extensions)
			addExtension(l_Extension);
		return *this;
	}

	InstanceBuilder::ReturnData InstanceBuilder::build() const
	{
		ReturnData l_Data{};

		VULKAN_TRY(vkCreateInstance(&m_createInfo, nullptr, &l_Data.instance));

		volkLoadInstance(l_Data.instance);

		if (m_DebugCreateInfo.pfnUserCallback)
		{
			VULKAN_TRY(createDebugUtilsMessengerEXT(l_Data.instance, &m_DebugCreateInfo, nullptr, &l_Data.debugMessenger));
		}

		return l_Data;
	}

	void destroyInstance(const VkInstance p_Instance, const VkDebugUtilsMessengerEXT p_DebugMessenger)
	{
		if (p_DebugMessenger != VK_NULL_HANDLE)
		{
			destroyDebugUtilsMessengerEXT(p_Instance, p_DebugMessenger, nullptr);
		}
		vkDestroyInstance(p_Instance, nullptr);
	}
}
