#include <types.hpp>
#include <vulkan/vulkan.h>

#include <vector>

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

struct window_t {
	HWND handle;
};

#elif defined(__APPLE__)

#include <Cocoa/Cocoa.h>

#elif defined(__linux__) || defined(__unix__) || defined(__posix)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <vulkan/vulkan_xlib.h>

struct window_t {
	u32 width;
	u32 height;

	Display* display;
	Window handle;

	VkSurfaceKHR surface;
};

window_t* createWindow(const char* title, u16 width, u16 height) {
	Display* display = XOpenDisplay(nullptr);
	if (display == nullptr) {
		return nullptr;
	}

	int screen = DefaultScreen(display);
	Window handle = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, width, height, 0, BlackPixel(display, screen), WhitePixel(display, screen));
	XSelectInput(display, handle, ExposureMask | KeyPressMask);
	XMapWindow(display, handle);

	XStoreName(display, handle, title);

	Atom deleteMsg = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, handle, &deleteMsg, 1);

	window_t* window = new window_t;
	window->width = width;
	window->height = height;
	window->display = display;
	window->handle = handle;
	return window;
}

void destroyWindow(window_t* window) {
	XDestroyWindow(window->display, window->handle);
	XCloseDisplay(window->display);
	delete window;
}

bool processEvents(window_t* window) {
	XEvent event;
	while (XPending(window->display) > 0) {
		XNextEvent(window->display, &event);
		switch (event.type) {
			case Expose:
				break;
			case KeyPress:
				break;
			case ClientMessage:
				if (event.xclient.data.l[0] == XInternAtom(window->display, "WM_DELETE_WINDOW", False)) {
					return false;
				}
				break;
			case ConfigureNotify:
				if (event.xconfigure.width != window->width || event.xconfigure.height != window->height) {
					window->width = event.xconfigure.width;
					window->height = event.xconfigure.height;
				}
				break;
		}
	}

	return true;
}

void appendWindowExtensions(std::vector<const char*>& extensions) {
	extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
}

VkSurfaceKHR createSurface(VkInstance instance, window_t* window) {
	VkXlibSurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	createInfo.dpy = window->display;
	createInfo.window = window->handle;

	VkSurfaceKHR surface;
	if (vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	return surface;
}

#else
#error "Unsupported platform"
#endif