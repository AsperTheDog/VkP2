#pragma once

#include "base.hpp"
#include "buffer.hpp"
#include "command_buffer.hpp"
#include "desc.hpp"
#include "device.hpp"
#include "image.hpp"
#include "instance.hpp"
#include "pipeline.hpp"
#include "shader.hpp"
#include "swapchain.hpp"
#include "sync.hpp"

#ifdef VKP2_INCLUDE_EXTRA
	#include "extra/window.hpp"
	#include "extra/signal.hpp"
	#include "extra/static_vector.hpp"
#endif

#ifdef VKP2_INCLUDE_DYN
	#ifdef VKP2_INCLUDE_EXTRA
		#include "dyn/extra/arena.hpp"
	#endif
	#include "dyn/barrier.hpp"
	#include "dyn/pipeline.hpp"
#endif
