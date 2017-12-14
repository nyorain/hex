// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <vpp/device.hpp> // vpp::Device
#include <vpp/queue.hpp> // vpp::Queue
#include <vpp/buffer.hpp> // vpp::Buffer
#include <vpp/pipeline.hpp> // vpp::Pipeline
#include <vpp/renderer.hpp> // vpp::DefaultRenderer
#include <vpp/descriptor.hpp> // vpp::DescriptorSet
#include <vpp/commandBuffer.hpp>
#include <vpp/renderPass.hpp>
#include <vpp/framebuffer.hpp>
#include <vpp/sync.hpp>
#include <vpp/queue.hpp>
#include <vpp/vk.hpp>
#include <nytl/vec.hpp>

class Engine;

class Renderer : public vpp::DefaultRenderer {
public:
	Renderer(const vpp::Device&, vk::SurfaceKHR, nytl::Vec2ui size,
		vk::SampleCountBits samples, const vpp::Queue& present);
	~Renderer() = default;

	void resize(nytl::Vec2ui size);
	void samples(vk::SampleCountBits);

protected:
	void createMultisampleTarget(const vk::Extent2D& size);
	void record(const RenderBuffer&) override;
	void initBuffers(const vk::Extent2D&, nytl::Span<RenderBuffer>) override;

	void createGraphicsPipeline(const vpp::Device& device);

protected:
	vpp::ViewableImage multisampleTarget_;
	vpp::RenderPass renderPass_;
	vk::SampleCountBits sampleCount_;
	vk::SwapchainCreateInfoKHR scInfo_;

	vpp::Pipeline gfxPipeline_;
	vpp::Pipeline linePipeline_;
	vpp::PipelineLayout gfxPipelineLayout_;

	vpp::Buffer storage_;
	vpp::Pipeline compPipeline_;
	vpp::PipelineLayout compPipelineLayout_;
	vpp::DescriptorSetLayout compDsLayout_;
	vpp::DescriptorPool dsPool_;
	vpp::DescriptorSet compDs_;
};