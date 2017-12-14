// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include "render.hpp"
// #include "render2.hpp"
#include "window.hpp"

#include <ny/backend.hpp>
#include <ny/appContext.hpp>
#include <ny/key.hpp>
#include <vpp/instance.hpp>
#include <vpp/debug.hpp>
#include <dlg/dlg.hpp>

#include <chrono>

// settings
constexpr bool useValidation = true;
constexpr auto startMsaa = vk::SampleCountBits::e1;
constexpr auto layerName = "VK_LAYER_LUNARG_standard_validation";
constexpr auto printFrames = true;

int main()
{
	// - initialization -
	auto& backend = ny::Backend::choose();
	if(!backend.vulkan()) {
		throw std::runtime_error("ny backend has no vulkan support!");
	}

	auto appContext = backend.createAppContext();

	// vulkan init
	// instance
	auto iniExtensions = appContext->vulkanExtensions();
	iniExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	vk::ApplicationInfo appInfo ("vpp-app", 1, "vpp", 1, VK_API_VERSION_1_0);
	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.pApplicationInfo = &appInfo;

	instanceInfo.enabledExtensionCount = iniExtensions.size();
	instanceInfo.ppEnabledExtensionNames = iniExtensions.data();

	if(useValidation) {
		instanceInfo.enabledLayerCount = 1;
		instanceInfo.ppEnabledLayerNames = &layerName;
	}

	vpp::Instance instance {};
	try {
		instance = {instanceInfo};
		if(!instance.vkInstance()) {
			throw std::runtime_error("vkCreateInstance returned a nullptr");
		}
	} catch(const std::exception& error) {
		dlg_error("Vulkan instance creation failed: {}", error.what());
		dlg_error("\tThis may indicate that your system that does support vulkan");
		dlg_error("\tThis application requires vulkan to work; rethrowing");
		throw;
	}

	// debug callback
	std::unique_ptr<vpp::DebugCallback> debugCallback;
	if(useValidation) {
		debugCallback = std::make_unique<vpp::DebugCallback>(instance);
	}

	// init ny window
	MainWindow window(*appContext, instance);
	auto vkSurf = window.vkSurface();

	const vpp::Queue* presentQueue {};
	auto device = vpp::Device(instance, vkSurf, presentQueue);
	auto renderer = Renderer(device, vkSurf, window.size(),
		startMsaa, *presentQueue);

	// - events - 
	bool run = true;
	bool sPressed = true;
	bool play = false;

	window.onClose = [&](const auto&) { run = false; };
	window.onKey = [&](const auto& ev) { 
		if(ev.pressed && ev.keycode == ny::Keycode::escape) {
			dlg_info("Escape pressed, exiting");
			run = false;
		} else if(ev.pressed && ev.keycode == ny::Keycode::s) {
			sPressed = true;
		} else if(ev.pressed && ev.keycode == ny::Keycode::p) {
			play = !play;
		}
	};
	window.onResize = [&](const auto& ev) {
		renderer.resize(ev.size);
		sPressed = true;
	};

	// - main loop -
	using Clock = std::chrono::high_resolution_clock;
	using Secf = std::chrono::duration<float, std::ratio<1, 1>>;

	auto lastFrame = Clock::now();
	auto fpsCounter = 0u;
	auto secCounter = 0.f;

	while(run) {
		bool ret = play ? appContext->pollEvents() : appContext->waitEvents();
		if(!ret) {
			dlg_info("pollEvents returned false");
			return 0;
		}

		if(sPressed || play) {
			renderer.renderBlock();
			sPressed = false;
		}

		if(printFrames) {
			auto now = Clock::now();
			auto diff = now - lastFrame;
			auto deltaCount = std::chrono::duration_cast<Secf>(diff).count();
			lastFrame = now;

			++fpsCounter;
			secCounter += deltaCount;
			if(secCounter >= 1.f) {
				dlg_info("{} fps", fpsCounter);
				secCounter = 0.f;
				fpsCounter = 0;
			}
		}
	}
}