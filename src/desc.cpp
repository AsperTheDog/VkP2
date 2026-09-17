#include "desc.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "shader.hpp"

namespace vkp::desc
{
	SetLayouts createSetLayouts(const device::DeviceData& p_DeviceData, const shader::Shader<true>& p_Shader, const VkShaderStageFlags p_StageFlags)
	{
		SetLayouts l_Layouts;
		slang::ProgramLayout* l_Program = p_Shader.layout();
		if (l_Program)
		{
			detail::createSetLayouts<detail::VectorStorage>(p_DeviceData, l_Program, p_StageFlags, l_Layouts.handles);
		}
		return l_Layouts;
	}

	void SetLayouts::destroy(const device::DeviceData& p_DeviceData)
	{
		for (VkDescriptorSetLayout& l_Layout : handles)
		{
			if (l_Layout != VK_NULL_HANDLE)
			{
				p_DeviceData.deviceTable.vkDestroyDescriptorSetLayout(p_DeviceData.device, l_Layout, nullptr);
				l_Layout = VK_NULL_HANDLE;
			}
		}
		handles.clear();
	}

	std::vector<Binding> reflectBindings(const shader::Shader<true>& p_Shader)
	{
		std::vector<Binding> l_Bindings;
		slang::ProgramLayout* l_Program = p_Shader.layout();
		for (SlangUInt i = 0; i < l_Program->getParameterCount(); ++i)
		{
			slang::VariableLayoutReflection* l_Parameter = l_Program->getParameterByIndex(static_cast<unsigned>(i));
			slang::TypeLayoutReflection* l_ParameterLayout = l_Parameter->getTypeLayout();
			if (!l_ParameterLayout)
			{
				continue;
			}
			const char* l_Name = l_Parameter->getName();
			const uint32_t l_Space = l_Parameter->getBindingSpace();
			const uint32_t l_Binding = l_Parameter->getBindingIndex();

			for (SlangInt s = 0; s < l_ParameterLayout->getDescriptorSetCount(); ++s)
			{
				const uint32_t l_OffsetSpace = static_cast<uint32_t>(l_ParameterLayout->getDescriptorSetSpaceOffset(s));
				for (SlangInt r = 0; r < l_ParameterLayout->getDescriptorSetDescriptorRangeCount(s); ++r)
				{
					const slang::BindingType l_Type = l_ParameterLayout->getDescriptorSetDescriptorRangeType(s, r);
					if (detail::isNonDescriptorType(l_Type))
					{
						continue;
					}
					const SlangInt l_Count = l_ParameterLayout->getDescriptorSetDescriptorRangeDescriptorCount(s, r);
					const bool l_Unbounded = l_Count < 0 || l_Count >= detail::kUnboundedThreshold;
					const uint32_t l_BindingIndex = static_cast<uint32_t>(l_ParameterLayout->getDescriptorSetDescriptorRangeIndexOffset(s, r));

					l_Bindings.push_back({
						.name = l_Name ? l_Name : "",
						.set = l_Space + l_OffsetSpace,
						.binding = l_Binding + l_BindingIndex,
						.type = detail::toDescriptorType(l_Type),
						.count = l_Unbounded ? 1u : static_cast<uint32_t>(l_Count),
						.unbounded = l_Unbounded,
					});
				}
			}
		}
		return l_Bindings;
	}

	PoolConfig makePoolConfig(const std::span<const Binding> p_Bindings, const uint32_t p_MaxSets, const VkDescriptorPoolCreateFlags p_Flags)
	{
		constexpr uint32_t kUnboundedCapacity = 1024;
		std::vector<std::pair<VkDescriptorType, uint32_t>> l_Accumulated;
		for (const Binding& l_Binding : p_Bindings)
		{
			const uint32_t l_Count = l_Binding.unbounded ? kUnboundedCapacity : l_Binding.count;
			const auto l_It = std::find_if(l_Accumulated.begin(), l_Accumulated.end(), [&](const auto& p_Pair) { return p_Pair.first == l_Binding.type; });
			if (l_It != l_Accumulated.end())
			{
				l_It->second += l_Count;
			}
			else
			{
				l_Accumulated.emplace_back(l_Binding.type, l_Count);
			}
		}

		PoolConfig l_Config;
		l_Config.maxSets = p_MaxSets;
		l_Config.flags = p_Flags;
		for (const auto& l_Entry : l_Accumulated)
		{
			l_Config.sizes.push_back({ .type = l_Entry.first, .descriptorCount = l_Entry.second * p_MaxSets });
		}
		return l_Config;
	}

	VkDescriptorPool createPool(const device::DeviceData& p_DeviceData, const PoolConfig& p_Config)
	{
		const VkDescriptorPoolCreateInfo l_Info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = p_Config.flags,
			.maxSets = p_Config.maxSets,
			.poolSizeCount = static_cast<uint32_t>(p_Config.sizes.size()),
			.pPoolSizes = p_Config.sizes.data(),
		};
		VkDescriptorPool l_Pool = VK_NULL_HANDLE;
		VULKAN_TRY(p_DeviceData.deviceTable.vkCreateDescriptorPool(p_DeviceData.device, &l_Info, nullptr, &l_Pool));
		return l_Pool;
	}

	void destroyPool(const device::DeviceData& p_DeviceData, VkDescriptorPool& p_Pool)
	{
		if (p_Pool != VK_NULL_HANDLE)
		{
			p_DeviceData.deviceTable.vkDestroyDescriptorPool(p_DeviceData.device, p_Pool, nullptr);
			p_Pool = VK_NULL_HANDLE;
		}
	}

	VkDescriptorSet allocateSet(const device::DeviceData& p_DeviceData, const VkDescriptorPool p_Pool, const VkDescriptorSetLayout p_Layout)
	{
		const VkDescriptorSetAllocateInfo l_Info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = p_Pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &p_Layout,
		};
		VkDescriptorSet l_Set = VK_NULL_HANDLE;
		VULKAN_TRY(p_DeviceData.deviceTable.vkAllocateDescriptorSets(p_DeviceData.device, &l_Info, &l_Set));
		return l_Set;
	}

	WriteBatch& WriteBatch::image(const VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorImageInfo& p_Info, const uint32_t p_ArrayIndex)
	{
		m_ImageInfos.push_back(p_Info);

		VkWriteDescriptorSet l_Write{};
		l_Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		l_Write.pNext = nullptr;
		l_Write.dstSet = p_Set;
		l_Write.dstBinding = p_Binding.binding;
		l_Write.dstArrayElement = p_ArrayIndex;
		l_Write.descriptorCount = 1;
		l_Write.descriptorType = p_Binding.type;
		l_Write.pImageInfo = &m_ImageInfos.back();
		m_Writes.push_back(l_Write);
		return *this;
	}

	WriteBatch& WriteBatch::buffer(const VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorBufferInfo& p_Info, const uint32_t p_ArrayIndex)
	{
		m_BufferInfos.push_back(p_Info);

		VkWriteDescriptorSet l_Write{};
		l_Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		l_Write.pNext = nullptr;
		l_Write.dstSet = p_Set;
		l_Write.dstBinding = p_Binding.binding;
		l_Write.dstArrayElement = p_ArrayIndex;
		l_Write.descriptorCount = 1;
		l_Write.descriptorType = p_Binding.type;
		l_Write.pBufferInfo = &m_BufferInfos.back();
		m_Writes.push_back(l_Write);
		return *this;
	}

	WriteBatch& WriteBatch::sampler(const VkDescriptorSet p_Set, const Binding& p_Binding, const VkSampler p_Sampler, const uint32_t p_ArrayIndex)
	{
		const VkDescriptorImageInfo l_Info{
			.sampler = p_Sampler,
			.imageView = VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		return image(p_Set, p_Binding, l_Info, p_ArrayIndex);
	}

	void WriteBatch::flush(const device::DeviceData& p_DeviceData)
	{
		if (!m_Writes.empty())
		{
			p_DeviceData.deviceTable.vkUpdateDescriptorSets(p_DeviceData.device, static_cast<uint32_t>(m_Writes.size()), m_Writes.data(), 0, nullptr);
		}

		m_Writes.clear();
		m_ImageInfos.clear();
		m_BufferInfos.clear();
	}

	void writeImage(const device::DeviceData& p_DeviceData, const VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorImageInfo& p_Info, const uint32_t p_ArrayIndex)
	{
		WriteBatch{}.image(p_Set, p_Binding, p_Info, p_ArrayIndex).flush(p_DeviceData);
	}

	void writeBuffer(const device::DeviceData& p_DeviceData, const VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorBufferInfo& p_Info, const uint32_t p_ArrayIndex)
	{
		WriteBatch{}.buffer(p_Set, p_Binding, p_Info, p_ArrayIndex).flush(p_DeviceData);
	}

	void writeSampler(const device::DeviceData& p_DeviceData, const VkDescriptorSet p_Set, const Binding& p_Binding, const VkSampler p_Sampler, const uint32_t p_ArrayIndex)
	{
		WriteBatch{}.sampler(p_Set, p_Binding, p_Sampler, p_ArrayIndex).flush(p_DeviceData);
	}
}
