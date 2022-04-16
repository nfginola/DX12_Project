#pragma once
#include <windows.h>
#include <queue>

// DirectXTK
#include <DXTK/Keyboard.h>
#include <DXTK/Mouse.h>

using Keys = DirectX::Keyboard::Keys;

enum class MouseMode
{
	Absolute,
	Relative
};

class Input
{
public:
	Input(HWND hwnd);
	~Input();

	// For systems to call to prepare input for use
	void process_keyboard(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void process_mouse(UINT uMsg, WPARAM wParam, LPARAM lParam);

	// Use in frame loop
	void begin();	// Called at the beginning of every frame
	void end();		// Called at the end of every frame

	// Mouse
	void set_mouse_mode(MouseMode mode);

	bool lmb_pressed() const;
	bool lmb_released() const;
	bool lmb_down() const;

	bool rmb_pressed() const;
	bool rmb_released() const;
	bool rmb_down() const;

	bool mmb_pressed() const;
	bool mmb_released() const;
	bool mmb_down() const;

	int get_scroll_value() const;
	std::pair<int, int> get_mouse_position() const;
	const std::pair<int, int>& get_mouse_delta() const;

	// Keyboard
	bool key_pressed(Keys key) const;
	bool key_released(Keys key) const;
	bool key_down(Keys key) const;

private:
	void init_mouse(DirectX::Mouse::Mode mode);
	void set_mouse_mode(DirectX::Mouse::Mode mode);
	void restore_prev_mouse_mode();

	// Cursor hidden automatically in Relative mode and shown automatically in Absolute mode
	void hide_cursor() const;
	void show_cursor() const;

	// Cursor centered automatically in Relative mode and uncentered automatically in Absolute mode
	void center_cursor();
	void uncenter_cursor();

private:
	static LPBYTE s_raw_input_data;
	static uint32_t s_raw_input_data_size;
	HWND m_hwnd;

	std::unique_ptr<DirectX::Keyboard> m_kb;
	std::unique_ptr<DirectX::Mouse> m_mouse;

	// Mouse helper
	DirectX::Mouse::State m_mouse_state;
	DirectX::Mouse::ButtonStateTracker m_mouse_tracker;
	DirectX::Mouse::Mode m_mouse_mode_prev;
	DirectX::Mouse::Mode m_mouse_mode_curr;
	std::pair<int, int> m_screen_pos_prev;
	std::pair<int, int> m_screen_pos_curr;
	std::pair<int, int> m_mouse_dt;
	bool m_cursor_centered;

	// Delta accumulator for this frame
	std::queue<std::pair<int, int>> m_mouse_dts_this_frame;

	// Keyboard helper
	DirectX::Keyboard::State m_kb_state;
	DirectX::Keyboard::KeyboardStateTracker m_kb_tracker;

};


