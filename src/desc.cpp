#include "desc.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "shader.hpp"

namespace vkp::desc
{
	namespace
	{
		constexpr SlangInt kUnboundedThreshold = 1 << 24;

		VkDescriptorType toDescriptorType(const slang::BindingType p_Type)
		{
			using B = slang::BindingType;
			const bool l_Mutable = (static_cast<uint32_t>(p_Type) & static_cast<uint32_t>(B::MutableFlag)) != 0;
			switch (static_cast<uint32_t>(p_Type) & static_cast<uint32_t>(B::BaseMask))
			{
			case static_cast<uint32_t>(B::Sampler): return VK_DESCRIPTOR_TYPE_SAMPLER;
			case static_cast<uint32_t>(B::Texture): return l_Mutable ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			case static_cast<uint32_t>(B::CombinedTextureSampler): return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			case static_cast<uint32_t>(B::ConstantBuffer):
			case static_cast<uint32_t>(B::ParameterBlock): return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			case static_cast<uint32_t>(B::TypedBuffer): return l_Mutable ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			case static_cast<uint32_t>(B::RawBuffer): return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			case static_cast<uint32_t>(B::RayTracingAccelerationStructure): return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			default: throw std::runtime_error("vkp::desc: unsupported reflection binding type");
			}
		}

		bool isNonDescriptorType(const slang::BindingType p_Type)
		{
			switch (p_Type)
			{
			case slang::BindingType::Unknown:
			case slang::BindingType::VaryingInput:
			case slang::BindingType::VaryingOutput:
			case slang::BindingType::ExistentialValue:
			case slang::BindingType::PushConstant:
				return true;
			default:
				return false;
			}
		}
	}

	std::vector<VkDescriptorSetLayout> createSetLayouts(const device::DeviceData& p_DeviceData, const shader::Shader<true>& p_Shader, const VkShaderStageFlags p_StageFlags)
	{
		slang::ProgramLayout* l_Program = p_Shader.layout();
		slang::VariableLayoutReflection* l_Global = l_Program->getGlobalParamsVarLayout();
		if (!l_Global)
		{
			return {};
		}
		slang::TypeLayoutReflection* l_GlobalLayout = l_Global->getTypeLayout();
		if (!l_GlobalLayout)
		{
			return {};
		}

		uint32_t l_SetCount = 0;
		for (SlangInt i = 0; i < l_GlobalLayout->getDescriptorSetCount(); ++i)
		{
			l_SetCount = std::max(l_SetCount, static_cast<uint32_t>(l_GlobalLayout->getDescriptorSetSpaceOffset(i)) + 1);
		}

		std::vector<VkDescriptorSetLayout> l_Created(l_SetCount, VK_NULL_HANDLE);
		for (SlangInt i = 0; i < l_GlobalLayout->getDescriptorSetCount(); ++i)
		{
			const uint32_t l_Space = static_cast<uint32_t>(l_GlobalLayout->getDescriptorSetSpaceOffset(i));
			std::vector<VkDescriptorSetLayoutBinding> l_Bindings;
			std::vector<VkDescriptorBindingFlags> l_Flags;

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
			l_Created[l_Space] = l_Layout;
		}

		return l_Created;
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
					if (isNonDescriptorType(l_Type))
					{
						continue;
					}
					const SlangInt l_Count = l_ParameterLayout->getDescriptorSetDescriptorRangeDescriptorCount(s, r);
					const bool l_Unbounded = l_Count < 0 || l_Count >= kUnboundedThreshold;
					const uint32_t l_BindingIndex = static_cast<uint32_t>(l_ParameterLayout->getDescriptorSetDescriptorRangeIndexOffset(s, r));

					l_Bindings.push_back({
						.name = l_Name ? l_Name : "",
						.set = l_Space + l_OffsetSpace,
						.binding = l_Binding + l_BindingIndex,
						.type = toDescriptorType(l_Type),
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

	namespace
	{
		void writeDescriptor(const device::DeviceData& p_DeviceData, const VkDescriptorSet p_Set, const Binding& p_Binding, const uint32_t p_ArrayIndex, const VkDescriptorType p_Type, const void* p_Info, const VkDescriptorImageInfo* p_ImageInfo, const VkDescriptorBufferInfo* p_BufferInfo)
		{
			VkWriteDescriptorSet l_Write{};
			l_Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			l_Write.pNext = nullptr;
			l_Write.dstSet = p_Set;
			l_Write.dstBinding = p_Binding.binding;
			l_Write.dstArrayElement = p_ArrayIndex;
			l_Write.descriptorCount = 1;
			l_Write.descriptorType = p_Type;
			l_Write.pImageInfo = p_ImageInfo;
			l_Write.pBufferInfo = p_BufferInfo;
			l_Write.pTexelBufferView = nullptr;
			p_DeviceData.deviceTable.vkUpdateDescriptorSets(p_DeviceData.device, 1, &l_Write, 0, nullptr);
			(void)p_Info;
		}
	}

	void writeImage(const device::DeviceData& p_DeviceData, const VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorImageInfo& p_Info, const uint32_t p_ArrayIndex)
	{
		writeDescriptor(p_DeviceData, p_Set, p_Binding, p_ArrayIndex, p_Binding.type, nullptr, &p_Info, nullptr);
	}

	void writeBuffer(const device::DeviceData& p_DeviceData, const VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorBufferInfo& p_Info, const uint32_t p_ArrayIndex)
	{
		writeDescriptor(p_DeviceData, p_Set, p_Binding, p_ArrayIndex, p_Binding.type, nullptr, nullptr, &p_Info);
	}

	void writeSampler(const device::DeviceData& p_DeviceData, const VkDescriptorSet p_Set, const Binding& p_Binding, const VkSampler p_Sampler, const uint32_t p_ArrayIndex)
	{
		const VkDescriptorImageInfo l_Info{
			.sampler = p_Sampler,
			.imageView = VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		writeDescriptor(p_DeviceData, p_Set, p_Binding, p_ArrayIndex, p_Binding.type, nullptr, &l_Info, nullptr);
	}
}
