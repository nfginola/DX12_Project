#pragma once
#include "dxcapi.h"
#include "DXCommon.h"

class CompiledShaderBlob;

/*
	Important notes about DXC.

	Always grab the latest release from https://github.com/microsoft/DirectXShaderCompiler/releases
	and use the Headers, Lib and associated Dlls!

	If we, for example, forget to grab dxil.dll, we will have trouble compiling our PSO
	https://computergraphics.stackexchange.com/questions/9953/dxc-error-when-compiling-pso
	https://docs.microsoft.com/en-us/archive/blogs/marcelolr/using-the-github-dxcompiler-dll

	For reference, the files needed are:
		- d3d12shader.h	(optional)
		- dxcapi.h

		- dxcompiler.lib
		- dxcompiler.dll
		- dxil.dll

	References:
		https://asawicki.info/news_1719_two_shader_compilers_of_direct3d_12


*/

class DXCompiler
{
public:
	struct CompileOptions
	{
		// Extend later when needed
	};

public:
	DXCompiler(ShaderModel model = ShaderModel::e6_0);
	~DXCompiler();
	
	sptr<CompiledShaderBlob> compile_from_file(
		const std::filesystem::path& rel_path, 
		ShaderType shader,
		const std::wstring entry,
		const DXCompiler::CompileOptions& options = {});
	
private:
	std::wstring grab_profile(ShaderType shader);

private:
	ShaderModel m_shader_model;

	cptr<IDxcLibrary> m_library;
	cptr<IDxcUtils> m_utils;
	cptr<IDxcCompiler> m_compiler;
	cptr<IDxcIncludeHandler> m_def_inc_hdlr;
};

