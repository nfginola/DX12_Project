#pragma once
#include <d3d12.h>

class CompiledShaderBlob
{
public:
	CompiledShaderBlob() = default;
	CompiledShaderBlob(void* data, size_t size);
	~CompiledShaderBlob() = default;

	operator D3D12_SHADER_BYTECODE();

private:
	friend class DXCompiler;
private:
	std::vector<uint8_t> m_generic_blob;

};
