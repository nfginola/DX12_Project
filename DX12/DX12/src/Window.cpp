#include "pch.h"
#include "Window.h"

Window::Window(HINSTANCE hInstance, std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> winProc, int client_width, int client_height, const std::string& title) :
	m_hInstance(hInstance),
	m_hwnd(nullptr),
	m_title(title),
	m_client_width(client_width),
	m_client_height(client_height),
	m_alive(true),
	m_fullscreen(false),
	m_style(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX /* | WS_MAXIMIZEBOX | WS_SIZEBOX*/),
	m_ex_style(0),
	m_screenTransitionDetails(ScreenTransitionDetails{ 0, 0, 0, 0 }),
	m_custom_proc(winProc)
{
	m_id = get_next_id();

	register_window();
	make_window();
	fit_to_client_dim();

	assert(m_hwnd != NULL);
	ShowWindow(m_hwnd, SW_SHOWDEFAULT);
}

Window::~Window()
{
	DestroyWindow(m_hwnd);
	UnregisterClass(utils::to_wstr(m_id).c_str(), m_hInstance);
}

void Window::pump_messages() const
{
	MSG msg = { };
	/*
		If hWnd is NULL, PeekMessage retrieves messages for any window that belongs to the current thread,
		and any messages on the current thread's message queue whose hwnd value is NULL (see the MSG structure).
		Therefore if hWnd is NULL, both window messages and thread messages are processed.
		This is VERY important for ImGUI docking branch where we can have multiple windows!
	*/
	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

bool Window::is_alive() const
{
	return m_alive;
}

void Window::register_window()
{
	std::wstring classID = utils::to_wstr(m_id);

	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = window_proc;
	wc.hInstance = m_hInstance;
	wc.lpszClassName = classID.c_str();

	// https://stackoverflow.com/questions/4503506/cursor-style-doesnt-stay-updated
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);	// Standard cursor to load (Fixes bug where the sizing icon is stuck)

	RegisterClass(&wc);
}

void Window::make_window()
{
	auto class_id = utils::to_wstr(m_id);
	auto title = utils::to_wstr(m_title);

	m_hwnd = CreateWindowEx(
		0,								// Default behaviour (optionals)
		class_id.c_str(),				// Class name (identifier)
		title.c_str(),					// Window title 
		m_style,						// Window style 
		135,							// Window x-pos
		70,								// Window y-pos 

		// This will be resized to our desired client dimensions later
		CW_USEDEFAULT,					// Default width
		CW_USEDEFAULT,					// Default height

		NULL,							// Parent window (none, we are top level window)
		NULL,							// Menu
		m_hInstance,
		(LPVOID)this					// Passing this pointer so that we can use member function procedure handling
	);
	assert(m_hwnd != NULL);

	// Associate this window with the hwnd
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
}

void Window::fit_to_client_dim()
{
	// Store previous window position
	RECT old_rect{};
	GetWindowRect(m_hwnd, &old_rect);
	LONG prev_x = old_rect.left;
	LONG prev_y = old_rect.top;

	// We pass a rect with desired client dim
	// Rect is modified with dimensions for the Window that accomodates the client dimensions
	RECT desired_rect{};
	desired_rect.right = m_client_width;
	desired_rect.bottom = m_client_height;
	bool res = AdjustWindowRectEx(&desired_rect, m_style, false, m_ex_style);
	assert(res);

	LONG window_width = desired_rect.right - desired_rect.left;
	LONG window_height = desired_rect.bottom - desired_rect.top;
	res = SetWindowPos(m_hwnd, HWND_TOP, prev_x, prev_y, window_width, window_height, SWP_NOREPOSITION);
	assert(res);
}

void Window::set_fullscreen(bool fullscreenState)
{
	m_fullscreen = fullscreenState;
	int& prev_x = m_screenTransitionDetails.prev_x;
	int& prev_y = m_screenTransitionDetails.prev_y;
	int& prev_width = m_screenTransitionDetails.prev_width;
	int& prev_height = m_screenTransitionDetails.prev_height;

	if (!fullscreenState)	// Go to windowed mode
	{
		// Set back to windowed
		SetWindowLongPtr(m_hwnd, GWL_STYLE, m_style);
		SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, m_ex_style);

		ShowWindow(m_hwnd, SW_SHOWNORMAL);

		SetWindowPos(m_hwnd, HWND_TOP, prev_x, prev_y, prev_width, prev_height, SWP_NOZORDER | SWP_FRAMECHANGED);
		//AdjustWindowDimensions();	// No need to re-adjust w.r.t border, since prev width/height (previous windowed) already accounts for the border
	}
	else  // Go to fullscreen mode
	{
		// Save windowed window dimensions
		RECT old_rect{};
		GetWindowRect(m_hwnd, &old_rect);
		prev_x = old_rect.left;
		prev_y = old_rect.top;
		prev_width = old_rect.right - old_rect.left;
		prev_height = old_rect.bottom - old_rect.top;

		// Set fake fullscreen
		SetWindowLongPtr(m_hwnd, GWL_STYLE, WS_POPUP);
		SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);

		SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

		RECT curr_rect{};
		GetWindowRect(m_hwnd, &curr_rect);
		m_client_width = curr_rect.right - curr_rect.left;
		m_client_height = curr_rect.bottom - curr_rect.top;

		ShowWindow(m_hwnd, SW_SHOWMAXIMIZED);
	}
}

bool Window::is_fullscreen() const
{
	return m_fullscreen;
}

HWND Window::get_hwnd() const
{
	return m_hwnd;
}

int Window::get_client_width() const
{
	return m_client_width;
}

int Window::get_client_height() const
{
	return m_client_height;
}

void Window::resize_client(UINT width, UINT height)
{
	m_client_width = width;
	m_client_height = height;
}

std::string Window::get_next_id()
{
	static int id = 0;
	std::string winID = "WindowID" + std::to_string(id);
	++id;
	return winID;
}

LRESULT Window::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Window* active_win = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (active_win)
		return active_win->handle_proc(uMsg, wParam, lParam);
	else
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Window::handle_proc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Default behavior
	switch (uMsg)
	{
	case WM_CLOSE:
	{
		//DestroyWindow(m_hwnd);	// Window destructor cleans up hwnd
		m_alive = false;
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(m_hwnd, &ps);

		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
		EndPaint(m_hwnd, &ps);
		break;
	}
	}

	return m_custom_proc(m_hwnd, uMsg, wParam, lParam);
}

