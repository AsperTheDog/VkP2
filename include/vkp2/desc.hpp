#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "device.hpp"

namespace vkp::shader
{
	template<bool Reflect>
	class Shader;
}

namespace vkp::desc
{
	struct Binding
	{
		std::string name;
		uint32_t set;
		uint32_t binding;
		VkDescriptorType type;
		uint32_t count;
		bool unbounded;
	};

	std::vector<VkDescriptorSetLayout> createSetLayouts(const device::DeviceData& p_DeviceData, const shader::Shader<true>& p_Shader, VkShaderStageFlags p_StageFlags);
	std::vector<Binding> reflectBindings(const shader::Shader<true>& p_Shader);

	struct PoolConfig
	{
		std::vector<VkDescriptorPoolSize> sizes;
		uint32_t maxSets;
		VkDescriptorPoolCreateFlags flags = 0;
	};

	PoolConfig makePoolConfig(std::span<const Binding> p_Bindings, uint32_t p_MaxSets, VkDescriptorPoolCreateFlags p_Flags = 0);
	VkDescriptorPool createPool(const device::DeviceData& p_DeviceData, const PoolConfig& p_Config);
	VkDescriptorSet allocateSet(const device::DeviceData& p_DeviceData, VkDescriptorPool p_Pool, VkDescriptorSetLayout p_Layout);

	void writeImage(const device::DeviceData& p_DeviceData, VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorImageInfo& p_Info, uint32_t p_ArrayIndex = 0);
	void writeBuffer(const device::DeviceData& p_DeviceData, VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorBufferInfo& p_Info, uint32_t p_ArrayIndex = 0);
	void writeSampler(const device::DeviceData& p_DeviceData, VkDescriptorSet p_Set, const Binding& p_Binding, VkSampler p_Sampler, uint32_t p_ArrayIndex = 0);
}
