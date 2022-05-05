#pragma once
#include "Camera.h"

class FPPCamera final : public Camera
{
public:
	FPPCamera(float fov_deg, float aspect_ratio, float near_plane = 0.1f, float far_plane = 100.f);
	~FPPCamera() = default;

	void update_orientation(float mouse_x_delta, float mouse_y_delta, float dt);

	/*
		Right:		right_fac = 1
		Left:		right_fac = -1
		Up:			up_fac = 1
		Down:		up_fac = -1
		Forward:	forward_fac = 1
		Backward:	forward_fac = -1

		This function is meant to be called once during a frame.
		Meaning you are responsible for supplying in which direction
		the camera should be moving this frame on function call.
	*/
	void update_position(float right_fac, float up_fac, float forward_fac, float dt);

	void update_matrices();

	void set_speed(float speed);
	void set_sensitivity(float sens);

	void set_yaw(float yaw);
	void set_pitch(float pitch);

	float get_yaw();
	float get_pitch();

private:
	// Used to offset yaw so the default viewing point is in forward Z.
	static constexpr float s_yaw_offset = 90.f;

	// local in world
	DirectX::SimpleMath::Vector4 m_lookat_dir = s_world_forward;
	DirectX::SimpleMath::Vector4 m_right_dir = s_world_right;
	DirectX::SimpleMath::Vector4 m_up_dir = s_world_up;

	float m_sensitivity = 12.3f;
	float m_move_speed = 7.f;
	float m_yaw = s_yaw_offset;
	float m_pitch = 0.f;

};

