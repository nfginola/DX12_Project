#include "pch.h"
#include "DXCompiler.h"

#include "CompiledShaderBlob.h"

DXCompiler::DXCompiler(ShaderModel model) :
	m_shader_model(model)
{
	// Grab interfaces
	HRESULT hr;
	hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&m_library));
	if (FAILED(hr))
		throw std::runtime_error("Failed to intiialize IDxcLibrary");

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler));
	if (FAILED(hr))
		throw std::runtime_error("Failed to initialize IDxcCompiler");

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils));
	if (FAILED(hr))
		throw std::runtime_error("Failed to initialize IDxcUtils");

	// Grab default include handler
	m_utils->CreateDefaultIncludeHandler(m_def_inc_hdlr.GetAddressOf());
}

DXCompiler::~DXCompiler()
{
}

sptr<CompiledShaderBlob> DXCompiler::compile_from_file(
	const std::filesystem::path& rel_path, 
	ShaderType type,
	const std::wstring entry,
	const DXCompiler::CompileOptions& options)
{
	std::wstring profile = grab_profile(type);

	// Create blob to store compiled data
	uint32_t code_page = CP_UTF8;
	cptr<IDxcBlobEncoding> sourceBlob;

	HRESULT hr;
	hr = m_library->CreateBlobFromFile(rel_path.c_str(), &code_page, &sourceBlob);
	if (FAILED(hr))
		throw std::runtime_error("Failed to create blob");

	// Compile
	cptr<IDxcOperationResult> result;
	hr = m_compiler->Compile(
		sourceBlob.Get(), // pSource
		rel_path.c_str(),// pSourceName
		entry.c_str(), // pEntryPoint
		profile.c_str(), // pTargetProfile
		NULL, 0, // pArguments, argCount
		NULL, 0, // pDefines, defineCount
		m_def_inc_hdlr.Get(),
		result.GetAddressOf()); // ppResult

	if (SUCCEEDED(hr))
		result->GetStatus(&hr);

	// Check error
	if (FAILED(hr))
	{
		cptr<IDxcBlobEncoding> errors;
		hr = result->GetErrorBuffer(&errors);
		if (SUCCEEDED(hr) && errors)
		{
			wprintf(L"Compilation failed with errors:\n%hs\n",
				(const char*)errors->GetBufferPointer());
			throw std::runtime_error("Failed to compile shader");
		}
	}

	cptr<IDxcBlob> res;
	hr = result->GetResult(res.GetAddressOf());
	if (FAILED(hr))
		throw std::runtime_error("Failed to get result");

	auto compiled = std::make_shared<CompiledShaderBlob>(res->GetBufferPointer(), (UINT)res->GetBufferSize());
	return compiled;
}

std::wstring DXCompiler::grab_profile(ShaderType type)
{
	std::wstring profile;
	switch (type)
	{
	case ShaderType::eVertex:
		profile = L"vs";
		break;
	case ShaderType::ePixel:
		profile = L"ps";
		break;
	case ShaderType::eGeometry:
		profile = L"gs";
		break;
	case ShaderType::eHull:
		profile = L"hs";
		break;
	case ShaderType::eDomain:
		profile = L"ds";
		break;
	case ShaderType::eCompute:
		profile = L"cs";
		break;
	default:
		assert(false);
	}
	profile += L"_";

	switch (m_shader_model)
	{
	case ShaderModel::e6_0:
		profile += L"6_0";
		break;
	default:
		assert(false);
	}
	return profile;
}
