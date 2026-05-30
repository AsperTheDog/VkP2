#pragma once
#include <cassert>
#include <optional>
#include <vector>
#include <volk.h>

namespace vkp::device {
    struct DeviceActivationContext {
        struct QueueInfo {
            uint32_t queueFamilyIndex;
            uint32_t count;
        };

        std::vector<const char*> extensions;
        std::vector<QueueInfo> queuesToCreate;

        VkPhysicalDeviceVulkan13Features features13{};
        VkPhysicalDeviceVulkan12Features features12{};
        VkPhysicalDeviceVulkan11Features features11{};
        VkPhysicalDeviceFeatures2 features{};

        VkDevice build(VkPhysicalDevice p_PhysicalDevice);

        template<typename T>
		void pNextAdd(T* p_Structure)
		{
			p_Structure->pNext = features13.pNext;
			features13.pNext = p_Structure;
		}
    };

    inline VkDevice DeviceActivationContext::build(VkPhysicalDevice p_PhysicalDevice)
    {
		VkDeviceCreateInfo l_CreateInfo{};
		l_CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		l_CreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		l_CreateInfo.ppEnabledExtensionNames = extensions.data();

		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features12.pNext = &features13;
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features11.pNext = &features12;
		features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        features.pNext = &features11;
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		l_CreateInfo.pNext = &features;

		std::vector<VkDeviceQueueCreateInfo> l_QueueCreateInfos(queuesToCreate.size());
		std::vector<std::vector<float>> l_QueuePriorities(queuesToCreate.size());
		for (size_t i = 0; i < queuesToCreate.size(); ++i)
		{
			l_QueuePriorities[i].resize(queuesToCreate[i].count, 1.0f);
			l_QueueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			l_QueueCreateInfos[i].queueFamilyIndex = queuesToCreate[i].queueFamilyIndex;
			l_QueueCreateInfos[i].queueCount = queuesToCreate[i].count;
			l_QueueCreateInfos[i].pQueuePriorities = l_QueuePriorities[i].data();
		}
        l_CreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queuesToCreate.size());
		l_CreateInfo.pQueueCreateInfos = l_QueueCreateInfos.data();

        VkDevice l_Device;
		vkCreateDevice(p_PhysicalDevice, &l_CreateInfo, nullptr, &l_Device);
		return l_Device;
    }

    template<typename T>
    concept DeviceEvaluation = requires(VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, T* p_State, DeviceActivationContext& p_Context)
    {
        { T::evaluate(p_Device, p_Instance, p_Surface, p_State) } -> std::same_as<uint32_t>;
        { T::activate(p_Context, p_State) } -> std::same_as<void>;
    };

    template<typename EvalFunc, typename ActFunc>
    struct CustomFilter 
	{
        EvalFunc filterFunc;
		ActFunc activateFunc;

        static uint32_t evaluate(VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, CustomFilter* p_State)
        {
            return p_State->filterFunc(p_Device, p_Instance, p_Surface);
        }

		static void activate(DeviceActivationContext& p_Context, CustomFilter* p_State)
		{
			p_State->activateFunc(p_Context);
		}
    };

    template<DeviceEvaluation... SubEvals>
    struct PresetGroup {
        std::tuple<SubEvals...> subPresets;

        explicit PresetGroup(SubEvals... p_Presets) : subPresets(std::move(p_Presets)...) {}

        static uint32_t evaluate(VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, PresetGroup* p_State)
        {
            assert(p_State);

            uint32_t l_TotalGroupScore = 0;
            bool l_Incompatible = false;

            auto l_EvaluateIdx = [&]<size_t I>() 
        	{
                if (l_Incompatible) 
                    return;

                using CurrentPresetType = std::tuple_element_t<I, std::tuple<SubEvals...>>;

                auto& l_PresetInstance = std::get<I>(p_State->subPresets);

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

        static void activate(DeviceActivationContext& p_Context, PresetGroup* p_State)
        {
            assert(p_State);
            auto l_ActivateIdx = [&]<size_t I>()
            {
                using CurrentPresetType = std::tuple_element_t<I, std::tuple<SubEvals...>>;
                auto& l_PresetInstance = std::get<I>(p_State->subPresets);
                CurrentPresetType::activate(p_Context, &l_PresetInstance);
            };
            [&] <size_t... Is>(std::index_sequence<Is...>) { (l_ActivateIdx.template operator() < Is > (), ...); }(std::make_index_sequence<sizeof...(SubEvals)>{});
        }
    };

    template<DeviceEvaluation... SubEvals>
    auto makeGroup(SubEvals&&... p_Presets) 
	{
        return PresetGroup<std::decay_t<SubEvals>...>(std::forward<SubEvals>(p_Presets)...);
    }

    struct UberQueueFamily
    {
        bool dedicatedTransfer;

        explicit UberQueueFamily(const bool p_DedicatedTransfer = false) : dedicatedTransfer(p_DedicatedTransfer) {}

        static uint32_t evaluate(const VkPhysicalDevice p_Device, VkInstance p_Instance, std::optional<VkSurfaceKHR> p_Surface, UberQueueFamily* p_State)
        {
            assert(p_State);
            assert(p_Surface.has_value());

			uint32_t l_QueueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(p_Device, &l_QueueFamilyCount, nullptr);
			std::vector<VkQueueFamilyProperties> l_QueueFamilies(l_QueueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(p_Device, &l_QueueFamilyCount, l_QueueFamilies.data());

            for (uint32_t i = 0; i < l_QueueFamilyCount; ++i)
            {
                const VkQueueFamilyProperties& l_QueueFamily = l_QueueFamilies[i];
                const bool l_HasGraphics = (l_QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                const bool l_HasCompute = (l_QueueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
                const bool l_HasTransfer = (l_QueueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
                const bool l_HasDedicatedTransfer = (l_HasTransfer && l_QueueFamily.queueCount > 1) || !p_State->dedicatedTransfer;
                VkBool32 l_HasPresent;
                vkGetPhysicalDeviceSurfaceSupportKHR(p_Device, i, p_Surface.value(), &l_HasPresent);
				if (l_HasGraphics && l_HasCompute && l_HasTransfer && l_HasPresent && l_HasDedicatedTransfer)
				{
					p_State->m_UberQueueInfo = i;
					return 1;
				}
            }

            return 0;
        }

		static void activate(DeviceActivationContext& p_Context, const UberQueueFamily* p_State)
		{
			p_Context.queuesToCreate.emplace_back(p_State->m_UberQueueInfo, p_State->dedicatedTransfer ? 2 : 1);
		}

    private:
        uint32_t m_UberQueueInfo = UINT32_MAX;
    };

    struct LeanVulkan
    {
        static uint32_t evaluate(VkPhysicalDevice p_Device, VkInstance, std::optional<VkSurfaceKHR>, LeanVulkan*)
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

        static void activate(DeviceActivationContext& p_Context, LeanVulkan*)
        {
			p_Context.features13.dynamicRendering = VK_TRUE;
			p_Context.features13.synchronization2 = VK_TRUE;
			p_Context.features12.timelineSemaphore = VK_TRUE;
			p_Context.features12.bufferDeviceAddress = VK_TRUE;
			p_Context.features12.descriptorIndexing = VK_TRUE;
        }
    };

    struct MeshShaders
    {
        bool taskShadersRequired = true;
        
        static uint32_t evaluate(const VkPhysicalDevice p_Device, VkInstance, std::optional<VkSurfaceKHR>, MeshShaders* p_State)
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

        static void activate(DeviceActivationContext& p_Context, MeshShaders* p_State)
        {
			p_State->m_MeshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
			p_State->m_MeshShaderFeatures.meshShader = VK_TRUE;
			if (p_State->taskShadersRequired)
				p_State->m_MeshShaderFeatures.taskShader = VK_TRUE;

            p_Context.pNextAdd(&p_State->m_MeshShaderFeatures);
        }

    private:
		VkPhysicalDeviceMeshShaderFeaturesEXT m_MeshShaderFeatures{};
    };

    struct BasicProperties
    {
		VkPhysicalDeviceType requiredType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
		uint64_t minimalRam = 0;
		uint32_t minVkVersion = VK_API_VERSION_1_0;

        static uint32_t evaluate(const VkPhysicalDevice p_Device, VkInstance, std::optional<VkSurfaceKHR>, BasicProperties* p_State)
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

        static void activate(DeviceActivationContext& p_Context, BasicProperties* p_State) {}
    };

	inline auto LeanModern(const uint64_t p_MinimalRam = 0)
	{
        return makeGroup(
            BasicProperties{ .minimalRam = p_MinimalRam, .minVkVersion = VK_API_VERSION_1_3 },
            LeanVulkan{},
			UberQueueFamily{ true }
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

    template<DeviceEvaluation P>
	VkDevice buildFromEval(const VkPhysicalDevice p_Device, P* p_Preset)
	{
		DeviceActivationContext l_Context;
		p_Preset->activate(l_Context, p_Preset);
		return l_Context.build(p_Device);
	}
}
