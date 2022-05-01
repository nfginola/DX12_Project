We need to export symbols:

// https://devblogs.microsoft.com/directx/directx12agility/
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 602; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8"..\\..\\DX12\\vendor\\AgilitySDK\\"; }

note that the D3D12SDKPath is the path FROM the .exe!! (also note the need for the trailing slashes)
