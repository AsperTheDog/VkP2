#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <slang/slang.h>

#include "device.hpp"

namespace vkp::shader
{
	[[nodiscard]] constexpr SlangStage toSlangStage(const VkShaderStageFlagBits p_Stage) noexcept
	{
		switch (p_Stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT: return SLANG_STAGE_VERTEX;
		case VK_SHADER_STAGE_FRAGMENT_BIT: return SLANG_STAGE_FRAGMENT;
		case VK_SHADER_STAGE_COMPUTE_BIT: return SLANG_STAGE_COMPUTE;
		case VK_SHADER_STAGE_GEOMETRY_BIT: return SLANG_STAGE_GEOMETRY;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return SLANG_STAGE_HULL;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return SLANG_STAGE_DOMAIN;
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR: return SLANG_STAGE_RAY_GENERATION;
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: return SLANG_STAGE_ANY_HIT;
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: return SLANG_STAGE_CLOSEST_HIT;
		case VK_SHADER_STAGE_MISS_BIT_KHR: return SLANG_STAGE_MISS;
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR: return SLANG_STAGE_INTERSECTION;
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR: return SLANG_STAGE_CALLABLE;
		case VK_SHADER_STAGE_MESH_BIT_EXT: return SLANG_STAGE_MESH;
		case VK_SHADER_STAGE_TASK_BIT_EXT: return SLANG_STAGE_AMPLIFICATION;
		default: return SLANG_STAGE_NONE;
		}
	}

	[[nodiscard]] constexpr VkShaderStageFlagBits toVulkanStage(const SlangStage p_Stage) noexcept
	{
		switch (p_Stage)
		{
		case SLANG_STAGE_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
		case SLANG_STAGE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
		case SLANG_STAGE_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
		case SLANG_STAGE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
		case SLANG_STAGE_HULL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		case SLANG_STAGE_DOMAIN: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		case SLANG_STAGE_RAY_GENERATION: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case SLANG_STAGE_ANY_HIT: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		case SLANG_STAGE_CLOSEST_HIT: return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		case SLANG_STAGE_MISS: return VK_SHADER_STAGE_MISS_BIT_KHR;
		case SLANG_STAGE_INTERSECTION: return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		case SLANG_STAGE_CALLABLE: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		case SLANG_STAGE_MESH: return VK_SHADER_STAGE_MESH_BIT_EXT;
		case SLANG_STAGE_AMPLIFICATION: return VK_SHADER_STAGE_TASK_BIT_EXT;
		default: return static_cast<VkShaderStageFlagBits>(0);
		}
	}

	enum class Optimization : uint8_t
	{
		None = 0,
		Default = 1,
		High = 2,
		Maximal = 3,
	};

	enum class DebugInfo : uint8_t
	{
		None = 0,
		Minimal = 1,
		Standard = 2,
		Maximal = 3,
	};

	struct Macro
	{
		std::string name;
		std::string value;
	};

	struct CompileOptions
	{
		Optimization optimization = Optimization::None;
		DebugInfo debugInfo = DebugInfo::None;
		std::vector<Macro> macros;
		std::vector<std::string> searchPaths;
		std::filesystem::path cacheFolder;
	};

	class ShaderImpl
	{
	public:
		ShaderImpl() = default;
		~ShaderImpl();
		ShaderImpl(ShaderImpl&& p_Other) noexcept;
		ShaderImpl& operator=(ShaderImpl&& p_Other) noexcept;
		ShaderImpl(const ShaderImpl&) = delete;
		ShaderImpl& operator=(const ShaderImpl&) = delete;

		[[nodiscard]] std::vector<uint32_t> getSPIRV(VkShaderStageFlagBits p_Stage) const;
		[[nodiscard]] std::vector<uint32_t> getSPIRV(std::string_view p_EntryPointName) const;
		[[nodiscard]] VkShaderModule createModule(const device::DeviceData& p_DeviceData, VkShaderStageFlagBits p_Stage) const;
		[[nodiscard]] VkShaderModule createModule(const device::DeviceData& p_DeviceData, std::string_view p_EntryPointName) const;
		[[nodiscard]] slang::ProgramLayout* layout() const;

	private:
		friend class ShaderBuilder;

		slang::ISession* m_Session = nullptr;
		slang::IComponentType* m_Program = nullptr;
		std::filesystem::path m_CacheFolder;
		size_t m_CacheKey = 0;
	};

	template<bool Reflect = false>
	class Shader
	{
	public:
		Shader() noexcept = default;
		Shader(Shader&&) noexcept = default;
		Shader& operator=(Shader&&) noexcept = default;
		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;
		~Shader() = default;

		[[nodiscard]] explicit operator bool() const noexcept { return m_Impl != nullptr; }

		[[nodiscard]] std::vector<uint32_t> getSPIRV(const VkShaderStageFlagBits p_Stage) const
		{
			return impl().getSPIRV(p_Stage);
		}

		[[nodiscard]] std::vector<uint32_t> getSPIRV(const std::string_view p_EntryPointName) const
		{
			return impl().getSPIRV(p_EntryPointName);
		}

		[[nodiscard]] VkShaderModule createModule(const device::DeviceData& p_DeviceData, const VkShaderStageFlagBits p_Stage) const
		{
			return impl().createModule(p_DeviceData, p_Stage);
		}

		[[nodiscard]] VkShaderModule createModule(const device::DeviceData& p_DeviceData, const std::string_view p_EntryPointName) const
		{
			return impl().createModule(p_DeviceData, p_EntryPointName);
		}

		[[nodiscard]] slang::ProgramLayout* layout() const requires Reflect
		{
			return impl().layout();
		}

	private:
		friend class ShaderBuilder;
		explicit Shader(std::unique_ptr<ShaderImpl> p_Impl) noexcept : m_Impl(std::move(p_Impl)) {}

		[[nodiscard]] ShaderImpl& impl() const
		{
			if (!m_Impl)
			{
				throw std::runtime_error("vkp::shader: shader is empty (moved-from or default-constructed)");
			}
			return *m_Impl;
		}

		std::unique_ptr<ShaderImpl> m_Impl;
	};

	class ShaderBuilder
	{
	public:
		ShaderBuilder& addModule(const std::filesystem::path& p_File, std::string_view p_ModuleName);
		ShaderBuilder& addModuleSource(std::string_view p_Source, std::string_view p_ModuleName);
		ShaderBuilder& addEntryPoint(std::string_view p_Name);
		ShaderBuilder& addSearchPath(std::string p_Path);
		ShaderBuilder& addMacro(std::string p_Name, std::string p_Value);
		ShaderBuilder& setOptimization(Optimization p_Level);
		ShaderBuilder& setDebugInfo(DebugInfo p_Level);
		ShaderBuilder& setOptions(const CompileOptions& p_Options);
		ShaderBuilder& setCacheFolder(std::filesystem::path p_Folder);

		template<bool Reflect = false>
		[[nodiscard]] Shader<Reflect> build() const;

	private:
		struct ModuleSource
		{
			std::string source;
			std::string name;
		};

		[[nodiscard]] std::unique_ptr<ShaderImpl> compileImpl() const;

		std::vector<ModuleSource> m_Modules;
		std::vector<std::string> m_EntryPoints;
		std::vector<std::string> m_SearchPaths;
		std::vector<Macro> m_Macros;
		Optimization m_Optimization = Optimization::None;
		DebugInfo m_DebugInfo = DebugInfo::None;
		std::filesystem::path m_CacheFolder;
	};

	template<bool Reflect>
	[[nodiscard]] Shader<Reflect> ShaderBuilder::build() const
	{
		return Shader<Reflect>{ compileImpl() };
	}

	template<bool Reflect = false>
	[[nodiscard]] Shader<Reflect> compileFromSource(const std::string_view p_Source, const std::string_view p_ModuleName, const CompileOptions& p_Options = {})
	{
		ShaderBuilder l_Builder;
		l_Builder.setOptions(p_Options);
		l_Builder.addModuleSource(p_Source, p_ModuleName);
		return l_Builder.build<Reflect>();
	}

	template<bool Reflect = false>
	[[nodiscard]] Shader<Reflect> compileFromFile(const std::filesystem::path& p_File, const std::string_view p_ModuleName, const CompileOptions& p_Options = {})
	{
		ShaderBuilder l_Builder;
		l_Builder.setOptions(p_Options);
		l_Builder.addModule(p_File, p_ModuleName);
		return l_Builder.build<Reflect>();
	}
}
