#include "pch.h"
#include "Camera/FPPCamera.h"
#include "DepthDefines.h"

FPPCamera::FPPCamera(float fov_deg, float aspect_ratio, float near_plane, float far_plane)
{
	// Create persp mat
#ifdef REVERSE_Z_DEPTH
	m_proj_mat = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(fov_deg), aspect_ratio, far_plane, near_plane);
#else
	m_proj_mat = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(fov_deg), aspect_ratio, near_plane, far_plane);
#endif

}

void FPPCamera::update_orientation(float mouse_x_delta, float mouse_y_delta, float dt)
{
	/*
		Given h = 1

		Y
		|		  /|
		|		 / |
		|		/  |
		|	   /   |
		|	  /    |
		|	 /     | sin(a)
		|	/	   |
		|  /	   |
		| /\	   |
		|/ a)      |
		----------------- XZ
			cos(a)

		Forward.X = cos(a)
		Forward.Y = sin(a)
		Forward.Z = cos(a)

		Z
		|		  /|
		|		 / |
		|		/  |
		|	   /   |
		|	  /    |
		|	 /     | sin(b)
		|	/	   |
		|  /	   |
		| /\	   |
		|/ b)      |
		----------------- X
			cos(b)

		Forward.X = cos(b)

		Forward.Z = sin(b)

		-------------------------
		Put together all contribution:

		Forward.X = cos(a) * cos(b)
		Forward.Y = sin(a)
		Forward.Z = cos(a) * sin(b)

		--------------------------
		Pitch (positive rotation around X using RH rule)
		Yaw (positive rotation around Y using LH rule)

		a = Pitch
		b = Yaw

		But note that when Yaw (b) = 0, we will point towards the X direction.
		We can set it to Yaw (b) = 90 to point it towards Z by default.

		Additionally, when changing Yaw, the default rotation around Y uses RH rule as
		seen on the second triangle. We can instead decrement the the Yaw to have rotation
		around Y according to LH rule.

		--------------------------

	*/

	m_yaw -= mouse_x_delta * m_sensitivity;

	// Delta Y is positive downwards by default, therefore we add negative of Y delta to pitch
	// (Positive pitch is rotation around X/Z using RH rule)
	m_pitch += -mouse_y_delta * m_sensitivity;	

	// Restrict 
	if (m_pitch > 89.f)
		m_pitch = 89.f;
	if (m_pitch < -89.f)
		m_pitch = -89.f;

	// Wrap 
	if (m_yaw > 360.f)
		m_yaw = 0.f;
	if (m_yaw < -360.f)
		m_yaw = 0.f;
	
	// Forward Direction
	m_lookat_dir.x = cos(DirectX::XMConvertToRadians(m_pitch)) * cos(DirectX::XMConvertToRadians(m_yaw));
	m_lookat_dir.y = sin(DirectX::XMConvertToRadians(m_pitch));
	m_lookat_dir.z = cos(DirectX::XMConvertToRadians(m_pitch)) * sin(DirectX::XMConvertToRadians(m_yaw));
	m_lookat_dir.Normalize();

	// Up direction
	m_up_dir.x = cos(DirectX::XMConvertToRadians(m_pitch + 90.f)) * cos(DirectX::XMConvertToRadians(m_yaw));
	m_up_dir.y = sin(DirectX::XMConvertToRadians(m_pitch + 90.f));
	m_up_dir.z = cos(DirectX::XMConvertToRadians(m_pitch + 90.f)) * sin(DirectX::XMConvertToRadians(m_yaw));
	m_up_dir.Normalize();

	// Right direction
	m_right_dir.x = cos(DirectX::XMConvertToRadians(m_pitch)) * cos(DirectX::XMConvertToRadians(m_yaw - s_yaw_offset));
	//m_right_dir.y = sin(DirectX::XMConvertToRadians(m_pitch));
	m_right_dir.y = 0.f;		// Disable y contrib 
	m_right_dir.z = cos(DirectX::XMConvertToRadians(m_pitch)) * sin(DirectX::XMConvertToRadians(m_yaw - s_yaw_offset));
	m_right_dir.Normalize();
	
	// Compute look at pos according to direction
	m_lookat_pos = m_position + m_lookat_dir;

	//m_view_mat = DirectX::XMMatrixLookAtLH(
	//	m_position,
	//	m_lookat_pos,
	//	s_world_up);
}


void FPPCamera::update_position(float right_fac, float up_fac, float forward_fac, float dt)
{
	assert(right_fac <= 1.f && right_fac >= -1.f);
	assert(up_fac <= 1.f && up_fac >= -1.f);
	assert(forward_fac <= 1.f && forward_fac >= -1.f);

	auto sideway_contrib = right_fac * m_right_dir;
	auto vert_contrib = up_fac * m_up_dir;
	auto frontback_contrib = forward_fac * m_lookat_dir;

	auto total_contrib = sideway_contrib + frontback_contrib + vert_contrib;
	total_contrib.Normalize();

	m_position += total_contrib * m_move_speed * dt;

	// Adjust lookat pos using new position
	m_lookat_pos = m_position + m_lookat_dir;

	//m_view_mat = DirectX::XMMatrixLookAtLH(
	//	m_position,
	//	m_lookat_pos,
	//	s_world_up);
}

void FPPCamera::update_matrices()
{
	m_view_mat = DirectX::XMMatrixLookAtLH(
		m_position,
		m_lookat_pos,
		s_world_up);
}

void FPPCamera::set_speed(float speed)
{
	m_move_speed = speed;
}

void FPPCamera::set_sensitivity(float sens)
{
	m_sensitivity = sens;
}

void FPPCamera::set_yaw(float yaw)
{
	m_yaw = yaw;
}

void FPPCamera::set_pitch(float pitch)
{
	m_pitch = pitch;
}

float FPPCamera::get_yaw()
{
	return m_yaw;;
}

float FPPCamera::get_pitch()
{
	return m_pitch;
}


