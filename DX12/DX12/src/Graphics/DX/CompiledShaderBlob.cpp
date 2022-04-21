#include "pch.h"
#include "CompiledShaderBlob.h"

CompiledShaderBlob::CompiledShaderBlob(void* data, size_t size)
{
	m_generic_blob.resize(size);
	uint8_t* blobStart = (uint8_t*)data;
	uint8_t* blobEnd = blobStart + size;
	std::copy(blobStart, blobEnd, m_generic_blob.data());
}


CompiledShaderBlob::operator const std::vector<uint8_t>& ()
{
	return m_generic_blob;
}

//CompiledShaderBlob::operator D3D12_SHADER_BYTECODE()
//{
//	D3D12_SHADER_BYTECODE bc{};
//	bc.BytecodeLength = m_generic_blob.size();
//	bc.pShaderBytecode = m_generic_blob.data();
//	return bc;
//}
