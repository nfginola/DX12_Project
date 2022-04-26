#include "pch.h"
#include "DXTexture.h"

DXTexture::DXTexture(cptr<ID3D12Resource> tex) :
	m_res(tex)
{
}

ID3D12Resource* DXTexture::resource() const
{
	return m_res.Get();
}
