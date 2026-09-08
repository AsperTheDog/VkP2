#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "device.hpp"
#include "extra/small_vector.hpp"

namespace vkp::shader
{
	template<bool Reflect>
	class Shader;
}

namespace vkp::pipeline
{
	template<typename Allocator, typename T, size_t N>
	using Vec = SmallVector<T, N, typename std::allocator_traits<Allocator>::template rebind_alloc<T>>;

	template<typename Allocator = std::allocator<void>>
	struct BasicPipelineData
	{
		BasicPipelineData() = default;

		explicit BasicPipelineData(const Allocator& p_Allocator)
			: descriptorSetLayouts(p_Allocator)
		{
		}

		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		Vec<Allocator, VkDescriptorSetLayout, 8> descriptorSetLayouts;
	};

	using PipelineData = BasicPipelineData<>;

	template<typename Allocator = std::allocator<void>>
	class BasicPipelineBuilder
	{
	public:
		explicit BasicPipelineBuilder(const Allocator& p_Allocator = Allocator());

		BasicPipelineBuilder& addShaderStage(VkShaderModule p_Module, VkShaderStageFlagBits p_Stage, std::string_view p_EntryPointName = "main");
		BasicPipelineBuilder& addShaderStage(VkShaderModule p_Module, VkShaderStageFlagBits p_Stage, std::string_view p_EntryPointName, std::span<const VkSpecializationMapEntry> p_SpecializationMap, std::span<const uint8_t> p_SpecializationData);
		BasicPipelineBuilder& addDescriptorSetLayout(VkDescriptorSetLayout p_Layout);
		BasicPipelineBuilder& addPushConstantRange(VkPushConstantRange p_Range);
		BasicPipelineBuilder& setPipelineLayout(VkPipelineLayout p_Layout);
		BasicPipelineBuilder& useReflection(const shader::Shader<true>& p_Shader);

		BasicPipelineBuilder& setPipelineCacheFolder(std::filesystem::path p_Folder);
		BasicPipelineBuilder& setRenderPass(VkRenderPass p_RenderPass, uint32_t p_Subpass = 0);
		BasicPipelineBuilder& setColorFormats(std::span<const VkFormat> p_Formats);
		BasicPipelineBuilder& setDepthFormat(VkFormat p_Format);
		BasicPipelineBuilder& setVertexInput(std::span<const VkVertexInputBindingDescription> p_Bindings, std::span<const VkVertexInputAttributeDescription> p_Attributes);
		BasicPipelineBuilder& setTopology(VkPrimitiveTopology p_Topology);
		BasicPipelineBuilder& setPolygonMode(VkPolygonMode p_Mode);
		BasicPipelineBuilder& setCullMode(VkCullModeFlags p_CullMode);
		BasicPipelineBuilder& setFrontFace(VkFrontFace p_FrontFace);
		BasicPipelineBuilder& setSampleCount(VkSampleCountFlagBits p_SampleCount);
		BasicPipelineBuilder& setDepthTest(bool p_Test, bool p_Write = true);
		BasicPipelineBuilder& setBlending(bool p_Enable);
		BasicPipelineBuilder& setDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& p_State);
		BasicPipelineBuilder& setColorBlendState(const VkPipelineColorBlendStateCreateInfo& p_State);
		BasicPipelineBuilder& setMultisampleState(const VkPipelineMultisampleStateCreateInfo& p_State);
		BasicPipelineBuilder& setRasterizationState(const VkPipelineRasterizationStateCreateInfo& p_State);
		BasicPipelineBuilder& setPatchControlPoints(uint32_t p_Count);
		BasicPipelineBuilder& setFlags(VkPipelineCreateFlags p_Flags);
		BasicPipelineBuilder& setDynamicDepth(bool p_Test, bool p_Write);
		BasicPipelineBuilder& setDynamicCull();
		BasicPipelineBuilder& setDynamicRasterizerDiscard();
		BasicPipelineBuilder& setDynamicDepthBias();
		BasicPipelineBuilder& setDynamicBlendConstants();
		BasicPipelineBuilder& addDynamicState(VkDynamicState p_State);

		[[nodiscard]] BasicPipelineData<Allocator> buildGraphics(const device::DeviceData& p_DeviceData, VkPipelineCache p_PipelineCache = VK_NULL_HANDLE);
		[[nodiscard]] BasicPipelineData<Allocator> buildCompute(const device::DeviceData& p_DeviceData, VkPipelineCache p_PipelineCache = VK_NULL_HANDLE);

	private:
		struct Stage
		{
			VkShaderModule module;
			VkShaderStageFlagBits stage;
			std::string entryPointName;
			Vec<Allocator, VkSpecializationMapEntry, 8> specializationEntries;
			Vec<Allocator, uint8_t, 64> specializationData;
			bool hasSpecialization = false;

			Stage(const VkShaderModule p_Module, const VkShaderStageFlagBits p_Stage, std::string p_EntryPointName, const Allocator& p_Allocator)
				: module(p_Module), stage(p_Stage), entryPointName(std::move(p_EntryPointName)), specializationEntries(p_Allocator), specializationData(p_Allocator) {}
		};

		VkShaderStageFlags stageMask() const;
		void generateVertexInput();
		Vec<Allocator, VkDescriptorSetLayout, 8> createDescriptorSetLayouts(const device::DeviceData& p_DeviceData) const;

		Allocator m_Allocator;
		Vec<Allocator, Stage, 8> m_Stages;
		Vec<Allocator, VkDescriptorSetLayout, 8> m_DescriptorSetLayouts;
		Vec<Allocator, VkPushConstantRange, 4> m_PushConstantRanges;
		std::filesystem::path m_CacheFolder;

		VkPipelineLayout m_InjectedLayout = VK_NULL_HANDLE;
		const shader::Shader<true>* m_Reflection = nullptr;

		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		uint32_t m_Subpass = 0;
		Vec<Allocator, VkFormat, 8> m_ColorFormats;
		VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;

		Vec<Allocator, VkVertexInputBindingDescription, 8> m_VertexBindings;
		Vec<Allocator, VkVertexInputAttributeDescription, 16> m_VertexAttributes;
		bool m_HasVertexInput = false;

		VkPrimitiveTopology m_Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		VkPolygonMode m_PolygonMode = VK_POLYGON_MODE_FILL;
		VkCullModeFlags m_CullMode = VK_CULL_MODE_NONE;
		VkFrontFace m_FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		VkSampleCountFlagBits m_SampleCount = VK_SAMPLE_COUNT_1_BIT;
		bool m_DepthTest = false;
		bool m_DepthWrite = false;
		bool m_Blending = false;
		std::optional<VkPipelineDepthStencilStateCreateInfo> m_DepthStencilOverride;
		std::optional<VkPipelineColorBlendStateCreateInfo> m_ColorBlendOverride;
		std::optional<VkPipelineMultisampleStateCreateInfo> m_MultisampleOverride;
		std::optional<VkPipelineRasterizationStateCreateInfo> m_RasterizationOverride;
		uint32_t m_PatchControlPoints = 0;
		bool m_HasPatchControlPoints = false;
		VkPipelineCreateFlags m_CreateFlags = 0;
		Vec<Allocator, VkDynamicState, 16> m_DynamicStates;
	};

	using PipelineBuilder = BasicPipelineBuilder<>;
}

#include "inline/pipeline.inl"
