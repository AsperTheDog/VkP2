#include "shader.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <span>
#include <sstream>

#include <slang/slang-com-ptr.h>

namespace vkp::shader
{
	namespace
	{
		std::string toDiagnostics(slang::IBlob* p_Blob)
		{
			if (!p_Blob || !p_Blob->getBufferPointer())
			{
				return {};
			}
			return static_cast<const char*>(p_Blob->getBufferPointer());
		}

		std::string diagnosticsSuffix(slang::IBlob* p_Blob)
		{
			const std::string l_Text = toDiagnostics(p_Blob);
			return l_Text.empty() ? std::string{} : (": " + l_Text);
		}

		slang::IGlobalSession& globalSession()
		{
			static slang::IGlobalSession* s_Session = []
			{
				slang::IGlobalSession* l_Session = nullptr;
				if (SLANG_FAILED(slang::createGlobalSession(&l_Session)))
				{
					throw std::runtime_error("vkp::shader: failed to create Slang global session");
				}
				return l_Session;
			}();
			return *s_Session;
		}

		SlangInt findEntryPointIndex(slang::ProgramLayout* p_Layout, const SlangStage p_Stage)
		{
			for (SlangUInt i = 0; i < p_Layout->getEntryPointCount(); ++i)
			{
				if (p_Layout->getEntryPointByIndex(i)->getStage() == p_Stage)
				{
					return static_cast<SlangInt>(i);
				}
			}
			return -1;
		}

		SlangInt findEntryPointIndex(slang::ProgramLayout* p_Layout, const std::string_view p_Name)
		{
			for (SlangUInt i = 0; i < p_Layout->getEntryPointCount(); ++i)
			{
				if (p_Name == p_Layout->getEntryPointByIndex(i)->getName())
				{
					return static_cast<SlangInt>(i);
				}
			}
			return -1;
		}

		std::vector<uint32_t> emitCode(slang::IComponentType* p_Program, const SlangInt p_EntryPointIndex)
		{
			Slang::ComPtr<slang::IBlob> l_Code;
			Slang::ComPtr<slang::IBlob> l_Diagnostics;
			if (SLANG_FAILED(p_Program->getEntryPointCode(p_EntryPointIndex, 0, l_Code.writeRef(), l_Diagnostics.writeRef())))
			{
				throw std::runtime_error("vkp::shader: failed to emit SPIR-V" + diagnosticsSuffix(l_Diagnostics.get()));
			}

			const size_t l_Size = l_Code->getBufferSize();
			std::vector<uint32_t> l_Result(l_Size / sizeof(uint32_t));
			if (l_Size > 0)
			{
				std::memcpy(l_Result.data(), l_Code->getBufferPointer(), l_Size);
			}
			return l_Result;
		}

		VkShaderModule createShaderModule(const device::DeviceData& p_DeviceData, const std::span<const uint32_t> p_Code)
		{
			const VkShaderModuleCreateInfo l_CreateInfo{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.codeSize = p_Code.size() * sizeof(uint32_t),
				.pCode = p_Code.data(),
			};

			VkShaderModule l_Module = VK_NULL_HANDLE;
			VULKAN_TRY(p_DeviceData.deviceTable.vkCreateShaderModule(p_DeviceData.device, &l_CreateInfo, nullptr, &l_Module));
			return l_Module;
		}

		constexpr size_t kFnv64Offset = 14695981039346656037ULL;
		constexpr size_t kFnv64Prime = 1099511628211ULL;
		constexpr uint32_t kSpirvMagic = 0x07230203;

		void fnvUpdate(size_t& io_Hash, const std::span<const uint8_t> p_Data)
		{
			for (const uint8_t l_Byte : p_Data)
			{
				io_Hash ^= l_Byte;
				io_Hash *= kFnv64Prime;
			}
		}

		void fnvUpdateStr(size_t& io_Hash, const std::string_view p_Str)
		{
			fnvUpdate(io_Hash, { reinterpret_cast<const uint8_t*>(p_Str.data()), p_Str.size() });
		}

		std::string toHex(const size_t p_Value)
		{
			std::ostringstream l_Stream;
			l_Stream << std::hex << p_Value;
			return l_Stream.str();
		}

		void hashFileInto(size_t& io_Hash, const std::filesystem::path& p_Path)
		{
			std::ifstream l_File(p_Path, std::ios::binary);
			if (!l_File)
			{
				return;
			}

			std::array<uint8_t, 4096> l_Buffer{};
			while (l_File)
			{
				l_File.read(reinterpret_cast<char*>(l_Buffer.data()), l_Buffer.size());
				const std::streamsize l_Count = l_File.gcount();
				if (l_Count > 0)
				{
					fnvUpdate(io_Hash, { l_Buffer.data(), static_cast<size_t>(l_Count) });
				}
			}
		}

		std::vector<uint32_t> readSpirvFile(const std::filesystem::path& p_File)
		{
			std::vector<uint32_t> l_Code;
			std::ifstream l_File(p_File, std::ios::binary);
			if (!l_File)
			{
				return l_Code;
			}

			const uintmax_t l_Size = std::filesystem::file_size(p_File);
			if (l_Size == 0 || (l_Size % sizeof(uint32_t)) != 0)
			{
				return l_Code;
			}

			l_Code.resize(l_Size / sizeof(uint32_t));
			l_File.read(reinterpret_cast<char*>(l_Code.data()), static_cast<std::streamsize>(l_Size));
			if (!l_File)
			{
				l_Code.clear();
			}
			return l_Code;
		}

		void writeSpirvFile(const std::filesystem::path& p_File, const std::span<const uint32_t> p_Code)
		{
			if (p_Code.empty())
			{
				return;
			}

			std::error_code l_Error;
			std::filesystem::create_directories(p_File.parent_path(), l_Error);
			if (l_Error)
			{
				return;
			}

			std::ofstream l_File(p_File, std::ios::binary);
			if (!l_File)
			{
				return;
			}
			l_File.write(reinterpret_cast<const char*>(p_Code.data()), static_cast<std::streamsize>(p_Code.size() * sizeof(uint32_t)));
		}

		std::vector<uint32_t> codeForEntryPoint(
			slang::IComponentType* p_Program,
			const SlangInt p_EntryPointIndex,
			const std::string_view p_EntryPointName,
			const std::filesystem::path& p_CacheFolder,
			const size_t p_CacheKey)
		{
			if (p_CacheFolder.empty())
			{
				return emitCode(p_Program, p_EntryPointIndex);
			}

			const std::filesystem::path l_CacheFile = p_CacheFolder / (toHex(p_CacheKey) + "_" + std::string(p_EntryPointName) + ".spv");
			if (std::filesystem::exists(l_CacheFile))
			{
				const std::vector<uint32_t> l_Cached = readSpirvFile(l_CacheFile);
				if (!l_Cached.empty() && l_Cached[0] == kSpirvMagic)
				{
					return l_Cached;
				}
			}

			const std::vector<uint32_t> l_Code = emitCode(p_Program, p_EntryPointIndex);
			writeSpirvFile(l_CacheFile, l_Code);
			return l_Code;
		}
	}

	ShaderImpl::~ShaderImpl()
	{
		if (m_Program)
		{
			m_Program->release();
			m_Program = nullptr;
		}
		if (m_Session)
		{
			m_Session->release();
			m_Session = nullptr;
		}
	}

	ShaderImpl::ShaderImpl(ShaderImpl&& p_Other) noexcept
		: m_Session(p_Other.m_Session)
		, m_Program(p_Other.m_Program)
		, m_CacheFolder(std::move(p_Other.m_CacheFolder))
		, m_CacheKey(p_Other.m_CacheKey)
	{
		p_Other.m_Session = nullptr;
		p_Other.m_Program = nullptr;
	}

	ShaderImpl& ShaderImpl::operator=(ShaderImpl&& p_Other) noexcept
	{
		if (this != &p_Other)
		{
			if (m_Program)
			{
				m_Program->release();
			}
			if (m_Session)
			{
				m_Session->release();
			}

			m_Session = p_Other.m_Session;
			m_Program = p_Other.m_Program;
			m_CacheFolder = std::move(p_Other.m_CacheFolder);
			m_CacheKey = p_Other.m_CacheKey;
			p_Other.m_Session = nullptr;
			p_Other.m_Program = nullptr;
		}
		return *this;
	}

	std::vector<uint32_t> ShaderImpl::getSPIRV(const VkShaderStageFlagBits p_Stage) const
	{
		const SlangStage l_SlangStage = toSlangStage(p_Stage);
		if (l_SlangStage == SLANG_STAGE_NONE)
		{
			throw std::runtime_error("vkp::shader: unsupported Vulkan shader stage");
		}

		Slang::ComPtr<slang::IBlob> l_Diagnostics;
		slang::ProgramLayout* l_Layout = m_Program->getLayout(0, l_Diagnostics.writeRef());
		if (!l_Layout)
		{
			throw std::runtime_error("vkp::shader: failed to obtain program layout" + diagnosticsSuffix(l_Diagnostics.get()));
		}

		const SlangInt l_Index = findEntryPointIndex(l_Layout, l_SlangStage);
		if (l_Index < 0)
		{
			throw std::runtime_error("vkp::shader: no entry point found for the requested stage");
		}

		const char* l_EntryPointName = l_Layout->getEntryPointByIndex(static_cast<SlangUInt>(l_Index))->getName();
		return codeForEntryPoint(m_Program, l_Index, l_EntryPointName ? l_EntryPointName : "", m_CacheFolder, m_CacheKey);
	}

	std::vector<uint32_t> ShaderImpl::getSPIRV(const std::string_view p_EntryPointName) const
	{
		Slang::ComPtr<slang::IBlob> l_Diagnostics;
		slang::ProgramLayout* l_Layout = m_Program->getLayout(0, l_Diagnostics.writeRef());
		if (!l_Layout)
		{
			throw std::runtime_error("vkp::shader: failed to obtain program layout" + diagnosticsSuffix(l_Diagnostics.get()));
		}

		const SlangInt l_Index = findEntryPointIndex(l_Layout, p_EntryPointName);
		if (l_Index < 0)
		{
			throw std::runtime_error("vkp::shader: no entry point named '" + std::string(p_EntryPointName) + "'");
		}

		return codeForEntryPoint(m_Program, l_Index, p_EntryPointName, m_CacheFolder, m_CacheKey);
	}

	VkShaderModule ShaderImpl::createModule(const device::DeviceData& p_DeviceData, const VkShaderStageFlagBits p_Stage) const
	{
		const std::vector<uint32_t> l_Code = getSPIRV(p_Stage);
		return createShaderModule(p_DeviceData, l_Code);
	}

	VkShaderModule ShaderImpl::createModule(const device::DeviceData& p_DeviceData, const std::string_view p_EntryPointName) const
	{
		const std::vector<uint32_t> l_Code = getSPIRV(p_EntryPointName);
		return createShaderModule(p_DeviceData, l_Code);
	}

	slang::ProgramLayout* ShaderImpl::layout() const
	{
		Slang::ComPtr<slang::IBlob> l_Diagnostics;
		slang::ProgramLayout* l_Layout = m_Program->getLayout(0, l_Diagnostics.writeRef());
		if (!l_Layout)
		{
			throw std::runtime_error("vkp::shader: failed to obtain program layout" + diagnosticsSuffix(l_Diagnostics.get()));
		}
		return l_Layout;
	}

	ShaderBuilder& ShaderBuilder::addModule(const std::filesystem::path& p_File, const std::string_view p_ModuleName)
	{
		const std::ifstream l_Stream(p_File, std::ios::binary);
		if (!l_Stream.is_open())
		{
			throw std::runtime_error("vkp::shader: failed to open shader file '" + p_File.string() + "'");
		}

		std::stringstream l_Buffer;
		l_Buffer << l_Stream.rdbuf();

		addSearchPath(p_File.parent_path().string());
		return addModuleSource(l_Buffer.str(), p_ModuleName);
	}

	ShaderBuilder& ShaderBuilder::addModuleSource(const std::string_view p_Source, const std::string_view p_ModuleName)
	{
		m_Modules.push_back({ .source = std::string(p_Source), .name = std::string(p_ModuleName) });
		return *this;
	}

	ShaderBuilder& ShaderBuilder::addEntryPoint(const std::string_view p_Name)
	{
		for (const std::string& l_Existing : m_EntryPoints)
		{
			if (l_Existing == p_Name)
			{
				return *this;
			}
		}
		m_EntryPoints.emplace_back(p_Name);
		return *this;
	}

	ShaderBuilder& ShaderBuilder::addSearchPath(std::string p_Path)
	{
		m_SearchPaths.push_back(std::move(p_Path));
		return *this;
	}

	ShaderBuilder& ShaderBuilder::addMacro(std::string p_Name, std::string p_Value)
	{
		m_Macros.push_back({ .name = std::move(p_Name), .value = std::move(p_Value) });
		return *this;
	}

	ShaderBuilder& ShaderBuilder::setOptimization(const Optimization p_Level)
	{
		m_Optimization = p_Level;
		return *this;
	}

	ShaderBuilder& ShaderBuilder::setDebugInfo(const DebugInfo p_Level)
	{
		m_DebugInfo = p_Level;
		return *this;
	}

	ShaderBuilder& ShaderBuilder::setOptions(const CompileOptions& p_Options)
	{
		m_Optimization = p_Options.optimization;
		m_DebugInfo = p_Options.debugInfo;
		m_Macros = p_Options.macros;
		m_SearchPaths = p_Options.searchPaths;
		m_CacheFolder = p_Options.cacheFolder;
		return *this;
	}

	ShaderBuilder& ShaderBuilder::setCacheFolder(std::filesystem::path p_Folder)
	{
		m_CacheFolder = std::move(p_Folder);
		return *this;
	}

	std::unique_ptr<ShaderImpl> ShaderBuilder::compileImpl() const
	{
		if (m_Modules.empty())
		{
			throw std::runtime_error("vkp::shader: ShaderBuilder has no modules");
		}

		slang::IGlobalSession& l_Global = globalSession();

		slang::SessionDesc l_SessionDesc{};
		slang::TargetDesc l_TargetDesc{};
		l_TargetDesc.format = SLANG_SPIRV;
		l_TargetDesc.profile = l_Global.findProfile("spirv_1_5");
		if (l_TargetDesc.profile == SLANG_PROFILE_UNKNOWN)
		{
			throw std::runtime_error("vkp::shader: 'spirv_1_5' profile is not available");
		}

		std::array<slang::CompilerOptionEntry, 3> l_Options{};
		l_Options[0] = { .name = slang::CompilerOptionName::EmitSpirvDirectly, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1, .intValue1 = 0, .stringValue0 = nullptr, .stringValue1 = nullptr }};
		l_Options[1] = { .name = slang::CompilerOptionName::Optimization, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = static_cast<int32_t>(m_Optimization), .intValue1 = 0, .stringValue0 = nullptr, .stringValue1 = nullptr }};
		l_Options[2] = { .name = slang::CompilerOptionName::DebugInformation, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = static_cast<int32_t>(m_DebugInfo), .intValue1 = 0, .stringValue0 = nullptr, .stringValue1 = nullptr }};

		l_SessionDesc.targets = &l_TargetDesc;
		l_SessionDesc.targetCount = 1;
		l_SessionDesc.compilerOptionEntries = l_Options.data();
		l_SessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(l_Options.size());

		std::vector<const char*> l_SearchPaths;
		l_SearchPaths.reserve(m_SearchPaths.size());
		for (const std::string& l_Path : m_SearchPaths)
		{
			l_SearchPaths.push_back(l_Path.c_str());
		}
		l_SessionDesc.searchPaths = l_SearchPaths.data();
		l_SessionDesc.searchPathCount = static_cast<SlangInt>(l_SearchPaths.size());

		std::vector<slang::PreprocessorMacroDesc> l_MacroDescs;
		l_MacroDescs.reserve(m_Macros.size());
		for (const Macro& l_Macro : m_Macros)
		{
			l_MacroDescs.push_back({ .name = l_Macro.name.c_str(), .value = l_Macro.value.c_str() });
		}
		l_SessionDesc.preprocessorMacros = l_MacroDescs.data();
		l_SessionDesc.preprocessorMacroCount = static_cast<SlangInt>(l_MacroDescs.size());

		Slang::ComPtr<slang::ISession> l_Session;
		{
			slang::ISession* l_Raw = nullptr;
			if (SLANG_FAILED(l_Global.createSession(l_SessionDesc, &l_Raw)))
			{
				throw std::runtime_error("vkp::shader: failed to create Slang session");
			}
			l_Session.attach(l_Raw);
		}

		std::vector<slang::IComponentType*> l_Components;
		l_Components.reserve(m_Modules.size() * 2);
		std::vector<Slang::ComPtr<slang::IEntryPoint>> l_EntryPoints;
		l_EntryPoints.reserve(m_Modules.size() * 2);

		std::vector<slang::IModule*> l_Modules;
		l_Modules.reserve(m_Modules.size());

		const bool l_CacheEnabled = !m_CacheFolder.empty();
		size_t l_ContentHash = kFnv64Offset;
		if (l_CacheEnabled)
		{
			for (const Macro& l_Macro : m_Macros)
			{
				fnvUpdateStr(l_ContentHash, l_Macro.name);
				fnvUpdateStr(l_ContentHash, l_Macro.value);
			}
			const std::array<uint8_t, 2> l_Levels{ static_cast<uint8_t>(m_Optimization), static_cast<uint8_t>(m_DebugInfo) };
			fnvUpdate(l_ContentHash, l_Levels);
		}

		for (const ModuleSource& l_ModuleSrc : m_Modules)
		{
			Slang::ComPtr<slang::IBlob> l_Diagnostics;
			slang::IModule* l_Module = l_Session->loadModuleFromSourceString(
				l_ModuleSrc.name.c_str(), l_ModuleSrc.name.c_str(), l_ModuleSrc.source.c_str(), l_Diagnostics.writeRef());

			if (!l_Module)
			{
				throw std::runtime_error("vkp::shader: failed to load module '" + l_ModuleSrc.name + "'" + diagnosticsSuffix(l_Diagnostics.get()));
			}

			l_Components.push_back(l_Module);
			l_Modules.push_back(l_Module);

			if (l_CacheEnabled)
			{
				fnvUpdateStr(l_ContentHash, l_ModuleSrc.name);
				fnvUpdateStr(l_ContentHash, l_ModuleSrc.source);

				const SlangInt32 l_DependencyCount = l_Module->getDependencyFileCount();
				for (SlangInt32 i = 0; i < l_DependencyCount; ++i)
				{
					const char* l_DependencyPath = l_Module->getDependencyFilePath(i);
					if (!l_DependencyPath)
					{
						continue;
					}

					const std::filesystem::path l_Dependency(l_DependencyPath);
					if (std::filesystem::is_regular_file(l_Dependency))
					{
						hashFileInto(l_ContentHash, l_Dependency);
						continue;
					}
					for (const std::string& l_SearchPath : m_SearchPaths)
					{
						const std::filesystem::path l_Candidate = std::filesystem::path(l_SearchPath) / l_Dependency;
						if (std::filesystem::is_regular_file(l_Candidate))
						{
							hashFileInto(l_ContentHash, l_Candidate);
							break;
						}
					}
				}
			}

			if (m_EntryPoints.empty())
			{
				const SlangInt32 l_EntryPointCount = l_Module->getDefinedEntryPointCount();
				for (SlangInt32 i = 0; i < l_EntryPointCount; ++i)
				{
					Slang::ComPtr<slang::IEntryPoint> l_EntryPoint;
					if (SLANG_FAILED(l_Module->getDefinedEntryPoint(i, l_EntryPoint.writeRef())))
					{
						throw std::runtime_error("vkp::shader: failed to enumerate entry points of module '" + l_ModuleSrc.name + "'");
					}
					l_Components.push_back(l_EntryPoint.get());
					l_EntryPoints.push_back(std::move(l_EntryPoint));
				}
			}
		}

		for (const std::string& l_Name : m_EntryPoints)
		{
			bool l_Found = false;
			for (slang::IModule* l_Module : l_Modules)
			{
				Slang::ComPtr<slang::IEntryPoint> l_EntryPoint;
				if (SLANG_SUCCEEDED(l_Module->findEntryPointByName(l_Name.c_str(), l_EntryPoint.writeRef())) && l_EntryPoint)
				{
					l_Components.push_back(l_EntryPoint.get());
					l_EntryPoints.push_back(std::move(l_EntryPoint));
					l_Found = true;
					break;
				}
			}
			if (!l_Found)
			{
				throw std::runtime_error("vkp::shader: no entry point named '" + l_Name + "'");
			}
		}

		Slang::ComPtr<slang::IComponentType> l_Program;
		{
			Slang::ComPtr<slang::IBlob> l_Diagnostics;
			slang::IComponentType* l_Raw = nullptr;
			if (SLANG_FAILED(l_Session->createCompositeComponentType(l_Components.data(), static_cast<SlangInt>(l_Components.size()), &l_Raw, l_Diagnostics.writeRef())))
			{
				throw std::runtime_error("vkp::shader: failed to link shader program" + diagnosticsSuffix(l_Diagnostics.get()));
			}
			l_Program.attach(l_Raw);
		}

		auto l_Result = std::make_unique<ShaderImpl>();
		l_Result->m_Session = l_Session.detach();
		l_Result->m_Program = l_Program.detach();
		l_Result->m_CacheFolder = m_CacheFolder;
		l_Result->m_CacheKey = l_CacheEnabled ? l_ContentHash : 0;
		return l_Result;
	}

}
