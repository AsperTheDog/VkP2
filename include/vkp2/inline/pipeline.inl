
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include "shader.hpp"

namespace vkp::pipeline
{
	namespace
	{
		constexpr SlangInt kUnboundedThreshold = 1 << 24;

		struct AttributeFormat
		{
			VkFormat format;
			uint32_t size;
			uint32_t alignment;
		};

		AttributeFormat attributeFormatFor(const slang::TypeReflection::ScalarType p_Scalar, const uint32_t p_Extent)
		{
			using S = slang::TypeReflection::ScalarType;
			const VkFormat l_Format = [p_Scalar, p_Extent]() -> VkFormat
			{
				switch (p_Scalar)
				{
				case S::Float64:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R64_SFLOAT;
					case 2: return VK_FORMAT_R64G64_SFLOAT;
					case 3: return VK_FORMAT_R64G64B64_SFLOAT;
					case 4: return VK_FORMAT_R64G64B64A64_SFLOAT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				case S::Float32:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R32_SFLOAT;
					case 2: return VK_FORMAT_R32G32_SFLOAT;
					case 3: return VK_FORMAT_R32G32B32_SFLOAT;
					case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				case S::Float16:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R16_SFLOAT;
					case 2: return VK_FORMAT_R16G16_SFLOAT;
					case 3: return VK_FORMAT_R16G16B16_SFLOAT;
					case 4: return VK_FORMAT_R16G16B16A16_SFLOAT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				case S::Int32:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R32_SINT;
					case 2: return VK_FORMAT_R32G32_SINT;
					case 3: return VK_FORMAT_R32G32B32_SINT;
					case 4: return VK_FORMAT_R32G32B32A32_SINT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				case S::Int16:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R16_SINT;
					case 2: return VK_FORMAT_R16G16_SINT;
					case 3: return VK_FORMAT_R16G16B16_SINT;
					case 4: return VK_FORMAT_R16G16B16A16_SINT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				case S::Int8:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R8_SINT;
					case 2: return VK_FORMAT_R8G8_SINT;
					case 3: return VK_FORMAT_R8G8B8_SINT;
					case 4: return VK_FORMAT_R8G8B8A8_SINT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				case S::UInt32:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R32_UINT;
					case 2: return VK_FORMAT_R32G32_UINT;
					case 3: return VK_FORMAT_R32G32B32_UINT;
					case 4: return VK_FORMAT_R32G32B32A32_UINT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				case S::UInt16:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R16_UINT;
					case 2: return VK_FORMAT_R16G16_UINT;
					case 3: return VK_FORMAT_R16G16B16_UINT;
					case 4: return VK_FORMAT_R16G16B16A16_UINT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				case S::UInt8:
				case S::Bool:
					switch (p_Extent)
					{
					case 1: return VK_FORMAT_R8_UINT;
					case 2: return VK_FORMAT_R8G8_UINT;
					case 3: return VK_FORMAT_R8G8B8_UINT;
					case 4: return VK_FORMAT_R8G8B8A8_UINT;
					default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute extent");
					}
				default: throw std::runtime_error("vkp::pipeline: unsupported vertex attribute scalar type");
				}
			}();
			const uint32_t l_ComponentBytes = [p_Scalar]()
			{
				switch (p_Scalar)
				{
				case S::Float64: return 8u;
				case S::Float32:
				case S::Int32:
				case S::UInt32: return 4u;
				case S::Float16:
				case S::Int16:
				case S::UInt16: return 2u;
				default: return 1u;
				}
			}();
			return { .format = l_Format, .size = l_ComponentBytes * p_Extent, .alignment = l_ComponentBytes > 4 ? 8u : 4u };
		}

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
			default: throw std::runtime_error("vkp::pipeline: unsupported reflection binding type");
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

		bool isSystemSemantic(const char* p_Semantic)
		{
			return p_Semantic && std::string_view(p_Semantic).starts_with("SV_");
		}

		template<typename Allocator = std::allocator<void>>
		Vec<Allocator, VkPushConstantRange, 4> reflectedPushConstantRanges(slang::ProgramLayout* p_Program, const VkShaderStageFlags p_Stages, const Allocator& p_Allocator = Allocator())
		{
			Vec<Allocator, VkPushConstantRange, 4> l_Ranges(p_Allocator);
			slang::VariableLayoutReflection* l_Global = p_Program->getGlobalParamsVarLayout();
			if (!l_Global)
			{
				return l_Ranges;
			}
			slang::TypeLayoutReflection* l_GlobalLayout = l_Global->getTypeLayout();
			if (!l_GlobalLayout)
			{
				return l_Ranges;
			}
			for (SlangInt i = 0; i < l_GlobalLayout->getBindingRangeCount(); ++i)
			{
				if (l_GlobalLayout->getBindingRangeType(i) != slang::BindingType::PushConstant)
				{
					continue;
				}
				slang::TypeLayoutReflection* l_Leaf = l_GlobalLayout->getBindingRangeLeafTypeLayout(i);
				slang::TypeLayoutReflection* l_Element = l_Leaf ? l_Leaf->getElementTypeLayout() : nullptr;
				const size_t l_Size = l_Element ? l_Element->getSize() : (l_Leaf ? l_Leaf->getSize() : 0);
				if (l_Size == 0 || l_Size > UINT32_MAX)
				{
					continue;
				}
				l_Ranges.push_back({ .stageFlags = p_Stages, .offset = 0, .size = static_cast<uint32_t>(l_Size) });
			}
			return l_Ranges;
		}

		std::filesystem::path pipelineCachePath(const device::DeviceData& p_DeviceData, const std::filesystem::path& p_Folder)
		{
			VkPhysicalDeviceProperties l_Properties{};
			vkGetPhysicalDeviceProperties(p_DeviceData.physicalDevice, &l_Properties);
			char l_Name[64];
			std::snprintf(l_Name, sizeof(l_Name), "pipeline_%04x_%04x_%08x.bin", l_Properties.vendorID, l_Properties.deviceID, l_Properties.driverVersion);
			return p_Folder / l_Name;
		}

		template<typename Allocator = std::allocator<void>>
		VkPipelineCache loadPipelineCache(const device::DeviceData& p_DeviceData, const std::filesystem::path& p_Folder, const Allocator& p_Allocator = Allocator())
		{
			const std::filesystem::path l_Path = pipelineCachePath(p_DeviceData, p_Folder);
			if (!std::filesystem::exists(l_Path))
			{
				return VK_NULL_HANDLE;
			}

			std::ifstream l_File(l_Path, std::ios::binary);
			if (!l_File)
			{
				return VK_NULL_HANDLE;
			}
			using ByteAlloc = std::allocator_traits<Allocator>::template rebind_alloc<uint8_t>;
			std::vector<uint8_t, ByteAlloc> l_Data((std::istreambuf_iterator<char>(l_File)), std::istreambuf_iterator<char>(), ByteAlloc(p_Allocator));
			if (l_Data.empty())
			{
				return VK_NULL_HANDLE;
			}

			const VkPipelineCacheCreateInfo l_CreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.initialDataSize = l_Data.size(),
				.pInitialData = l_Data.data(),
			};
			VkPipelineCache l_Cache = VK_NULL_HANDLE;
			VULKAN_TRY(p_DeviceData.deviceTable.vkCreatePipelineCache(p_DeviceData.device, &l_CreateInfo, nullptr, &l_Cache));
			return l_Cache;
		}

		template<typename Allocator = std::allocator<void>>
		void storePipelineCache(const device::DeviceData& p_DeviceData, const std::filesystem::path& p_Folder, const VkPipelineCache p_Cache, const Allocator& p_Allocator = Allocator())
		{
			if (p_Cache == VK_NULL_HANDLE)
			{
				return;
			}
			size_t l_Size = 0;
			VULKAN_TRY(p_DeviceData.deviceTable.vkGetPipelineCacheData(p_DeviceData.device, p_Cache, &l_Size, nullptr));
			if (l_Size == 0)
			{
				return;
			}
			using ByteAlloc = std::allocator_traits<Allocator>::template rebind_alloc<uint8_t>;
			std::vector<uint8_t, ByteAlloc> l_Data(l_Size, 0, ByteAlloc(p_Allocator));
			VULKAN_TRY(p_DeviceData.deviceTable.vkGetPipelineCacheData(p_DeviceData.device, p_Cache, &l_Size, l_Data.data()));

			const std::filesystem::path l_Path = pipelineCachePath(p_DeviceData, p_Folder);
			std::error_code l_Error;
			std::filesystem::create_directories(l_Path.parent_path(), l_Error);
			if (l_Error)
			{
				return;
			}
			std::ofstream l_File(l_Path, std::ios::binary);
			if (!l_File)
			{
				return;
			}
			l_File.write(reinterpret_cast<const char*>(l_Data.data()), static_cast<std::streamsize>(l_Data.size()));
		}

		template<typename Allocator = std::allocator<void>>
		class PipelineCacheGuard
		{
		public:
			PipelineCacheGuard(const device::DeviceData& p_DeviceData, const VkPipelineCache p_Explicit, std::filesystem::path p_Folder, const Allocator& p_Allocator)
				: m_DeviceData(p_DeviceData), m_Folder(std::move(p_Folder)), m_Cache(p_Explicit), m_Allocator(p_Allocator)
			{
				if (m_Cache == VK_NULL_HANDLE && !m_Folder.empty())
				{
					m_Cache = loadPipelineCache(m_DeviceData, m_Folder, m_Allocator);
					if (m_Cache == VK_NULL_HANDLE)
					{
						constexpr VkPipelineCacheCreateInfo l_CreateInfo{
							.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
							.pNext = nullptr,
							.flags = 0,
							.initialDataSize = 0,
							.pInitialData = nullptr,
						};
						VULKAN_TRY(m_DeviceData.deviceTable.vkCreatePipelineCache(m_DeviceData.device, &l_CreateInfo, nullptr, &m_Cache));
					}
					m_Owned = true;
				}
			}

			~PipelineCacheGuard()
			{
				if (m_Owned)
				{
					storePipelineCache(m_DeviceData, m_Folder, m_Cache, m_Allocator);
					if (m_Cache != VK_NULL_HANDLE)
					{
						m_DeviceData.deviceTable.vkDestroyPipelineCache(m_DeviceData.device, m_Cache, nullptr);
					}
				}
			}

			PipelineCacheGuard(const PipelineCacheGuard&) = delete;
			PipelineCacheGuard& operator=(const PipelineCacheGuard&) = delete;

			[[nodiscard]] VkPipelineCache get() const { return m_Cache; }

		private:
			const device::DeviceData& m_DeviceData;
			std::filesystem::path m_Folder;
			VkPipelineCache m_Cache = VK_NULL_HANDLE;
			Allocator m_Allocator;
			bool m_Owned = false;
		};
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>::BasicPipelineBuilder(const Allocator& p_Allocator)
		: m_Allocator(p_Allocator),
		  m_Stages(m_Allocator),
		  m_DescriptorSetLayouts(m_Allocator),
		  m_PushConstantRanges(m_Allocator),
		  m_ColorFormats(m_Allocator),
		  m_VertexBindings(m_Allocator),
		  m_VertexAttributes(m_Allocator),
		  m_DynamicStates(m_Allocator)
	{
		m_DynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		m_DynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::addShaderStage(const VkShaderModule p_Module, const VkShaderStageFlagBits p_Stage, const std::string_view p_EntryPointName)
	{
		m_Stages.emplace_back(p_Module, p_Stage, std::string(p_EntryPointName), m_Allocator);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::addShaderStage(const VkShaderModule p_Module, const VkShaderStageFlagBits p_Stage, const std::string_view p_EntryPointName, const std::span<const VkSpecializationMapEntry> p_SpecializationMap, const std::span<const uint8_t> p_SpecializationData)
	{
		Stage& l_Stage = m_Stages.emplace_back(p_Module, p_Stage, std::string(p_EntryPointName), m_Allocator);
		l_Stage.hasSpecialization = true;
		l_Stage.specializationEntries.assign(p_SpecializationMap.begin(), p_SpecializationMap.end());
		l_Stage.specializationData.assign(p_SpecializationData.begin(), p_SpecializationData.end());
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::addDescriptorSetLayout(const VkDescriptorSetLayout p_Layout)
	{
		m_DescriptorSetLayouts.push_back(p_Layout);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::addPushConstantRange(const VkPushConstantRange p_Range)
	{
		m_PushConstantRanges.push_back(p_Range);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setPipelineLayout(const VkPipelineLayout p_Layout)
	{
		m_InjectedLayout = p_Layout;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setPipelineCacheFolder(std::filesystem::path p_Folder)
	{
		m_CacheFolder = std::move(p_Folder);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::useReflection(const shader::Shader<true>& p_Shader)
	{
		m_Reflection = &p_Shader;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setRenderPass(const VkRenderPass p_RenderPass, const uint32_t p_Subpass)
	{
		m_RenderPass = p_RenderPass;
		m_Subpass = p_Subpass;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setColorFormats(const std::span<const VkFormat> p_Formats)
	{
		m_ColorFormats.assign(p_Formats.begin(), p_Formats.end());
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setDepthFormat(const VkFormat p_Format)
	{
		m_DepthFormat = p_Format;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setVertexInput(const std::span<const VkVertexInputBindingDescription> p_Bindings, const std::span<const VkVertexInputAttributeDescription> p_Attributes)
	{
		m_VertexBindings.assign(p_Bindings.begin(), p_Bindings.end());
		m_VertexAttributes.assign(p_Attributes.begin(), p_Attributes.end());
		m_HasVertexInput = true;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setTopology(const VkPrimitiveTopology p_Topology)
	{
		m_Topology = p_Topology;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setPolygonMode(const VkPolygonMode p_Mode)
	{
		m_PolygonMode = p_Mode;
		m_RasterizationOverride.reset();
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setCullMode(const VkCullModeFlags p_CullMode)
	{
		m_CullMode = p_CullMode;
		m_RasterizationOverride.reset();
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setFrontFace(const VkFrontFace p_FrontFace)
	{
		m_FrontFace = p_FrontFace;
		m_RasterizationOverride.reset();
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setSampleCount(const VkSampleCountFlagBits p_SampleCount)
	{
		m_SampleCount = p_SampleCount;
		m_MultisampleOverride.reset();
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setDepthTest(const bool p_Test, const bool p_Write)
	{
		m_DepthTest = p_Test;
		m_DepthWrite = p_Write;
		m_DepthStencilOverride.reset();
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setBlending(const bool p_Enable)
	{
		m_Blending = p_Enable;
		m_ColorBlendOverride.reset();
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& p_State)
	{
		m_DepthStencilOverride = p_State;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setColorBlendState(const VkPipelineColorBlendStateCreateInfo& p_State)
	{
		m_ColorBlendOverride = p_State;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setMultisampleState(const VkPipelineMultisampleStateCreateInfo& p_State)
	{
		m_MultisampleOverride = p_State;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setRasterizationState(const VkPipelineRasterizationStateCreateInfo& p_State)
	{
		m_RasterizationOverride = p_State;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setPatchControlPoints(const uint32_t p_Count)
	{
		m_PatchControlPoints = p_Count;
		m_HasPatchControlPoints = true;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setFlags(const VkPipelineCreateFlags p_Flags)
	{
		m_CreateFlags = p_Flags;
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setDynamicDepth(const bool p_Test, const bool p_Write)
	{
		m_DepthTest = p_Test;
		m_DepthWrite = p_Write;
		addDynamicState(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
		addDynamicState(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
		addDynamicState(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setDynamicCull()
	{
		addDynamicState(VK_DYNAMIC_STATE_CULL_MODE);
		addDynamicState(VK_DYNAMIC_STATE_FRONT_FACE);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setDynamicRasterizerDiscard()
	{
		addDynamicState(VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setDynamicDepthBias()
	{
		addDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::setDynamicBlendConstants()
	{
		addDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
		return *this;
	}

	template<typename Allocator>
	BasicPipelineBuilder<Allocator>& BasicPipelineBuilder<Allocator>::addDynamicState(const VkDynamicState p_State)
	{
		if (std::ranges::find(m_DynamicStates, p_State) == m_DynamicStates.end())
		{
			m_DynamicStates.push_back(p_State);
		}
		return *this;
	}

	template<typename Allocator>
	VkShaderStageFlags BasicPipelineBuilder<Allocator>::stageMask() const
	{
		VkShaderStageFlags l_Mask = 0;
		for (const Stage& l_Stage : m_Stages)
		{
			l_Mask |= l_Stage.stage;
		}
		return l_Mask;
	}

	template<typename Allocator>
	Vec<Allocator, VkDescriptorSetLayout, 8> BasicPipelineBuilder<Allocator>::createDescriptorSetLayouts(const device::DeviceData& p_DeviceData) const
	{
		if (!m_Reflection)
		{
			return Vec<Allocator, VkDescriptorSetLayout, 8>(m_Allocator);
		}

		slang::ProgramLayout* l_Program = m_Reflection->layout();
		slang::VariableLayoutReflection* l_Global = l_Program->getGlobalParamsVarLayout();
		if (!l_Global)
		{
			return Vec<Allocator, VkDescriptorSetLayout, 8>(m_Allocator);
		}
		slang::TypeLayoutReflection* l_GlobalLayout = l_Global->getTypeLayout();
		if (!l_GlobalLayout)
		{
			return Vec<Allocator, VkDescriptorSetLayout, 8>(m_Allocator);
		}

		uint32_t l_SetCount = 0;
		for (SlangInt i = 0; i < l_GlobalLayout->getDescriptorSetCount(); ++i)
		{
			l_SetCount = std::max(l_SetCount, static_cast<uint32_t>(l_GlobalLayout->getDescriptorSetSpaceOffset(i)) + 1);
		}

		Vec<Allocator, VkDescriptorSetLayout, 8> l_Created(m_Allocator);
		l_Created.resize(l_SetCount, VK_NULL_HANDLE);
		for (SlangInt i = 0; i < l_GlobalLayout->getDescriptorSetCount(); ++i)
		{
			const uint32_t l_Space = static_cast<uint32_t>(l_GlobalLayout->getDescriptorSetSpaceOffset(i));
			Vec<Allocator, VkDescriptorSetLayoutBinding, 32> l_Bindings(m_Allocator);
			Vec<Allocator, VkDescriptorBindingFlags, 32> l_Flags(m_Allocator);

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
				throw std::runtime_error("vkp::pipeline: multiple unbounded descriptor arrays in one set are not supported by Vulkan; place each unbounded array in its own descriptor space");
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
				l_Binding.stageFlags = stageMask();

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

	template<typename Allocator>
	void BasicPipelineBuilder<Allocator>::generateVertexInput()
	{
		if (!m_Reflection)
		{
			return;
		}

		slang::ProgramLayout* l_Program = m_Reflection->layout();
		slang::EntryPointLayout* l_VertexEntry = nullptr;
		for (SlangUInt i = 0; i < l_Program->getEntryPointCount(); ++i)
		{
			slang::EntryPointLayout* l_Entry = l_Program->getEntryPointByIndex(i);
			if (l_Entry->getStage() == SLANG_STAGE_VERTEX)
			{
				l_VertexEntry = l_Entry;
				break;
			}
		}
		if (!l_VertexEntry)
		{
			return;
		}

		struct Cursor
		{
			uint32_t location = 0;
			uint32_t offset = 0;

			void advance(const uint32_t p_Size)
			{
				offset = (offset + 3u) & ~3u;
				offset += p_Size;
			}
		};

		Cursor l_Cursor;

		const auto l_EmitType = [&](const auto& p_Self, slang::TypeReflection* p_Type) -> void
		{
			using K = slang::TypeReflection::Kind;
			switch (p_Type->getKind())
			{
			case K::Scalar:
			{
				const AttributeFormat l_Fmt = attributeFormatFor(p_Type->getScalarType(), 1);
				l_Cursor.offset = (l_Cursor.offset + l_Fmt.alignment - 1u) & ~(l_Fmt.alignment - 1u);
				m_VertexAttributes.push_back({ .location = l_Cursor.location, .binding = 0, .format = l_Fmt.format, .offset = l_Cursor.offset });
				++l_Cursor.location;
				l_Cursor.advance(l_Fmt.size);
				break;
			}
			case K::Vector:
			{
				const AttributeFormat l_Fmt = attributeFormatFor(p_Type->getScalarType(), static_cast<uint32_t>(p_Type->getElementCount()));
				l_Cursor.offset = (l_Cursor.offset + l_Fmt.alignment - 1u) & ~(l_Fmt.alignment - 1u);
				m_VertexAttributes.push_back({ .location = l_Cursor.location, .binding = 0, .format = l_Fmt.format, .offset = l_Cursor.offset });
				++l_Cursor.location;
				l_Cursor.advance(l_Fmt.size);
				break;
			}
			case K::Matrix:
			{
				const AttributeFormat l_Fmt = attributeFormatFor(p_Type->getScalarType(), p_Type->getRowCount());
				for (uint32_t c = 0; c < p_Type->getColumnCount(); ++c)
				{
					l_Cursor.offset = (l_Cursor.offset + l_Fmt.alignment - 1u) & ~(l_Fmt.alignment - 1u);
					m_VertexAttributes.push_back({ .location = l_Cursor.location, .binding = 0, .format = l_Fmt.format, .offset = l_Cursor.offset });
					++l_Cursor.location;
					l_Cursor.advance(l_Fmt.size);
				}
				break;
			}
			case K::Struct:
			{
				for (uint32_t f = 0; f < p_Type->getFieldCount(); ++f)
				{
					slang::VariableReflection* l_Field = p_Type->getFieldByIndex(f);
					p_Self(p_Self, l_Field->getType());
				}
				break;
			}
			case K::Array:
			{
				const size_t l_Count = p_Type->getElementCount();
				if (l_Count == SLANG_UNKNOWN_SIZE || l_Count == SLANG_UNBOUNDED_SIZE)
				{
					throw std::runtime_error("vkp::pipeline: unsized vertex input array");
				}
				for (uint32_t i = 0; i < l_Count; ++i)
				{
					p_Self(p_Self, p_Type->getElementType());
				}
				break;
			}
			default:
				throw std::runtime_error("vkp::pipeline: unsupported vertex input type");
			}
		};

		for (uint32_t i = 0; i < l_VertexEntry->getParameterCount(); ++i)
		{
			slang::VariableLayoutReflection* l_Parameter = l_VertexEntry->getParameterByIndex(i);
			if (isSystemSemantic(l_Parameter->getSemanticName()))
			{
				continue;
			}
			l_EmitType(l_EmitType, l_Parameter->getTypeLayout()->getType());
		}

		m_VertexBindings.push_back({ .binding = 0, .stride = (l_Cursor.offset + 3u) & ~3u, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX });
	}

	template<typename Allocator>
	BasicPipelineData<Allocator> BasicPipelineBuilder<Allocator>::buildGraphics(const device::DeviceData& p_DeviceData, const VkPipelineCache p_PipelineCache)
	{
		if (m_Stages.empty())
		{
			throw std::runtime_error("vkp::pipeline: no shader stages");
		}
		if (m_RenderPass == VK_NULL_HANDLE && m_ColorFormats.empty())
		{
			throw std::runtime_error("vkp::pipeline: no color formats or render pass");
		}
		if (m_Reflection && !m_DescriptorSetLayouts.empty())
		{
			throw std::runtime_error("vkp::pipeline: cannot mix reflection and explicit descriptor set layouts");
		}

		if (!m_HasVertexInput)
		{
			const bool l_HasVertexStage = std::ranges::any_of(m_Stages, [](const Stage& p_Stage) { return p_Stage.stage == VK_SHADER_STAGE_VERTEX_BIT; });
			if (l_HasVertexStage)
			{
				generateVertexInput();
			}
		}

		Vec<Allocator, VkDescriptorSetLayout, 8> l_CreatedLayouts(m_Allocator);
		std::span<const VkDescriptorSetLayout> l_Layouts;
		VkPipelineLayout l_Layout = VK_NULL_HANDLE;
		BasicPipelineData<Allocator> l_Out(m_Allocator);

		if (m_InjectedLayout != VK_NULL_HANDLE)
		{
			l_Layout = m_InjectedLayout;
		}
		else
		{
			if (m_Reflection)
			{
				l_CreatedLayouts = createDescriptorSetLayouts(p_DeviceData);
				l_Layouts = l_CreatedLayouts;
			}
			else
			{
				l_Layouts = m_DescriptorSetLayouts;
			}

			Vec<Allocator, VkPushConstantRange, 4> l_PushRanges(m_Allocator);
			for (const auto& l_Range : m_PushConstantRanges)
			{
				l_PushRanges.push_back(l_Range);
			}

			if (m_Reflection && l_PushRanges.empty())
			{
				auto l_ReflectedRanges = reflectedPushConstantRanges(m_Reflection->layout(), stageMask(), m_Allocator);
				for (const auto& l_Range : l_ReflectedRanges)
				{
					l_PushRanges.push_back(l_Range);
				}
			}

			const VkPipelineLayoutCreateInfo l_LayoutInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.setLayoutCount = static_cast<uint32_t>(l_Layouts.size()),
				.pSetLayouts = l_Layouts.data(),
				.pushConstantRangeCount = static_cast<uint32_t>(l_PushRanges.size()),
				.pPushConstantRanges = l_PushRanges.data(),
			};
			VULKAN_TRY(p_DeviceData.deviceTable.vkCreatePipelineLayout(p_DeviceData.device, &l_LayoutInfo, nullptr, &l_Layout));
		}

		Vec<Allocator, VkPipelineShaderStageCreateInfo, 8> l_StageInfos(m_Allocator);
		l_StageInfos.reserve(m_Stages.size());
		Vec<Allocator, VkSpecializationInfo, 8> l_SpecInfos(m_Allocator);
		l_SpecInfos.reserve(m_Stages.size());
		for (const Stage& l_Stage : m_Stages)
		{
			if (l_Stage.hasSpecialization)
			{
				l_SpecInfos.push_back({
					.mapEntryCount = static_cast<uint32_t>(l_Stage.specializationEntries.size()),
					.pMapEntries = l_Stage.specializationEntries.data(),
					.dataSize = l_Stage.specializationData.size(),
					.pData = l_Stage.specializationData.data(),
				});
			}
			else
			{
				l_SpecInfos.push_back({});
			}
		}
		for (size_t i = 0; i < m_Stages.size(); ++i)
		{
			const Stage& l_Stage = m_Stages[i];
			l_StageInfos.push_back({
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.stage = l_Stage.stage,
				.module = l_Stage.module,
				.pName = l_Stage.entryPointName.c_str(),
				.pSpecializationInfo = l_Stage.hasSpecialization ? &l_SpecInfos[i] : nullptr,
			});
		}

		const VkPipelineVertexInputStateCreateInfo l_VertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.vertexBindingDescriptionCount = static_cast<uint32_t>(m_VertexBindings.size()),
			.pVertexBindingDescriptions = m_VertexBindings.data(),
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_VertexAttributes.size()),
			.pVertexAttributeDescriptions = m_VertexAttributes.data(),
		};

		const VkPipelineInputAssemblyStateCreateInfo l_AssemblyInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.topology = m_Topology,
			.primitiveRestartEnable = VK_FALSE,
		};

		const VkPipelineRasterizationStateCreateInfo l_RasterInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = m_PolygonMode,
			.cullMode = m_CullMode,
			.frontFace = m_FrontFace,
			.depthBiasEnable = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f,
		};

		const VkPipelineMultisampleStateCreateInfo l_MultisampleInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.rasterizationSamples = m_SampleCount,
			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 1.0f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE,
		};

		const VkPipelineDepthStencilStateCreateInfo l_DepthInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = m_DepthTest ? VK_TRUE : VK_FALSE,
			.depthWriteEnable = m_DepthWrite ? VK_TRUE : VK_FALSE,
			.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {},
			.back = {},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f,
		};

		const uint32_t l_AttachmentCount = static_cast<uint32_t>(m_ColorFormats.size());
		std::array<VkPipelineColorBlendAttachmentState, 8> l_BlendAttachments{};
		for (uint32_t i = 0; i < l_AttachmentCount; ++i)
		{
			l_BlendAttachments[i] = {
				.blendEnable = m_Blending ? VK_TRUE : VK_FALSE,
				.srcColorBlendFactor = m_Blending ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE,
				.dstColorBlendFactor = m_Blending ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ZERO,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = m_Blending ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE,
				.dstAlphaBlendFactor = m_Blending ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ZERO,
				.alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			};
		}

		const VkPipelineColorBlendStateCreateInfo l_BlendInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = l_AttachmentCount,
			.pAttachments = l_BlendAttachments.data(),
			.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
		};

		constexpr VkPipelineViewportStateCreateInfo l_ViewportInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr,
		};

		const VkPipelineDynamicStateCreateInfo l_DynamicInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dynamicStateCount = static_cast<uint32_t>(m_DynamicStates.size()),
			.pDynamicStates = m_DynamicStates.data(),
		};

		const VkPipelineRenderingCreateInfo l_RenderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.pNext = nullptr,
			.viewMask = 0,
			.colorAttachmentCount = l_AttachmentCount,
			.pColorAttachmentFormats = m_ColorFormats.data(),
			.depthAttachmentFormat = m_DepthFormat,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
		};

		const VkPipelineRasterizationStateCreateInfo* l_RasterState = m_RasterizationOverride ? &*m_RasterizationOverride : &l_RasterInfo;
		const VkPipelineMultisampleStateCreateInfo* l_MultisampleState = m_MultisampleOverride ? &*m_MultisampleOverride : &l_MultisampleInfo;
		const VkPipelineDepthStencilStateCreateInfo* l_DepthStencilState = m_DepthStencilOverride ? &*m_DepthStencilOverride : &l_DepthInfo;
		const VkPipelineColorBlendStateCreateInfo* l_BlendState = m_ColorBlendOverride ? &*m_ColorBlendOverride : &l_BlendInfo;

		const bool l_TessellationActive = m_Topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
			|| std::ranges::any_of(m_Stages, [](const Stage& p_Stage)
			{
				return p_Stage.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || p_Stage.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			});
		const VkPipelineTessellationStateCreateInfo* l_TessState = nullptr;
		VkPipelineTessellationStateCreateInfo l_TessInfo{};
		if (l_TessellationActive)
		{
			if (!m_HasPatchControlPoints)
			{
				throw std::runtime_error("vkp::pipeline: tessellation pipeline requires patch control points");
			}
			l_TessInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.patchControlPoints = m_PatchControlPoints,
			};
			l_TessState = &l_TessInfo;
		}

		VkGraphicsPipelineCreateInfo l_PipelineInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = m_RenderPass == VK_NULL_HANDLE ? &l_RenderingInfo : nullptr,
			.flags = m_CreateFlags,
			.stageCount = static_cast<uint32_t>(l_StageInfos.size()),
			.pStages = l_StageInfos.data(),
			.pVertexInputState = &l_VertexInputInfo,
			.pInputAssemblyState = &l_AssemblyInfo,
			.pTessellationState = l_TessState,
			.pViewportState = &l_ViewportInfo,
			.pRasterizationState = l_RasterState,
			.pMultisampleState = l_MultisampleState,
			.pDepthStencilState = l_DepthStencilState,
			.pColorBlendState = l_BlendState,
			.pDynamicState = &l_DynamicInfo,
			.layout = l_Layout,
			.renderPass = m_RenderPass,
			.subpass = m_Subpass,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1,
		};

		const PipelineCacheGuard<Allocator> l_Cache(p_DeviceData, p_PipelineCache, m_CacheFolder, m_Allocator);
		VkPipeline l_Pipeline = VK_NULL_HANDLE;
		VULKAN_TRY(p_DeviceData.deviceTable.vkCreateGraphicsPipelines(p_DeviceData.device, l_Cache.get(), 1, &l_PipelineInfo, nullptr, &l_Pipeline));

		l_Out.pipeline = l_Pipeline;
		l_Out.layout = l_Layout;
		l_Out.descriptorSetLayouts = std::move(l_CreatedLayouts);
		return l_Out;
	}

	template<typename Allocator>
	BasicPipelineData<Allocator> BasicPipelineBuilder<Allocator>::buildCompute(const device::DeviceData& p_DeviceData, const VkPipelineCache p_PipelineCache)
	{
		if (m_Stages.size() != 1 || m_Stages[0].stage != VK_SHADER_STAGE_COMPUTE_BIT)
		{
			throw std::runtime_error("vkp::pipeline: compute pipeline requires exactly one compute stage");
		}

		Vec<Allocator, VkDescriptorSetLayout, 8> l_CreatedLayouts(m_Allocator);
		std::span<const VkDescriptorSetLayout> l_Layouts;
		VkPipelineLayout l_Layout = VK_NULL_HANDLE;
		BasicPipelineData<Allocator> l_Out(m_Allocator);

		if (m_InjectedLayout != VK_NULL_HANDLE)
		{
			l_Layout = m_InjectedLayout;
		}
		else
		{
			if (m_Reflection)
			{
				l_CreatedLayouts = createDescriptorSetLayouts(p_DeviceData);
				l_Layouts = l_CreatedLayouts;
			}
			else
			{
				l_Layouts = m_DescriptorSetLayouts;
			}

			Vec<Allocator, VkPushConstantRange, 4> l_PushRanges(m_PushConstantRanges, m_Allocator);
			if (m_Reflection && l_PushRanges.empty())
			{
				l_PushRanges = reflectedPushConstantRanges(m_Reflection->layout(), stageMask(), m_Allocator);
			}

			const VkPipelineLayoutCreateInfo l_LayoutInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.setLayoutCount = static_cast<uint32_t>(l_Layouts.size()),
				.pSetLayouts = l_Layouts.data(),
				.pushConstantRangeCount = static_cast<uint32_t>(l_PushRanges.size()),
				.pPushConstantRanges = l_PushRanges.data(),
			};
			VULKAN_TRY(p_DeviceData.deviceTable.vkCreatePipelineLayout(p_DeviceData.device, &l_LayoutInfo, nullptr, &l_Layout));
		}

		const Stage& l_Stage = m_Stages[0];
		VkSpecializationInfo l_SpecInfo{};
		if (l_Stage.hasSpecialization)
		{
			l_SpecInfo = {
				.mapEntryCount = static_cast<uint32_t>(l_Stage.specializationEntries.size()),
				.pMapEntries = l_Stage.specializationEntries.data(),
				.dataSize = l_Stage.specializationData.size(),
				.pData = l_Stage.specializationData.data(),
			};
		}
		const VkPipelineShaderStageCreateInfo l_StageInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = l_Stage.stage,
			.module = l_Stage.module,
			.pName = l_Stage.entryPointName.c_str(),
			.pSpecializationInfo = l_Stage.hasSpecialization ? &l_SpecInfo : nullptr,
		};

		const VkComputePipelineCreateInfo l_PipelineInfo{
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.pNext = nullptr,
			.flags = m_CreateFlags,
			.stage = l_StageInfo,
			.layout = l_Layout,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1,
		};

		const PipelineCacheGuard<Allocator> l_Cache(p_DeviceData, p_PipelineCache, m_CacheFolder, m_Allocator);
		VkPipeline l_Pipeline = VK_NULL_HANDLE;
		VULKAN_TRY(p_DeviceData.deviceTable.vkCreateComputePipelines(p_DeviceData.device, l_Cache.get(), 1, &l_PipelineInfo, nullptr, &l_Pipeline));

		l_Out.pipeline = l_Pipeline;
		l_Out.layout = l_Layout;
		l_Out.descriptorSetLayouts = std::move(l_CreatedLayouts);
		return l_Out;
	}
}
