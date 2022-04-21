#pragma once
#include "DXCommon.h"

static D3D12_RENDER_TARGET_BLEND_DESC def_rt_blend_desc()
{
	D3D12_RENDER_TARGET_BLEND_DESC bDesc{};
	bDesc.BlendEnable = FALSE;
	bDesc.LogicOpEnable = FALSE;
	bDesc.SrcBlend = D3D12_BLEND_ONE;
	bDesc.DestBlend = D3D12_BLEND_ZERO;
	bDesc.BlendOp = D3D12_BLEND_OP_ADD;
	bDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	bDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	bDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	bDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	bDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

static D3D12_RASTERIZER_DESC def_raster_desc()
{
	D3D12_RASTERIZER_DESC desc{};
	desc.FillMode = D3D12_FILL_MODE_SOLID;
	desc.CullMode = D3D12_CULL_MODE_BACK;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0.0f;
	desc.SlopeScaledDepthBias = 0.0f;
	desc.DepthClipEnable = TRUE;
	desc.MultisampleEnable = FALSE;
	desc.AntialiasedLineEnable = FALSE;
	desc.ForcedSampleCount = 0;
	desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	return desc;
}

static D3D12_DEPTH_STENCIL_DESC def_ds_desc()
{
	D3D12_DEPTH_STENCIL_DESC desc{};
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	desc.FrontFace.StencilFailOp = desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	desc.FrontFace.StencilDepthFailOp = desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	desc.FrontFace.StencilPassOp = desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFunc = desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	return desc;
}

static D3D12_BLEND_DESC def_blend_desc()
{
	D3D12_BLEND_DESC desc{};
	desc.AlphaToCoverageEnable = FALSE;
	desc.IndependentBlendEnable = FALSE;
	desc.RenderTarget[0].BlendEnable = FALSE;
	desc.RenderTarget[0].LogicOpEnable = FALSE;
	desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return desc;
}

// Default anisotropic sampler
static D3D12_STATIC_SAMPLER_DESC def_static_samp_desc(UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility)
{
	D3D12_STATIC_SAMPLER_DESC desc{};
	desc.ShaderRegister = shaderRegister;
	desc.RegisterSpace = registerSpace;
	desc.Filter = D3D12_FILTER_ANISOTROPIC;

	desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias = 0;
	desc.MaxAnisotropy = 16;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	desc.MinLOD = 0.f;
	desc.MaxLOD = D3D12_FLOAT32_MAX;
	desc.ShaderVisibility = visibility;

	return desc;
}

// Default anisotropic sampler
static D3D12_SAMPLER_DESC def_samp_desc()
{
	D3D12_SAMPLER_DESC desc{};

	desc.Filter = D3D12_FILTER_ANISOTROPIC;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias = 0;
	desc.MaxAnisotropy = 8;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.f;
	desc.MinLOD = 0.f;
	desc.MaxLOD = D3D12_FLOAT32_MAX;
	return desc;
}

class RootSigBuilder
{
private:
	D3D12_ROOT_PARAMETER internal_desc_fill(D3D12_ROOT_PARAMETER_TYPE type, D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister, UINT space)
	{
		D3D12_ROOT_PARAMETER param{};
		param.ShaderVisibility = visibility;
		param.ParameterType = type;
		param.Descriptor.ShaderRegister = shaderRegister;
		param.Descriptor.RegisterSpace = space;
		return param;
	}
public:
	RootSigBuilder() = default;
	~RootSigBuilder() = default;

	RootSigBuilder& push_cbv(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility, UINT* rootParamIdx = nullptr)
	{
		auto param = internal_desc_fill(D3D12_ROOT_PARAMETER_TYPE_CBV, visibility, shaderRegister, space);
		if (rootParamIdx)
			*rootParamIdx = (UINT)m_params.size();
		m_params.push_back(param);
		return *this;
	}

	RootSigBuilder& push_srv(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility, UINT* rootParamIdx = nullptr)
	{
		auto param = internal_desc_fill(D3D12_ROOT_PARAMETER_TYPE_SRV, visibility, shaderRegister, space);
		if (rootParamIdx)
			*rootParamIdx = (UINT)m_params.size();
		m_params.push_back(param);
		return *this;
	}

	RootSigBuilder& push_uav(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility, UINT* rootParamIdx = nullptr)
	{
		auto param = internal_desc_fill(D3D12_ROOT_PARAMETER_TYPE_UAV, visibility, shaderRegister, space);
		if (rootParamIdx)
			*rootParamIdx = (UINT)m_params.size();
		m_params.push_back(param);
		return *this;
	}

	RootSigBuilder& push_table(const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges, D3D12_SHADER_VISIBILITY visibility, UINT* rootParamIdx = nullptr)
	{
		m_tableRangesPerParam.push_back(ranges);

		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;

		// attach this to local copied memory
		// we need this to live until we serialize our rootsig
		param.DescriptorTable.NumDescriptorRanges = (UINT)m_tableRangesPerParam.back().size();
		param.DescriptorTable.pDescriptorRanges = m_tableRangesPerParam.back().data();

		param.ShaderVisibility = visibility;
		if (rootParamIdx)
			*rootParamIdx = (UINT)m_params.size();
		m_params.push_back(param);
		return *this;
	}

	RootSigBuilder& push_constant(UINT shaderRegister, UINT space, UINT dwords, D3D12_SHADER_VISIBILITY visibility, UINT* rootParamIdx = nullptr)
	{
		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;	// DWORD constant
		param.Constants.Num32BitValues = dwords;
		param.Constants.RegisterSpace = space;
		param.Constants.ShaderRegister = shaderRegister;
		param.ShaderVisibility = visibility;
		if (rootParamIdx)
			*rootParamIdx = (UINT)m_params.size();
		m_params.push_back(param);
		return *this;
	}

	RootSigBuilder& add_static_sampler(const D3D12_STATIC_SAMPLER_DESC& desc)
	{
		m_staticSamplers.push_back(desc);
		return *this;
	}

	// Different from the other builder API since its special case (ID3D12RootSignature is a created device primitive)
	cptr<ID3D12RootSignature> build(ID3D12Device* dev, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE)
	{
		cptr<ID3DBlob> signature;
		cptr<ID3DBlob> error;

		m_desc.NumParameters = (UINT)m_params.size();
		m_desc.pParameters = m_params.data();
		m_desc.NumStaticSamplers = (UINT)m_staticSamplers.size();
		m_desc.pStaticSamplers = m_staticSamplers.data();
		m_desc.Flags = flags;

		HRESULT hr = D3D12SerializeRootSignature(&m_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		if (FAILED(hr))
		{
			std::string msg = "Compilation failed with errors:\n";
			msg += std::string((const char*)error->GetBufferPointer());

			OutputDebugStringA(msg.c_str());

			throw std::runtime_error("Failed to compile shader");
		}

		ThrowIfFailed(hr, DET_ERR("Failed to serialize root sig."));
		ThrowIfFailed(dev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(m_root_sig.GetAddressOf())), DET_ERR("Failed to create root sig."));

		return m_root_sig;
	}

private:
	cptr<ID3D12RootSignature> m_root_sig;
	D3D12_ROOT_SIGNATURE_DESC m_desc{};
	std::vector<D3D12_ROOT_PARAMETER> m_params;
	std::vector<D3D12_STATIC_SAMPLER_DESC> m_staticSamplers;
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> m_tableRangesPerParam;

};

class BlendDescBuilder
{
public:
	BlendDescBuilder& SetAlphaToCoverage(bool enabled = false) { m_desc.AlphaToCoverageEnable = enabled; return *this; };
	BlendDescBuilder& SetIndependentBlend(bool enabled = false) { m_desc.IndependentBlendEnable = enabled; return *this; };
	BlendDescBuilder& AppendRTBlendDesc(const D3D12_RENDER_TARGET_BLEND_DESC& desc, bool enabled = false) { m_rt_blends.push_back(desc); return *this; };

	BlendDescBuilder(const D3D12_BLEND_DESC& desc = def_blend_desc()) : m_desc(desc) { m_rt_blends.push_back(m_desc.RenderTarget[0]); };
	~BlendDescBuilder() = default;

private:
	friend class PipelineBuilder;

	D3D12_BLEND_DESC& build()
	{
		// https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_blend_desc

		if (m_rt_blends.empty())
			throw std::runtime_error(DET_ERR("RT Blend on Slot 0 MUST be filled!"));

		m_desc.RenderTarget[0] = m_rt_blends[0];

		if (!m_desc.IndependentBlendEnable && m_rt_blends.size() > 1)
			throw std::runtime_error(DET_ERR("All other RT Blend except for Slot 0 is ignored, do you want this?"));

		return m_desc;
	}

private:
	std::vector<D3D12_RENDER_TARGET_BLEND_DESC> m_rt_blends;
	D3D12_BLEND_DESC m_desc;

};

class InputLayoutBuilder
{
public:
	// Ordering matters
	InputLayoutBuilder& append(
		LPCSTR semanticName,
		UINT semanticIndex,
		DXGI_FORMAT format,
		UINT inputSlot = 0,
		UINT instanceDataStepRate = 0,
		D3D12_INPUT_CLASSIFICATION inputClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		UINT alignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT)
	{
		D3D12_INPUT_ELEMENT_DESC desc{};
		desc.SemanticName = semanticName;
		desc.SemanticIndex = semanticIndex;
		desc.Format = format;
		desc.InputSlot = inputSlot;
		desc.AlignedByteOffset = alignedByteOffset;
		desc.InputSlotClass = inputClass;
		desc.InstanceDataStepRate = instanceDataStepRate;
		m_input_descs.push_back(desc);
		return *this;
	}

	InputLayoutBuilder() = default;
	~InputLayoutBuilder() = default;

private:
	friend class PipelineBuilder;

	/*
		We let PipelineBuilder use this builder internally.
		We have to guarantee that the input element descs are alive
	*/

	D3D12_INPUT_LAYOUT_DESC& build()
	{
		// safety check
		for (const auto& desc : m_input_descs)
		{
			if (desc.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA && desc.InstanceDataStepRate == 0)
				throw std::runtime_error(DET_ERR("Per instance data specified, but instace data steprate is 0!"));
		}

		// This builder needs to be kept alive until PSO is built!
		m_il_desc.NumElements = (UINT)m_input_descs.size();
		m_il_desc.pInputElementDescs = m_input_descs.data();
		return m_il_desc;
	}

private:
	D3D12_INPUT_LAYOUT_DESC m_il_desc{};
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_input_descs;
};

class DepthStencilDescBuilder
{
public:
	DepthStencilDescBuilder(const D3D12_DEPTH_STENCIL_DESC& desc = def_ds_desc()) : m_desc(desc) {};
	~DepthStencilDescBuilder() = default;

	DepthStencilDescBuilder& set_depth_enabled(BOOL enabled) { m_desc.DepthEnable = enabled; return *this; }
	DepthStencilDescBuilder& set_depth_write_mask(D3D12_DEPTH_WRITE_MASK mask) { m_desc.DepthWriteMask = mask; return *this; }
	DepthStencilDescBuilder& set_depth_func(D3D12_COMPARISON_FUNC func) { m_desc.DepthFunc = func; return *this; }

	DepthStencilDescBuilder& set_stencil_enabled(BOOL enabled) { m_desc.StencilEnable = enabled; return *this; }
	DepthStencilDescBuilder& set_stencil_read_mask(UINT8 mask) { m_desc.StencilReadMask = mask; return *this; }
	DepthStencilDescBuilder& set_stencil_write_mask(UINT8 mask) { m_desc.StencilWriteMask = mask; return *this; }

	DepthStencilDescBuilder& set_stencil_op_front_face(const D3D12_DEPTH_STENCILOP_DESC& desc) { m_desc.FrontFace = desc; return *this; }
	DepthStencilDescBuilder& set_stencil_op_back_face(const D3D12_DEPTH_STENCILOP_DESC& desc) { m_desc.BackFace = desc; return *this; }

private:
	friend class PipelineBuilder;
	D3D12_DEPTH_STENCIL_DESC& build() { return m_desc; };

private:
	D3D12_DEPTH_STENCIL_DESC m_desc;
};

class RasterizerDescBuilder
{
public:
	RasterizerDescBuilder(const D3D12_RASTERIZER_DESC& desc = def_raster_desc()) : m_desc(desc) {};
	~RasterizerDescBuilder() = default;

	RasterizerDescBuilder& set_fill_mode(D3D12_FILL_MODE mode) { m_desc.FillMode = mode; return *this; };
	RasterizerDescBuilder& set_cull_mode(D3D12_CULL_MODE mode) { m_desc.CullMode = mode; return *this; };
	RasterizerDescBuilder& set_front_is_ccw(BOOL enabled) { m_desc.FrontCounterClockwise = enabled; return *this; };

	RasterizerDescBuilder& set_depth_clip_enabled(BOOL enabled) { m_desc.DepthClipEnable = enabled; return *this; };
	RasterizerDescBuilder& set_multisample_enabled(BOOL enabled) { m_desc.MultisampleEnable = enabled; return *this; };

	RasterizerDescBuilder& set_depth_bias(INT bias) { m_desc.DepthBias = bias; return *this; };
	RasterizerDescBuilder& set_depth_bias_clamp(FLOAT clamp) { m_desc.DepthBiasClamp = clamp; return *this; };
	RasterizerDescBuilder& set_slope_scaled_depth_bias(FLOAT bias) { m_desc.SlopeScaledDepthBias = bias; return *this; };
	RasterizerDescBuilder& set_aa_line_enabled(BOOL enabled) { m_desc.AntialiasedLineEnable = enabled; return *this; };
	RasterizerDescBuilder& set_forced_sample_count(UINT count) { m_desc.ForcedSampleCount = count; return *this; };
	RasterizerDescBuilder& set_conservative_raster_mode(D3D12_CONSERVATIVE_RASTERIZATION_MODE mode) { m_desc.ConservativeRaster = mode; return *this; };

private:
	friend class PipelineBuilder;
	D3D12_RASTERIZER_DESC& build() { return m_desc; };

private:
	D3D12_RASTERIZER_DESC m_desc;
};

class PipelineBuilder
{
private:
	D3D12_SHADER_BYTECODE fill_bytecode(const std::vector<uint8_t>& blob)
	{
		D3D12_SHADER_BYTECODE bc{};
		bc.BytecodeLength = blob.size();
		bc.pShaderBytecode = blob.data();
		return bc;
	}
public:
	PipelineBuilder()
	{
		// Set some defaults
		auto rsBuilder = RasterizerDescBuilder();
		auto bsBuilder = BlendDescBuilder();
		auto dsBuilder = DepthStencilDescBuilder();

		m_rasterizer = rsBuilder.build();
		m_blend = bsBuilder.build();
		m_depth_stencil = dsBuilder.build();
	}

	// Owns rootsig, since it needs to be alive until build
	PipelineBuilder& set_root_sig(cptr<ID3D12RootSignature> rootSig) { m_root_sig = rootSig; return *this; };

	// Owns blob, since it needs to be alive until build
	PipelineBuilder& set_shader_bytecode(const std::vector<uint8_t>& blob, ShaderType type)
	{
		switch (type)
		{
		case ShaderType::eVertex:
			m_vs = blob;
			break;
		case ShaderType::eHull:
			m_hs = blob;
			break;
		case ShaderType::eDomain:
			m_ds = blob;
			break;
		case ShaderType::eGeometry:
			m_gs = blob;
			break;
		case ShaderType::ePixel:
			m_ps = blob;
			break;
		default:
			throw std::runtime_error("Invalid shader type");
		}
		return *this;
	}

	PipelineBuilder& set_input_layout(InputLayoutBuilder& inputLayoutBuilder) { m_input_layout = inputLayoutBuilder.build(); return *this; return *this; }

	PipelineBuilder& set_blend(BlendDescBuilder& blendDescBuilder) { m_blend = blendDescBuilder.build(); return *this; }

	PipelineBuilder& set_rasterizer(RasterizerDescBuilder& rasterizerBuilder) { m_rasterizer = rasterizerBuilder.build(); return *this; }

	PipelineBuilder& set_depth_stencil(DepthStencilDescBuilder& dsBuilder) { m_depth_stencil = dsBuilder.build(); return *this; }

	// Ordering matters
	PipelineBuilder& append_rt_format(DXGI_FORMAT format) { m_rtv_formats.push_back(format); return *this; }

	PipelineBuilder& set_depth_format(DepthFormat format)
	{
		switch (format)
		{
		case DepthFormat::eD32:
			m_dsv_format = DXGI_FORMAT_D32_FLOAT;				// Fully qualififies the format: DXGI_FORMAT_R32_TYPELESS
			break;
		case DepthFormat::eD32_S8:
			m_dsv_format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;		// Fully qualififies the format: DXGI_FORMAT_R32G8X24_TYPELESS
			break;
		case DepthFormat::eD24_S8:
			m_dsv_format = DXGI_FORMAT_D24_UNORM_S8_UINT;		// Fully qualififies the format: DXGI_FORMAT_R24G8_TYPELESS
			break;
		case DepthFormat::eD16:
			m_dsv_format = DXGI_FORMAT_D16_UNORM;				// Fully qualififies the format: DXGI_FORMAT_R16_TYPELESS
			break;
		default:
			throw std::runtime_error(DET_ERR("Invalid depth format"));
		}
		return *this;
	}

	PipelineBuilder& set_topology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology) { m_primitive_topology = topology; return *this; }

	PipelineBuilder& set_multisamples(UINT count, UINT quality) { m_sample_desc = { count, quality }; return *this; }

	cptr<ID3D12PipelineState> build(ID3D12Device* dev)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pDesc{};

		if (!m_root_sig)
			throw std::runtime_error(DET_ERR("No rootsig. found!"));

		if (m_vs.empty() || m_ps.empty())
			throw std::runtime_error(DET_ERR("Pipeline requires at least VS and PS"));

		pDesc.pRootSignature = m_root_sig.Get();

		pDesc.VS = fill_bytecode(m_vs);
		pDesc.PS = fill_bytecode(m_ps);
		if (!m_gs.empty()) pDesc.GS = fill_bytecode(m_gs);
		if (!m_hs.empty()) pDesc.HS = fill_bytecode(m_hs);
		if (!m_ds.empty()) pDesc.DS = fill_bytecode(m_ds);

		pDesc.StreamOutput = m_stream_out;
		pDesc.BlendState = m_blend;
		pDesc.SampleMask = m_sample_mask;
		pDesc.RasterizerState = m_rasterizer;
		pDesc.DepthStencilState = m_depth_stencil;
		pDesc.InputLayout = m_input_layout;
		pDesc.IBStripCutValue = m_ib_strip_cut_val;
		pDesc.PrimitiveTopologyType = m_primitive_topology;
		pDesc.NumRenderTargets = (UINT)m_rtv_formats.size();
		for (int i = 0; i < m_rtv_formats.size(); ++i)
		{
			pDesc.RTVFormats[i] = m_rtv_formats[i];
		}
		pDesc.DSVFormat = m_dsv_format;
		pDesc.SampleDesc = m_sample_desc;
		pDesc.NodeMask = m_nodeMask;
		pDesc.CachedPSO = m_cached_pso;
		pDesc.Flags = m_flags;

		cptr<ID3D12PipelineState> pipe;
		ThrowIfFailed(dev->CreateGraphicsPipelineState(&pDesc, IID_PPV_ARGS(pipe.GetAddressOf())), DET_ERR("Failed to create graphics PSO"));
		return pipe;
	}

private:
	cptr<ID3D12RootSignature> m_root_sig = nullptr;			// Builder
	std::vector<uint8_t> m_vs, m_ps, m_ds, m_hs, m_gs;			// Handled
	D3D12_RASTERIZER_DESC m_rasterizer{};						// Handled
	D3D12_DEPTH_STENCIL_DESC m_depth_stencil{};				// Handled
	D3D12_BLEND_DESC m_blend{};								// Builder
	D3D12_INPUT_LAYOUT_DESC m_input_layout{};					// Builder
	UINT m_sample_mask = UINT_MAX;							// Default
	D3D12_STREAM_OUTPUT_DESC m_stream_out{};					// Skipping for now

	// Defaults
	D3D12_INDEX_BUFFER_STRIP_CUT_VALUE m_ib_strip_cut_val = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE m_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	std::vector<DXGI_FORMAT> m_rtv_formats;					// Handled
	DXGI_FORMAT m_dsv_format = DXGI_FORMAT_UNKNOWN;			// Handled
	DXGI_SAMPLE_DESC m_sample_desc{ 1, 0 };					// No MSAA by default

	UINT m_nodeMask = 0;									// Default
	D3D12_CACHED_PIPELINE_STATE m_cached_pso{};				// Skipping for now (no cached PSOs)
	D3D12_PIPELINE_STATE_FLAGS m_flags = D3D12_PIPELINE_STATE_FLAG_NONE;	// No special flags


};

