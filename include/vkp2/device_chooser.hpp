#pragma once
#include <cassert>
#include <optional>
#include <vector>
#include <volk.h>

namespace vkp::chooser {
    template<typename T>
    concept DeviceEvaluation = requires(VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, const T* p_State) 
	{
        { T::evaluate(p_Device, p_Instance, p_Surface, p_State) } -> std::same_as<uint32_t>;
    };

    template<typename Func>
    struct CustomFilter 
	{
        Func filterFunc;

        static uint32_t evaluate(VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, const CustomFilter* p_State)
        {
            return p_State->filterFunc(p_Device, p_Instance, p_Surface);
        }
    };

    template<DeviceEvaluation... SubEvals>
    struct PresetGroup {
        std::tuple<SubEvals...> subPresets;

        explicit PresetGroup(SubEvals... p_Presets) : subPresets(std::move(p_Presets)...) {}

        static uint32_t evaluate(VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, const PresetGroup* p_State)
        {
            assert(p_State);

            uint32_t l_TotalGroupScore = 0;
            bool l_Incompatible = false;

            auto l_EvaluateIdx = [&]<size_t I>() 
        	{
                if (l_Incompatible) 
                    return;

                using CurrentPresetType = std::tuple_element_t<I, std::tuple<SubEvals...>>;

                const auto& l_PresetInstance = std::get<I>(p_State->subPresets);

                const uint32_t score = CurrentPresetType::evaluate(p_Device, p_Instance, p_Surface, &l_PresetInstance);

                if (score == 0) 
                {
                    l_Incompatible = true;
                }
                else 
                {
                    l_TotalGroupScore += score;
                }
            };

            [&] <size_t... Is>(std::index_sequence<Is...>) { (l_EvaluateIdx.template operator() < Is > (), ...); }(std::make_index_sequence<sizeof...(SubEvals)>{});

            if (l_Incompatible) 
                return 0;
            return l_TotalGroupScore;
        }
    };

    template<DeviceEvaluation... SubEvals>
    auto makeGroup(SubEvals&&... p_Presets) 
	{
        return PresetGroup<std::decay_t<SubEvals>...>(std::forward<SubEvals>(p_Presets)...);
    }

    struct UberQueue
    {
        static uint32_t evaluate(const VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, const UberQueue*)
        {
            assert(p_Surface.has_value());

			uint32_t l_QueueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(p_Device, &l_QueueFamilyCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilies(l_QueueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(p_Device, &l_QueueFamilyCount, queueFamilies.data());

            for (uint32_t i = 0; i < l_QueueFamilyCount; ++i)
            {
                const VkQueueFamilyProperties& queueFamily = queueFamilies[i];
                const bool l_HasGraphics = (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                const bool l_HasCompute = (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
                const bool l_HasTransfer = (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
                VkBool32 l_HasPresent;
                vkGetPhysicalDeviceSurfaceSupportKHR(p_Device, i, p_Surface.value(), &l_HasPresent);
				if (l_HasGraphics && l_HasCompute && l_HasTransfer && l_HasPresent)
				{
					return 1;
				}
            }

            return 0;
        }
    };

    struct LeanVulkan
    {
        static uint32_t evaluate(VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, const LeanVulkan*)
        {
			VkPhysicalDeviceVulkan13Features l_Features13{};
			l_Features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			VkPhysicalDeviceVulkan12Features l_Features12{};
			l_Features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			l_Features12.pNext = &l_Features13;
			VkPhysicalDeviceFeatures2 l_Features2{};
			l_Features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			l_Features2.pNext = &l_Features12;
			vkGetPhysicalDeviceFeatures2(p_Device, &l_Features2);
			bool l_Vk13Supported = l_Features13.dynamicRendering && l_Features13.synchronization2;
			bool l_Vk12Supported = l_Features12.timelineSemaphore && l_Features12.bufferDeviceAddress && l_Features12.descriptorIndexing;
			if (l_Vk13Supported && l_Vk12Supported)
				return 1;
			return 0;
        }
    };

    struct MeshShaders
    {
        bool taskShadersRequired = true;
        
        static uint32_t evaluate(const VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, const MeshShaders* p_State)
        {
			VkPhysicalDeviceMeshShaderFeaturesEXT l_MeshShaderFeatures{};
			l_MeshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
			VkPhysicalDeviceFeatures2 l_Features2{};
			l_Features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			l_Features2.pNext = &l_MeshShaderFeatures;
			vkGetPhysicalDeviceFeatures2(p_Device, &l_Features2);
			const bool l_TaskShader = l_MeshShaderFeatures.taskShader || !p_State->taskShadersRequired;
			const bool l_MeshShader = l_MeshShaderFeatures.meshShader;
			return l_TaskShader && l_MeshShader ? 1 : 0;
        }
    };

    struct BasicProperties
    {
		VkPhysicalDeviceType requiredType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
		uint64_t minimalRam = 0;
		uint32_t minVkVersion = VK_API_VERSION_1_0;

        static uint32_t evaluate(const VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, const BasicProperties* p_State)
        {
			VkPhysicalDeviceProperties l_Properties{};
			vkGetPhysicalDeviceProperties(p_Device, &l_Properties);
			const bool l_TypeOk = l_Properties.deviceType == p_State->requiredType;
            
        	uint64_t l_TotalRam = 0;
            VkPhysicalDeviceMemoryProperties l_MemoryProperties;
			vkGetPhysicalDeviceMemoryProperties(p_Device, &l_MemoryProperties);
			for (uint32_t i = 0; i < l_MemoryProperties.memoryHeapCount; ++i)
			{
				if ((l_MemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0)
				{
					l_TotalRam += l_MemoryProperties.memoryHeaps[i].size;
				}
			}
			const bool l_RamOk = l_TotalRam >= p_State->minimalRam;

			const bool l_VersionOk = l_Properties.apiVersion >= p_State->minVkVersion;
			return l_TypeOk && l_RamOk && l_VersionOk ? l_TotalRam : 0;
        }
    };

	inline auto LeanModern(const uint64_t p_MinimalRam = 0)
	{
        return makeGroup(
            BasicProperties{ .minimalRam = p_MinimalRam, .minVkVersion = VK_API_VERSION_1_3 },
            LeanVulkan{}
        );
	}

    template<DeviceEvaluation P>
	std::optional<VkPhysicalDevice> chooseBestPhysicalDevice(VkInstance l_Instance, std::optional<VkSurfaceKHR> l_Surface = std::nullopt, P* p_Preset = nullptr)
    {
		uint32_t l_DeviceCount = 0;
		vkEnumeratePhysicalDevices(l_Instance, &l_DeviceCount, nullptr);
		if (l_DeviceCount == 0)
			return std::nullopt;
        std::vector<VkPhysicalDevice> l_Devices(l_DeviceCount);
        vkEnumeratePhysicalDevices(l_Instance, &l_DeviceCount, l_Devices.data());

        uint32_t l_HighestScore = 0;
		VkPhysicalDevice l_BestDevice = VK_NULL_HANDLE;
		for (const VkPhysicalDevice& l_Device : l_Devices)
		{
			const uint32_t score = P::evaluate(l_Device, l_Instance, l_Surface, p_Preset);
			if (score > 0)
			{
				if (score > l_HighestScore)
				{
					l_HighestScore = score;
					l_BestDevice = l_Device;
				}
			}
		}
		return l_BestDevice != VK_NULL_HANDLE ? std::optional<VkPhysicalDevice>{l_BestDevice} : std::nullopt;
    }
}
