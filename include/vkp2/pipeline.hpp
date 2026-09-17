#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "concepts.hpp"
#include "device.hpp"
#include "extra/static_vector.hpp"

namespace vkp::shader
{
	template<bool Reflect>
	class Shader;
}

namespace vkp::pipeline
{
	struct PipelineCapacities
	{
		uint32_t shaderStages = 0;              // addShaderStage calls; also sizes the stage and specialization infos
		uint32_t descriptorSetLayouts = 0;      // addDescriptorSetLayout calls, or layouts reflected from the shader
		uint32_t descriptorBindings = 0;        // bindings inside a reflected layout, and their flags
		uint32_t pushConstantRanges = 0;        // addPushConstantRange calls, or ranges reflected from the shader
		uint32_t vertexBindings = 0;            // setVertexInput bindings, or the single binding generated from reflection
		uint32_t vertexAttributes = 0;          // setVertexInput attributes, or attributes reflected from the shader
		uint32_t dynamicStates = 0;             // addDynamicState calls; the builder seeds viewport and scissor, so at least 2
		uint32_t specializationEntries = 0;     // entries in one addShaderStage specialization map
		uint32_t specializationDataBytes = 0;   // bytes of one addShaderStage specialization data block
	};

	inline constexpr PipelineCapacities DefaultPipelineCapacities{
		.shaderStages = 8,
		.descriptorSetLayouts = 8,
		.descriptorBindings = 32,
		.pushConstantRanges = 4,
		.vertexBindings = 8,
		.vertexAttributes = 16,
		.dynamicStates = 16,
		.specializationEntries = 8,
		.specializationDataBytes = 64,
	};

	template<Sequence TSpecializationEntries, Sequence TSpecializationData>
	struct ShaderStage
	{
		ShaderStage() = default;

		ShaderStage(const VkShaderModule p_Module, const VkShaderStageFlagBits p_Stage, std::string p_EntryPointName, TSpecializationEntries p_SpecializationEntries, TSpecializationData p_SpecializationData)
			: module(p_Module)
			, stage(p_Stage)
			, entryPointName(std::move(p_EntryPointName))
			, specializationEntries(std::move(p_SpecializationEntries))
			, specializationData(std::move(p_SpecializationData))
		{
		}

		VkShaderModule module = VK_NULL_HANDLE;
		VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
		std::string entryPointName;
		TSpecializationEntries specializationEntries;
		TSpecializationData specializationData;
		bool hasSpecialization = false;
	};

	struct NoAllocator {};

	template<typename T>
	concept StoragePolicy = requires
	{
		typename T::ProtoAllocator;
		typename T::Stage;
		typename T::Stages;
		typename T::DescriptorSetLayouts;
		typename T::PushConstantRanges;
		typename T::ColorFormats;
		typename T::VertexBindings;
		typename T::VertexAttributes;
		typename T::DynamicStates;
		typename T::ShaderStageInfos;
		typename T::SpecializationInfos;
	}
	&& Sequence<typename T::Stages>
	&& Sequence<typename T::DescriptorSetLayouts>
	&& Sequence<typename T::PushConstantRanges>
	&& Sequence<typename T::ColorFormats>
	&& Sequence<typename T::VertexBindings>
	&& Sequence<typename T::VertexAttributes>
	&& Sequence<typename T::DynamicStates>
	&& Sequence<typename T::ShaderStageInfos>
	&& Sequence<typename T::SpecializationInfos>
	&& requires(const typename T::ProtoAllocator& p_Allocator)
	{
		{ T::template make<typename T::Stages>(p_Allocator) } -> std::same_as<typename T::Stages>;
	};

	template<PipelineCapacities Capacities>
	struct StaticPipelineStorage
	{
		using ProtoAllocator = NoAllocator;

		using SpecializationEntries = StaticVector<VkSpecializationMapEntry, Capacities.specializationEntries>;
		using SpecializationData = StaticVector<uint8_t, Capacities.specializationDataBytes>;
		using Stage = ShaderStage<SpecializationEntries, SpecializationData>;
		using Stages = StaticVector<Stage, Capacities.shaderStages>;
		using DescriptorSetLayouts = StaticVector<VkDescriptorSetLayout, Capacities.descriptorSetLayouts>;
		using DescriptorBindings = StaticVector<VkDescriptorSetLayoutBinding, Capacities.descriptorBindings>;
		using DescriptorBindingFlags = StaticVector<VkDescriptorBindingFlags, Capacities.descriptorBindings>;
		using PushConstantRanges = StaticVector<VkPushConstantRange, Capacities.pushConstantRanges>;
		using ColorFormats = StaticVector<VkFormat, 8>;   // color attachments are capped at 8 by the hardware
		using VertexBindings = StaticVector<VkVertexInputBindingDescription, Capacities.vertexBindings>;
		using VertexAttributes = StaticVector<VkVertexInputAttributeDescription, Capacities.vertexAttributes>;
		using DynamicStates = StaticVector<VkDynamicState, Capacities.dynamicStates>;
		using ShaderStageInfos = StaticVector<VkPipelineShaderStageCreateInfo, Capacities.shaderStages>;
		using SpecializationInfos = StaticVector<VkSpecializationInfo, Capacities.shaderStages>;

		template<typename TContainer>
		[[nodiscard]] static TContainer make(const ProtoAllocator&) { return TContainer{}; }
	};

	template<StoragePolicy TStorage>
	struct BasicPipelineData
	{
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		TStorage::DescriptorSetLayouts descriptorSetLayouts;
		bool ownsLayout = true;
	};

	template<PipelineCapacities Capacities>
	using PipelineData = BasicPipelineData<StaticPipelineStorage<Capacities>>;

	using DefaultPipelineData = PipelineData<DefaultPipelineCapacities>;

	template<StoragePolicy TStorage>
	class BasicPipelineBuilder
	{
	public:
		explicit BasicPipelineBuilder(TStorage::ProtoAllocator p_Allocator = {});

		BasicPipelineBuilder& addShaderStage(VkShaderModule p_Module, VkShaderStageFlagBits p_Stage, std::string_view p_EntryPointName = "main");
		BasicPipelineBuilder& addShaderStage(VkShaderModule p_Module, VkShaderStageFlagBits p_Stage, std::string_view p_EntryPointName, std::span<const VkSpecializationMapEntry> p_SpecializationMap, std::span<const uint8_t> p_SpecializationData);
		BasicPipelineBuilder& addDescriptorSetLayout(VkDescriptorSetLayout p_Layout);
		BasicPipelineBuilder& addPushConstantRange(VkPushConstantRange p_Range);
		BasicPipelineBuilder& setPipelineLayout(VkPipelineLayout p_Layout);
		BasicPipelineBuilder& useReflection(const shader::Shader<true>& p_Shader);

		BasicPipelineBuilder& setPipelineCacheFolder(std::filesystem::path p_Folder);
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

		[[nodiscard]] BasicPipelineData<TStorage> buildGraphics(const device::DeviceData& p_DeviceData, VkPipelineCache p_PipelineCache = VK_NULL_HANDLE);
		[[nodiscard]] BasicPipelineData<TStorage> buildCompute(const device::DeviceData& p_DeviceData, VkPipelineCache p_PipelineCache = VK_NULL_HANDLE);

	private:
		template<Sequence TContainer>
		[[nodiscard]] TContainer makeContainer() const { return TStorage::template make<TContainer>(m_Allocator); }

		VkShaderStageFlags stageMask() const;
		void generateVertexInput();
		TStorage::DescriptorSetLayouts createDescriptorSetLayouts(const device::DeviceData& p_DeviceData) const;

		TStorage::ProtoAllocator m_Allocator{};
		TStorage::Stages m_Stages;
		TStorage::DescriptorSetLayouts m_DescriptorSetLayouts;
		TStorage::PushConstantRanges m_PushConstantRanges;
		std::filesystem::path m_CacheFolder;

		VkPipelineLayout m_InjectedLayout = VK_NULL_HANDLE;
		const shader::Shader<true>* m_Reflection = nullptr;

		TStorage::ColorFormats m_ColorFormats;
		VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;

		TStorage::VertexBindings m_VertexBindings;
		TStorage::VertexAttributes m_VertexAttributes;
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
		TStorage::DynamicStates m_DynamicStates;
	};

	template<PipelineCapacities Capacities>
	using PipelineBuilder = BasicPipelineBuilder<StaticPipelineStorage<Capacities>>;

	using DefaultPipelineBuilder = PipelineBuilder<DefaultPipelineCapacities>;

	template<StoragePolicy TStorage>
	void destroyPipeline(const device::DeviceData& p_DeviceData, BasicPipelineData<TStorage>& p_Pipeline);
}

#include "inline/pipeline.inl"
