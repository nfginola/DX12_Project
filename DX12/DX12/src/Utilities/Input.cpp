#include "pch.h"
#include "Input.h"

LPBYTE Input::s_raw_input_data = nullptr;
uint32_t Input::s_raw_input_data_size = 0;

Input::Input(HWND hwnd) :
	m_hwnd(hwnd),
	m_screen_pos_prev({ 0, 0 }),
	m_screen_pos_curr({ 0, 0 }),
	m_mouse_dt({ 0, 0 }),
	m_cursor_centered(false)
{
	m_kb = std::make_unique<DirectX::Keyboard>();
	m_mouse = std::make_unique<DirectX::Mouse>();
	m_mouse->SetWindow(hwnd);

	init_mouse(DirectX::Mouse::Mode::MODE_ABSOLUTE);
}

Input::~Input()
{
	delete[] s_raw_input_data;
}

void Input::process_keyboard(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DirectX::Keyboard::ProcessMessage(uMsg, wParam, lParam);
}

void Input::process_mouse(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DirectX::Mouse::ProcessMessage(uMsg, wParam, lParam);

	switch (uMsg)
	{

	case WM_MOUSEMOVE:
		// abs position
		break;
	case WM_INPUT:
	{
		UINT dwSize = 0;

		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
		if (s_raw_input_data == nullptr)
		{
			s_raw_input_data = new BYTE[dwSize];
			s_raw_input_data_size = dwSize;
			//std::cout << "raw input buffer init with size " << dwSize << "\n";
		}
		else if (dwSize > s_raw_input_data_size)
		{
			// not enough memory --> expand
			delete[] s_raw_input_data;
			s_raw_input_data = new BYTE[dwSize];
			s_raw_input_data_size = dwSize;
			//std::cout << "raw input buffer resize\n";
		}

		if (s_raw_input_data == NULL)
		{
			assert(false);
		}

		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, s_raw_input_data, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
		{
			assert(false);
			OutputDebugString(TEXT("GetRawInputData does not return correct size !\n"));
		}

		RAWINPUT* raw = (RAWINPUT*)s_raw_input_data;

		if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			// We grab all the raw dt WMs that occured between last frames PumpMessage and this frames PumpMessage!
			// We used to grab the last dt, which is incorrect! (We would be ignoring a bunch of movement)
			m_mouse_dts_this_frame.push({ raw->data.mouse.lLastX, raw->data.mouse.lLastY });
		}
		break;
	}
	default:
		break;
	}
}

void Input::frame_begin()
{
	// We cant update this on process_mouse. Otherwise some functionality dont work properly (e.g PRESSED)
	m_mouse_state = m_mouse->GetState();
	m_mouse_tracker.Update(m_mouse_state);

	m_kb_state = m_kb->GetState();
	m_kb_tracker.Update(m_kb_state);

	// Track position only in absolute mode
	if (m_mouse_mode_curr == DirectX::Mouse::Mode::MODE_ABSOLUTE)
	{
		m_screen_pos_curr = { m_mouse_state.x, m_mouse_state.y };
		m_screen_pos_prev = m_screen_pos_curr;
	}

	//std::cout << m_mouse_dts_this_frame.size() << std::endl;
	// Calculate total mouse dt for this frame
	while (!m_mouse_dts_this_frame.empty())
	{
		const auto& d = m_mouse_dts_this_frame.front();
		m_mouse_dts_this_frame.pop();

		m_mouse_dt.first += d.first;
		m_mouse_dt.second += d.second;
	}
}

void Input::frame_end()
{
	// reset dt
	/*
		There is no WM for "no input", so we need to do this at the end of the frame once.

		NOTE:

		The only time we should be using "Getdt" is if it was computed through manual position absolute dt,
		which does have full move information (unlike downsampling dts from raw input WMs)
	*/
	m_mouse_dt = { 0, 0 };
}

void Input::set_mouse_mode(MouseMode mode)
{
	switch (mode)
	{
	case MouseMode::Relative:
		set_mouse_mode(DirectX::Mouse::Mode::MODE_RELATIVE);

		center_cursor();
		break;
	case MouseMode::Absolute:
		set_mouse_mode(DirectX::Mouse::Mode::MODE_ABSOLUTE);

		// Restore cursor location
		SetCursorPos(m_screen_pos_curr.first, m_screen_pos_curr.second);
		uncenter_cursor();
		break;
	default:
		assert(false);
	}
}

void Input::hide_cursor() const
{
	m_mouse->SetVisible(false);
}

void Input::show_cursor() const
{
	m_mouse->SetVisible(true);
}


void Input::center_cursor()
{
	// We cant explicitly interfere with the cursor position since we are using mouse absolute mode to calculate dt manually
	//RECT rect{};
	//GetWindowRect(m_hwnd, &rect);
	//SetCursorPos((rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2);

	m_cursor_centered = true;
}

void Input::uncenter_cursor()
{
	m_cursor_centered = false;
}



void Input::init_mouse(DirectX::Mouse::Mode mode)
{
	m_mouse_mode_curr = mode;
	set_mouse_mode(mode);
}

bool Input::lmb_pressed() const
{
	return m_mouse_tracker.leftButton == DirectX::Mouse::ButtonStateTracker::ButtonState::PRESSED;
}
bool Input::lmb_released() const
{
	return m_mouse_tracker.leftButton == DirectX::Mouse::ButtonStateTracker::ButtonState::RELEASED;
}
bool Input::lmb_down() const
{
	return m_mouse_tracker.leftButton == DirectX::Mouse::ButtonStateTracker::ButtonState::HELD;
}

bool Input::rmb_pressed() const
{
	return m_mouse_tracker.rightButton == DirectX::Mouse::ButtonStateTracker::ButtonState::PRESSED;
}
bool Input::rmb_released() const
{
	return m_mouse_tracker.rightButton == DirectX::Mouse::ButtonStateTracker::ButtonState::RELEASED;
}
bool Input::rmb_down() const
{
	return m_mouse_tracker.rightButton == DirectX::Mouse::ButtonStateTracker::ButtonState::HELD;
}

bool Input::mmb_pressed() const
{
	return m_mouse_tracker.middleButton == DirectX::Mouse::ButtonStateTracker::ButtonState::PRESSED;
}
bool Input::mmb_released() const
{
	return m_mouse_tracker.middleButton == DirectX::Mouse::ButtonStateTracker::ButtonState::RELEASED;
}
bool Input::mmb_down() const
{
	return m_mouse_tracker.middleButton == DirectX::Mouse::ButtonStateTracker::ButtonState::HELD;
}

int Input::get_scroll_value() const
{
	return m_mouse_state.scrollWheelValue;
}
std::pair<int, int> Input::get_mouse_position() const
{
	if (m_cursor_centered)
	{
		RECT rect{};
		GetWindowRect(m_hwnd, &rect);
		return { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
	}
	else
	{
		return m_screen_pos_curr;
	}
}
const std::pair<int, int>& Input::get_mouse_delta() const
{
	return m_mouse_dt;
}

bool Input::key_pressed(Keys key) const
{
	return m_kb_tracker.IsKeyPressed(key);
}
bool Input::key_released(Keys key) const
{
	return m_kb_tracker.IsKeyReleased(key);
}
bool Input::key_down(Keys key) const
{
	return m_kb_state.IsKeyDown(key);
}

void Input::set_mouse_mode(DirectX::Mouse::Mode mode)
{
	m_mouse_mode_prev = m_mouse_mode_curr;
	m_mouse_mode_curr = mode;
	m_mouse->SetMode(mode);
}
void Input::restore_prev_mouse_mode()
{
	m_mouse_mode_curr = m_mouse_mode_prev;
	m_mouse->SetMode(m_mouse_mode_prev);
}

