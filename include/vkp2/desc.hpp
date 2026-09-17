#pragma once

#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <vector>

#include <slang/slang.h>

#include "concepts.hpp"
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

	namespace detail
	{
		inline constexpr SlangInt kUnboundedThreshold = 1 << 24;

		[[nodiscard]] inline VkDescriptorType toDescriptorType(const slang::BindingType p_Type)
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

		[[nodiscard]] inline bool isNonDescriptorType(const slang::BindingType p_Type)
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

		template<typename T>
		concept StoragePolicy = requires
		{
			typename T::Layouts;
			typename T::Bindings;
			typename T::Flags;
		}
		&& Sequence<typename T::Layouts>
		&& Sequence<typename T::Bindings>
		&& Sequence<typename T::Flags>;

		template<Sequence TLayout, Sequence TBinding, Sequence TFlag>
		struct Storage
		{
			using Layouts = TLayout;
			using Bindings = TBinding;
			using Flags = TFlag;
		};

		using VectorStorage = Storage<std::vector<VkDescriptorSetLayout>, std::vector<VkDescriptorSetLayoutBinding>, std::vector<VkDescriptorBindingFlags>>;

		template<StoragePolicy TStorage>
		void createSetLayouts(const device::DeviceData& p_DeviceData, slang::ProgramLayout* p_Program, VkShaderStageFlags p_StageFlags, typename TStorage::Layouts& p_Out);
	}

	struct SetLayouts
	{
		std::vector<VkDescriptorSetLayout> handles;

		void destroy(const device::DeviceData& p_DeviceData);
	};

	[[nodiscard]] SetLayouts createSetLayouts(const device::DeviceData& p_DeviceData, const shader::Shader<true>& p_Shader, VkShaderStageFlags p_StageFlags);
	[[nodiscard]] std::vector<Binding> reflectBindings(const shader::Shader<true>& p_Shader);

	struct PoolConfig
	{
		std::vector<VkDescriptorPoolSize> sizes;
		uint32_t maxSets;
		VkDescriptorPoolCreateFlags flags = 0;
	};

	PoolConfig makePoolConfig(std::span<const Binding> p_Bindings, uint32_t p_MaxSets, VkDescriptorPoolCreateFlags p_Flags = 0);
	VkDescriptorPool createPool(const device::DeviceData& p_DeviceData, const PoolConfig& p_Config);
	void destroyPool(const device::DeviceData& p_DeviceData, VkDescriptorPool& p_Pool);
	VkDescriptorSet allocateSet(const device::DeviceData& p_DeviceData, VkDescriptorPool p_Pool, VkDescriptorSetLayout p_Layout);

	class WriteBatch
	{
	public:
		WriteBatch& image(VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorImageInfo& p_Info, uint32_t p_ArrayIndex = 0);
		WriteBatch& buffer(VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorBufferInfo& p_Info, uint32_t p_ArrayIndex = 0);
		WriteBatch& sampler(VkDescriptorSet p_Set, const Binding& p_Binding, VkSampler p_Sampler, uint32_t p_ArrayIndex = 0);

		void flush(const device::DeviceData& p_DeviceData);

		[[nodiscard]] bool empty() const { return m_Writes.empty(); }

	private:
		std::vector<VkWriteDescriptorSet> m_Writes;
		std::deque<VkDescriptorImageInfo> m_ImageInfos;
		std::deque<VkDescriptorBufferInfo> m_BufferInfos;
	};

	void writeImage(const device::DeviceData& p_DeviceData, VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorImageInfo& p_Info, uint32_t p_ArrayIndex = 0);
	void writeBuffer(const device::DeviceData& p_DeviceData, VkDescriptorSet p_Set, const Binding& p_Binding, const VkDescriptorBufferInfo& p_Info, uint32_t p_ArrayIndex = 0);
	void writeSampler(const device::DeviceData& p_DeviceData, VkDescriptorSet p_Set, const Binding& p_Binding, VkSampler p_Sampler, uint32_t p_ArrayIndex = 0);
}

#include "inline/desc.inl"
