#pragma once

class Camera
{
public:
	virtual ~Camera();
	
	void set_position(float x, float y, float z);
	void set_position(const DirectX::SimpleMath::Vector4& pos);
	void set_orientation();

	void set_lookat_pos(const DirectX::SimpleMath::Vector4& lookat);

	const DirectX::SimpleMath::Vector4& get_position() const;
	const DirectX::SimpleMath::Vector4& get_lookat() const;
	virtual const DirectX::SimpleMath::Matrix& get_view_mat() const;
	virtual const DirectX::SimpleMath::Matrix& get_proj_mat() const;

protected:
	Camera();

	// LH system
	static constexpr DirectX::SimpleMath::Vector4 s_world_up = { 0.f, 1.f, 0.f, 0.f };
	static constexpr DirectX::SimpleMath::Vector4 s_world_right = { 1.f, 0.f, 0.f, 0.f };
	static constexpr DirectX::SimpleMath::Vector4 s_world_forward = { 0.f, 0.f, 1.f, 0.f };

	DirectX::SimpleMath::Vector4 m_position;
	DirectX::SimpleMath::Matrix m_view_mat, m_proj_mat;

	DirectX::SimpleMath::Vector4 m_lookat_pos = DirectX::SimpleMath::Vector4(0.f, 0.f, 5000.f, 1.f);



};

