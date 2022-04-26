#pragma once
#include <d3d12.h>

class DXTexture
{
public:
	DXTexture() = default;
	DXTexture(cptr<ID3D12Resource> tex);

	ID3D12Resource* resource() const;

private:
	cptr<ID3D12Resource> m_res;
};

