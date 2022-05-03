#pragma once

#include <stdint.h>
#include <stdarg.h>

#include "common.hpp"
#include "maths.hpp"

namespace vkr {
	/* Basic logging. */
	VKR_API void error(const char* fmt, ...);
	VKR_API void warning(const char* fmt, ...);
	VKR_API void info(const char* fmt, ...);
	VKR_API void abort_with(const char* fmt, ...);
	VKR_API void verror(const char* fmt, va_list args);
	VKR_API void vwarning(const char* fmt, va_list args);
	VKR_API void vinfo(const char* fmt, va_list args);
	VKR_API void vabort_with(const char* fmt, va_list args);

	/* These structures are for storing information about the
	 * state of a class wrapper that might rely on external headers,
	 * namely glfw3.h and vulkan.h. */
	struct impl_App;
	struct impl_VideoContext;

	/* Class forward declarations. */
	class App;
	class VideoContext;

	/* To be inherited by client applications to provide custom
	 * functionality and data. */
	class VKR_API App {
	private:
		impl_App* handle;

		VideoContext* video;

		const char* title;
		v2i size;

		bool create_window_surface(const VideoContext& ctx) const;

		friend class VideoContext;
	public:
		App(const char* title, v2i size);

		virtual void on_init() = 0;
		virtual void on_update(f64) = 0;
		virtual void on_deinit() = 0;

		virtual ~App() {};

		void run();
	};

	class VKR_API VideoContext {
	private:
		bool validation_layers_supported();
	public:
		impl_VideoContext* handle;

		VideoContext(const App& app, const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions);
		~VideoContext();
	};
};
