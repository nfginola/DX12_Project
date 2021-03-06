#include "pch.h"
#include "DXTextureManager.h"


DXTextureManager::DXTextureManager(cptr<ID3D12Device> dev, cptr<ID3D12CommandQueue> wait_queue) :
	m_dev(dev),
	m_wait_queue(wait_queue)
{
	m_up_batch = std::make_unique<DirectX::ResourceUploadBatch>(dev.Get());

	/*
		Load default texture
	*/
	DXTextureDesc def{};
	def.filepath = "textures/default_invalid.png";
	def.flag = TextureFlag::eSRGB;
	def.usage_cpu = UsageIntentCPU::eUpdateNever;
	def.usage_gpu = UsageIntentGPU::eReadMultipleTimesPerFrame;
	m_def_tex = create_texture(def);
}

TextureHandle DXTextureManager::create_texture(const DXTextureDesc& desc)
{
	if (!desc.filepath.has_filename())
		return m_def_tex;			// default texture

	auto it = m_loaded_path_to_handle.find(desc.filepath.string());
	if (it != m_loaded_path_to_handle.cend())
	{
		return TextureHandle(it->second);;
	}

	m_up_batch->Begin();

	// Dynamic textures not supported for now (but we will soon)
	if (desc.usage_cpu != UsageIntentCPU::eUpdateNever)
		assert(false);

	DirectX::WIC_LOADER_FLAGS flags =
		DirectX::WIC_LOADER_FLAGS::WIC_LOADER_MIP_AUTOGEN |
		DirectX::WIC_LOADER_FLAGS::WIC_LOADER_FORCE_RGBA32;
		//DirectX::WIC_LOADER_FLAGS::WIC_LOADER_FIT_POW2;		// It seems like it sometimes uses BGRA instead..

	if (desc.flag == TextureFlag::eSRGB)
		flags |= DirectX::WIC_LOADER_FLAGS::WIC_LOADER_FORCE_SRGB;

	cptr<ID3D12Resource> res;
	// force SRGB (we do gamma correct rendering)
	auto hr = DirectX::CreateWICTextureFromFileEx(m_dev.Get(), *m_up_batch.get(), desc.filepath.c_str(), 0, D3D12_RESOURCE_FLAG_NONE, flags, res.GetAddressOf());

	if (FAILED(hr))
		assert(false);

	auto [handle, internal_res] = m_handles.get_next_free_handle();
	internal_res->tex = DXTexture(res);


	auto finish = m_up_batch->End(m_wait_queue.Get());

	// Wait for the upload thread to terminate
	finish.wait();

	m_loaded_path_to_handle.insert({ desc.filepath.string(), handle});

	return TextureHandle(handle);
}

void DXTextureManager::destroy_texture(TextureHandle handle)
{
	m_handles.free_handle(handle.handle);

	auto it = std::find_if(m_loaded_path_to_handle.begin(), m_loaded_path_to_handle.end(), [handle](auto it) { return it.second == handle.handle;  });

	if (it != m_loaded_path_to_handle.cend())
		m_loaded_path_to_handle.erase(it);
}

ID3D12Resource* DXTextureManager::get_resource(TextureHandle tex)
{
	return m_handles.get_resource(tex.handle)->tex.resource();
}

void DXTextureManager::create_srv(TextureHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
{
	auto res = m_handles.get_resource(handle.handle);

	D3D12_RESOURCE_DESC desc = res->tex.resource()->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC vdesc{};
	vdesc.Format = desc.Format;
	vdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	vdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	vdesc.Texture2D.MipLevels = -1;
	vdesc.Texture2D.MostDetailedMip = 0;
	vdesc.Texture2D.PlaneSlice = 0;
	vdesc.Texture2D.ResourceMinLODClamp = 0;

	m_dev->CreateShaderResourceView(res->tex.resource(), &vdesc, descriptor);

}
