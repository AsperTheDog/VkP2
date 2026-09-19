#pragma once

#include <memory>

#include "vkp2/pipeline.hpp"
#include "vkp2/dyn/extra/vector.hpp"

namespace vkp::dyn
{
	template<typename Allocator = std::allocator<void>>
	struct PipelineStorage
	{
		using ProtoAllocator = Allocator;

		template<typename T>
		using VectorFor = Vector<T, typename std::allocator_traits<Allocator>::template rebind_alloc<T>>;

		using SpecializationEntries = VectorFor<VkSpecializationMapEntry>;
		using SpecializationData = VectorFor<uint8_t>;
		using Stage = pipeline::ShaderStage<SpecializationEntries, SpecializationData>;
		using Stages = VectorFor<Stage>;
		using DescriptorSetLayouts = VectorFor<VkDescriptorSetLayout>;
		using DescriptorBindings = VectorFor<VkDescriptorSetLayoutBinding>;
		using DescriptorBindingFlags = VectorFor<VkDescriptorBindingFlags>;
		using PushConstantRanges = VectorFor<VkPushConstantRange>;
		using ColorFormats = VectorFor<VkFormat>;
		using VertexBindings = VectorFor<VkVertexInputBindingDescription>;
		using VertexAttributes = VectorFor<VkVertexInputAttributeDescription>;
		using DynamicStates = VectorFor<VkDynamicState>;
		using ShaderStageInfos = VectorFor<VkPipelineShaderStageCreateInfo>;
		using SpecializationInfos = VectorFor<VkSpecializationInfo>;

		template<typename TContainer>
		[[nodiscard]] static TContainer make(const Allocator& p_Allocator)
		{
			using ElementAllocator = std::allocator_traits<Allocator>::template rebind_alloc<typename TContainer::value_type>;
			return TContainer(ElementAllocator(p_Allocator));
		}
	};

	template<typename Allocator = std::allocator<void>>
	using PipelineBuilder = pipeline::BasicPipelineBuilder<PipelineStorage<Allocator>>;

	template<typename Allocator = std::allocator<void>>
	using PipelineData = pipeline::BasicPipelineData<PipelineStorage<Allocator>>;

	template<typename Allocator>
	[[nodiscard]] PipelineBuilder<Allocator> makePipelineBuilder(const Allocator& p_Allocator)
	{
		return PipelineBuilder<Allocator>(p_Allocator);
	}
}
