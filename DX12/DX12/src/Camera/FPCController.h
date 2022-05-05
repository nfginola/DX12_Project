#pragma once
class FPCController
{
public:
	FPCController(class Input* input, class GUIContext* gui_ctx);
	~FPCController() = default;

	/*
		May be replaced with FPCamera in the future where FP-Perspective (FPP) inherits from FP
	*/
	void set_camera(class FPPCamera* cam);
	void set_secondary_camera(FPPCamera* cam);

	class Camera* get_active_camera();

	void update(float dt);

private:	
	FPPCamera* m_cam_2;
	FPPCamera* m_cam;
	Input* m_input;

	// State to keep track of the direction controller
	float m_fwd_state = 0.f;
	float m_up_state = 0.f;
	float m_right_state = 0.f;

	// Base speed
	float m_init_speed = 15.f;
	float m_curr_speed = m_init_speed;
	float m_min_speed = 1.f;
	float m_max_speed = 28.f;

	// Sensitivity
	float m_mouse_sens = 0.07f;
	



};

