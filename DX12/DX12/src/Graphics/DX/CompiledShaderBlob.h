#pragma once
#include <d3d12.h>

class CompiledShaderBlob
{
public:
	CompiledShaderBlob() = default;
	CompiledShaderBlob(void* data, size_t size);
	~CompiledShaderBlob() = default;
	
	const std::vector<uint8_t>& get_blob() { return m_generic_blob; }

	operator const std::vector<uint8_t>& ();
	//operator D3D12_SHADER_BYTECODE();

private:
	friend class DXCompiler;
private:
	std::vector<uint8_t> m_generic_blob;

};
