#pragma once

#include <algorithm>
#include <stdexcept>

namespace vkp::desc::detail
{
	template<StoragePolicy TStorage>
	void createSetLayouts(const device::DeviceData& p_DeviceData, slang::ProgramLayout* p_Program, const VkShaderStageFlags p_StageFlags, typename TStorage::Layouts& p_Out)
	{
		slang::VariableLayoutReflection* l_Global = p_Program->getGlobalParamsVarLayout();
		if (!l_Global)
		{
			return;
		}
		slang::TypeLayoutReflection* l_GlobalLayout = l_Global->getTypeLayout();
		if (!l_GlobalLayout)
		{
			return;
		}

		uint32_t l_SetCount = 0;
		for (SlangInt i = 0; i < l_GlobalLayout->getDescriptorSetCount(); ++i)
		{
			l_SetCount = std::max(l_SetCount, static_cast<uint32_t>(l_GlobalLayout->getDescriptorSetSpaceOffset(i)) + 1);
		}

		p_Out.resize(l_SetCount, VK_NULL_HANDLE);
		for (SlangInt i = 0; i < l_GlobalLayout->getDescriptorSetCount(); ++i)
		{
			const uint32_t l_Space = static_cast<uint32_t>(l_GlobalLayout->getDescriptorSetSpaceOffset(i));
			typename TStorage::Bindings l_Bindings{};
			typename TStorage::Flags l_Flags{};

			SlangInt l_UnboundedIndex = -1;
			SlangInt l_UnboundedCount = 0;
			for (SlangInt r = 0; r < l_GlobalLayout->getDescriptorSetDescriptorRangeCount(i); ++r)
			{
				if (isNonDescriptorType(l_GlobalLayout->getDescriptorSetDescriptorRangeType(i, r)))
				{
					continue;
				}
				const SlangInt l_Count = l_GlobalLayout->getDescriptorSetDescriptorRangeDescriptorCount(i, r);
				if (l_Count < 0 || l_Count >= kUnboundedThreshold)
				{
					++l_UnboundedCount;
					l_UnboundedIndex = l_GlobalLayout->getDescriptorSetDescriptorRangeIndexOffset(i, r);
				}
			}
			if (l_UnboundedCount > 1 || (l_UnboundedCount == 1 && l_UnboundedIndex != l_GlobalLayout->getDescriptorSetDescriptorRangeIndexOffset(i, l_GlobalLayout->getDescriptorSetDescriptorRangeCount(i) - 1)))
			{
				throw std::runtime_error("vkp::desc: multiple unbounded descriptor arrays in one set are not supported by Vulkan; place each unbounded array in its own descriptor space");
			}

			for (SlangInt r = 0; r < l_GlobalLayout->getDescriptorSetDescriptorRangeCount(i); ++r)
			{
				const slang::BindingType l_Type = l_GlobalLayout->getDescriptorSetDescriptorRangeType(i, r);
				if (isNonDescriptorType(l_Type))
				{
					continue;
				}
				const SlangInt l_Count = l_GlobalLayout->getDescriptorSetDescriptorRangeDescriptorCount(i, r);
				const bool l_Unbounded = l_Count < 0 || l_Count >= kUnboundedThreshold;

				VkDescriptorSetLayoutBinding l_Binding{};
				l_Binding.binding = static_cast<uint32_t>(l_GlobalLayout->getDescriptorSetDescriptorRangeIndexOffset(i, r));
				l_Binding.descriptorType = toDescriptorType(l_Type);
				l_Binding.descriptorCount = l_Unbounded ? 1u : static_cast<uint32_t>(l_Count);
				l_Binding.stageFlags = p_StageFlags;

				VkDescriptorBindingFlags l_Flag = 0;
				if (l_Unbounded || l_Count > 256)
				{
					l_Flag |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
				}
				if (l_Unbounded)
				{
					l_Flag |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
				}

				l_Bindings.push_back(l_Binding);
				l_Flags.push_back(l_Flag);
			}

			const VkDescriptorSetLayoutBindingFlagsCreateInfo l_FlagInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
				.pNext = nullptr,
				.bindingCount = static_cast<uint32_t>(l_Flags.size()),
				.pBindingFlags = l_Flags.data(),
			};

			const VkDescriptorSetLayoutCreateInfo l_CreateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = l_Bindings.empty() ? nullptr : &l_FlagInfo,
				.flags = 0,
				.bindingCount = static_cast<uint32_t>(l_Bindings.size()),
				.pBindings = l_Bindings.data(),
			};

			VkDescriptorSetLayout l_Layout = VK_NULL_HANDLE;
			VULKAN_TRY(p_DeviceData.deviceTable.vkCreateDescriptorSetLayout(p_DeviceData.device, &l_CreateInfo, nullptr, &l_Layout));
			p_Out[l_Space] = l_Layout;
		}
	}
}
