// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include "render.hpp"

#include <nytl/mat.hpp>
#include <vpp/vk.hpp>
#include <vpp/util/file.hpp>
#include <vpp/renderPass.hpp>
#include <vpp/bufferOps.hpp>
#include <vpp/swapchain.hpp>

#include <dlg/dlg.hpp> // dlg

// shader data
#include <shaders/hex.vert.h>
#include <shaders/color.frag.h>
#include <shaders/hexLine.vert.h>
#include <shaders/black.frag.h>
#include <shaders/ant.comp.h>

constexpr std::int32_t mceil(float num) {
    std::int32_t inum = static_cast<std::int32_t>(num);
    if (num == static_cast<float>(inum)) {
        return inum;
    }
    return inum + (num > 0 ? 1 : 0);
}

constexpr auto hexWidth = 128;
constexpr unsigned int hexHeight = 157;

vpp::RenderPass createRenderPass(const vpp::Device&, vk::Format, 
	vk::SampleCountBits);

Renderer::Renderer(const vpp::Device& dev, vk::SurfaceKHR surface, 
	nytl::Vec2ui size, vk::SampleCountBits samples, const vpp::Queue& present)
{
	// pipeline
	sampleCount_ = samples;
	vpp::SwapchainPreferences prefs {};
	prefs.presentMode = vk::PresentModeKHR::fifo;
	scInfo_ = vpp::swapchainCreateInfo(dev, surface, {size[0], size[1]}, prefs);

	gfxPipelineLayout_ = {dev, {}, {}};
	renderPass_ = createRenderPass(dev, scInfo_.imageFormat, samples);
	createGraphicsPipeline(dev);

	// compute
	// pool 
	vk::DescriptorPoolSize typeCounts[1] {};
	typeCounts[0].type = vk::DescriptorType::storageBuffer;
	typeCounts[0].descriptorCount = 1;

	vk::DescriptorPoolCreateInfo descriptorPoolInfo;
	descriptorPoolInfo.poolSizeCount = 1;
	descriptorPoolInfo.pPoolSizes = typeCounts;
	descriptorPoolInfo.maxSets = 1;

	dsPool_ = {dev, descriptorPoolInfo};

	// layout
	auto bindings = {
		vpp::descriptorBinding(
			vk::DescriptorType::storageBuffer, 
			vk::ShaderStageBits::compute)
	};

	compDsLayout_ = {dev, bindings};
	compDs_ = {compDsLayout_, dsPool_};
	compPipelineLayout_ = {dev, {compDsLayout_}, {}};

	// pipeline
	auto computeShader = vpp::ShaderModule(dev, ant_comp_data);

	vk::ComputePipelineCreateInfo info;
	info.layout = compPipelineLayout_;
	info.stage.module = computeShader;
	info.stage.pName = "main";
	info.stage.stage = vk::ShaderStageBits::compute;

	vk::Pipeline vkPipeline;
	vk::createComputePipelines(dev, {}, 1, info, nullptr, vkPipeline);
	compPipeline_ = {dev, vkPipeline};

	// storage buffer for data
	auto mem = dev.memoryTypeBits(vk::MemoryPropertyBits::deviceLocal);
	auto bufSize = sizeof(int) * (2 + hexWidth * hexHeight);
	storage_ = {dev, {{}, bufSize,
			vk::BufferUsageBits::storageBuffer | 
			vk::BufferUsageBits::vertexBuffer | 
			vk::BufferUsageBits::transferDst |
			vk::BufferUsageBits::transferSrc},
		mem};
	std::vector<int> data(2 + hexWidth * hexHeight);
	data[0] = (hexHeight / 2) * hexWidth + hexWidth / 2;
	// data[0] = (hexHeight - 60) * hexWidth + hexWidth / 2;
	data[1] = 0;
	vpp::writeStaging430(storage_, vpp::raw(data));

	// update descriptor
	{
		vpp::DescriptorSetUpdate update(compDs_);
		update.storage({{storage_, 0, bufSize}});
	}

	// init renderer
	vpp::DefaultRenderer::init(renderPass_, scInfo_, present);
}

void Renderer::createMultisampleTarget(const vk::Extent2D& size)
{
	auto width = size.width;
	auto height = size.height;

	// img
	vk::ImageCreateInfo img;
	img.imageType = vk::ImageType::e2d;
	img.format = scInfo_.imageFormat;
	img.extent.width = width;
	img.extent.height = height;
	img.extent.depth = 1;
	img.mipLevels = 1;
	img.arrayLayers = 1;
	img.sharingMode = vk::SharingMode::exclusive;
	img.tiling = vk::ImageTiling::optimal;
	img.samples = sampleCount_;
	img.usage = vk::ImageUsageBits::transientAttachment | vk::ImageUsageBits::colorAttachment;
	img.initialLayout = vk::ImageLayout::undefined;

	// view
	vk::ImageViewCreateInfo view;
	view.viewType = vk::ImageViewType::e2d;
	view.format = img.format;
	view.components.r = vk::ComponentSwizzle::r;
	view.components.g = vk::ComponentSwizzle::g;
	view.components.b = vk::ComponentSwizzle::b;
	view.components.a = vk::ComponentSwizzle::a;
	view.subresourceRange.aspectMask = vk::ImageAspectBits::color;
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.layerCount = 1;

	// create the viewable image
	// will set the created image in the view info for us
	multisampleTarget_ = {device(), img, view};
}

void Renderer::record(const RenderBuffer& buf)
{
	static const auto clearValue = vk::ClearValue {{0.f, 0.f, 0.f, 1.f}};
	const auto width = scInfo_.imageExtent.width;
	const auto height = scInfo_.imageExtent.height;

	auto cmdBuf = buf.commandBuffer;
	vk::beginCommandBuffer(cmdBuf, {});

	vk::cmdBindPipeline(cmdBuf, vk::PipelineBindPoint::compute, compPipeline_);
	vk::cmdBindDescriptorSets(cmdBuf, vk::PipelineBindPoint::compute, 
		compPipelineLayout_, 0, {compDs_}, {});
	vk::cmdDispatch(cmdBuf, 1, 1, 1);

	vk::BufferMemoryBarrier barrier;
	barrier.offset = 0u;
	barrier.buffer = storage_;
	barrier.size = vk::wholeSize;
	barrier.srcAccessMask = vk::AccessBits::shaderWrite;
	barrier.dstAccessMask = vk::AccessBits::shaderRead;
	vk::cmdPipelineBarrier(
		cmdBuf,
		vk::PipelineStageBits::computeShader,
		vk::PipelineStageBits::fragmentShader,
		{},
		{}, {barrier}, {});

	/*
	// not fully working
	for(auto i = 0; i < 256; ++i) {
		vk::cmdDispatch(cmdBuf, 1, 1, 1);

		vk::BufferMemoryBarrier barrier;
		barrier.offset = 0u;
		barrier.buffer = storage_;
		barrier.size = vk::wholeSize;
		barrier.srcAccessMask = vk::AccessBits::shaderWrite;
		barrier.dstAccessMask = vk::AccessBits::shaderRead | vk::AccessBits::shaderWrite;
		vk::cmdPipelineBarrier(
			cmdBuf,
			vk::PipelineStageBits::computeShader,
			vk::PipelineStageBits::computeShader,
			{},
			{}, {barrier}, {});
	}
	*/
	
	vk::cmdBeginRenderPass(cmdBuf, {
		renderPass(),
		buf.framebuffer,
		{0u, 0u, width, height},
		1,
		&clearValue
	}, {});

	vk::Viewport vp {0.f, 0.f, (float) width, (float) height, 0.f, 1.f};
	vk::cmdSetViewport(cmdBuf, 0, 1, vp);
	vk::cmdSetScissor(cmdBuf, 0, 1, {0, 0, width, height});

	vk::cmdBindPipeline(cmdBuf, vk::PipelineBindPoint::graphics, gfxPipeline_);
	vk::cmdBindVertexBuffers(cmdBuf, 0, {storage_}, {sizeof(int) * 2});
	vk::cmdDraw(cmdBuf, 6, hexWidth * hexHeight, 0, 0);

	vk::cmdBindPipeline(cmdBuf, vk::PipelineBindPoint::graphics, linePipeline_);
	vk::cmdDraw(cmdBuf, 6, hexWidth * hexHeight, 0, 0);

	vk::cmdEndRenderPass(cmdBuf);
	vk::endCommandBuffer(cmdBuf);
}

void Renderer::resize(nytl::Vec2ui size)
{
	vpp::DefaultRenderer::resize({size[0], size[1]}, scInfo_);
}

void Renderer::samples(vk::SampleCountBits samples)
{
	sampleCount_ = samples;
	if(sampleCount_ != vk::SampleCountBits::e1) {
		createMultisampleTarget(scInfo_.imageExtent);
	}

	renderPass_ = createRenderPass(device(), scInfo_.imageFormat, samples);
	vpp::DefaultRenderer::renderPass_ = renderPass_;
	createGraphicsPipeline(device());

	initBuffers(scInfo_.imageExtent, renderBuffers_);
	invalidate();
}

void Renderer::initBuffers(const vk::Extent2D& size, 
	nytl::Span<RenderBuffer> bufs)
{
	if(sampleCount_ != vk::SampleCountBits::e1) {
		createMultisampleTarget(scInfo_.imageExtent);
		vpp::DefaultRenderer::initBuffers(size, bufs, 
			{multisampleTarget_.vkImageView()});
	} else {
		vpp::DefaultRenderer::initBuffers(size, bufs, {});
	}
}

// utility
void Renderer::createGraphicsPipeline(const vpp::Device& device)
{
	// auto msaa = sampleCount != vk::SampleCountBits::e1;
	auto lightVertex = vpp::ShaderModule(device, hex_vert_data);
	auto lightFragment = vpp::ShaderModule(device, color_frag_data);

	vpp::ShaderProgram lightStages({
		{lightVertex, vk::ShaderStageBits::vertex},
		{lightFragment, vk::ShaderStageBits::fragment}
	});

	vk::GraphicsPipelineCreateInfo pipeInfos[2];
	auto& trianglePipe = pipeInfos[0];

	trianglePipe.renderPass = renderPass_;
	trianglePipe.layout = gfxPipelineLayout_;

	trianglePipe.stageCount = lightStages.vkStageInfos().size();
	trianglePipe.pStages = lightStages.vkStageInfos().data();

	vk::VertexInputBindingDescription binding;
	binding.binding = 0;
	binding.inputRate = vk::VertexInputRate::instance;
	binding.stride = sizeof(int);

	vk::VertexInputAttributeDescription attribute;
	attribute.offset = 0;
	attribute.binding = 0;
	attribute.location = 0;
	attribute.format = vk::Format::r32Sint;

	vk::PipelineVertexInputStateCreateInfo vertexInfo {};
	vertexInfo.vertexBindingDescriptionCount = 1;
	vertexInfo.pVertexBindingDescriptions = &binding;
	vertexInfo.vertexAttributeDescriptionCount = 1;
	vertexInfo.pVertexAttributeDescriptions = &attribute;
	trianglePipe.pVertexInputState = &vertexInfo;

	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo;
	assemblyInfo.topology = vk::PrimitiveTopology::triangleFan;
	trianglePipe.pInputAssemblyState = &assemblyInfo;

	vk::PipelineRasterizationStateCreateInfo rasterizationInfo;
	rasterizationInfo.polygonMode = vk::PolygonMode::fill;
	rasterizationInfo.cullMode = vk::CullModeBits::none;
	rasterizationInfo.frontFace = vk::FrontFace::counterClockwise;
	rasterizationInfo.depthClampEnable = false;
	rasterizationInfo.rasterizerDiscardEnable = false;
	rasterizationInfo.depthBiasEnable = false;
	rasterizationInfo.lineWidth = 1.f;
	trianglePipe.pRasterizationState = &rasterizationInfo;

	vk::PipelineMultisampleStateCreateInfo multisampleInfo;
	multisampleInfo.rasterizationSamples = sampleCount_;
	multisampleInfo.sampleShadingEnable = false;
	multisampleInfo.alphaToCoverageEnable = false;
	trianglePipe.pMultisampleState = &multisampleInfo;

	vk::PipelineColorBlendAttachmentState blendAttachment;
	blendAttachment.blendEnable = true;
	blendAttachment.alphaBlendOp = vk::BlendOp::add;
	blendAttachment.srcColorBlendFactor = vk::BlendFactor::srcAlpha;
	blendAttachment.dstColorBlendFactor = vk::BlendFactor::oneMinusSrcAlpha;
	blendAttachment.srcAlphaBlendFactor = vk::BlendFactor::one;
	blendAttachment.dstAlphaBlendFactor = vk::BlendFactor::zero;
	blendAttachment.colorWriteMask =
		vk::ColorComponentBits::r |
		vk::ColorComponentBits::g |
		vk::ColorComponentBits::b |
		vk::ColorComponentBits::a;

	vk::PipelineColorBlendStateCreateInfo blendInfo;
	blendInfo.attachmentCount = 1;
	blendInfo.pAttachments = &blendAttachment;
	trianglePipe.pColorBlendState = &blendInfo;

	vk::PipelineViewportStateCreateInfo viewportInfo;
	viewportInfo.scissorCount = 1;
	viewportInfo.viewportCount = 1;
	trianglePipe.pViewportState = &viewportInfo;

	const auto dynStates = {vk::DynamicState::viewport, vk::DynamicState::scissor};

	vk::PipelineDynamicStateCreateInfo dynamicInfo;
	dynamicInfo.dynamicStateCount = dynStates.size();
	dynamicInfo.pDynamicStates = dynStates.begin();
	trianglePipe.pDynamicState = &dynamicInfo;

	//
	auto lineVertex = vpp::ShaderModule(device, hexLine_vert_data);
	auto lineFragment = vpp::ShaderModule(device, black_frag_data);

	auto& linePipe = pipeInfos[1];
	linePipe = trianglePipe;

	vpp::ShaderProgram lineStages({
		{lineVertex, vk::ShaderStageBits::vertex},
		{lineFragment, vk::ShaderStageBits::fragment}
	});

	vk::PipelineVertexInputStateCreateInfo vertexInfo2 {};
	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo2;
	assemblyInfo2.topology = vk::PrimitiveTopology::lineStrip;

	linePipe.pInputAssemblyState = &assemblyInfo2;
	linePipe.pVertexInputState = &vertexInfo2;
	linePipe.stageCount = lineStages.vkStageInfos().size();
	linePipe.pStages = lineStages.vkStageInfos().data();

	// setup cache
	constexpr auto cacheName = "graphicsCache.bin";
	vpp::PipelineCache cache {device, cacheName};

	vk::Pipeline pipes[2];
	vk::createGraphicsPipelines(device, cache, 2, *pipeInfos, nullptr, *pipes);
	vpp::save(cache, cacheName);

	gfxPipeline_ = {device, pipes[0]};
	linePipeline_ = {device, pipes[1]};
}

vpp::RenderPass createRenderPass(const vpp::Device& dev,
	vk::Format format, vk::SampleCountBits sampleCount)
{
	vk::AttachmentDescription attachments[2] {};
	auto msaa = sampleCount != vk::SampleCountBits::e1;

	auto swapchainID = 0u;
	if(msaa) {
		// multisample color attachment
		attachments[0].format = format;
		attachments[0].samples = sampleCount;
		attachments[0].loadOp = vk::AttachmentLoadOp::clear;
		attachments[0].storeOp = vk::AttachmentStoreOp::dontCare;
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::dontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::dontCare;
		attachments[0].initialLayout = vk::ImageLayout::undefined;
		attachments[0].finalLayout = vk::ImageLayout::presentSrcKHR;

		swapchainID = 1u;
	}

	// swapchain color attachments we want to resolve to
	attachments[swapchainID].format = format;
	attachments[swapchainID].samples = vk::SampleCountBits::e1;
	if(msaa) attachments[swapchainID].loadOp = vk::AttachmentLoadOp::dontCare;
	else attachments[swapchainID].loadOp = vk::AttachmentLoadOp::clear;
	attachments[swapchainID].storeOp = vk::AttachmentStoreOp::store;
	attachments[swapchainID].stencilLoadOp = vk::AttachmentLoadOp::dontCare;
	attachments[swapchainID].stencilStoreOp = vk::AttachmentStoreOp::dontCare;
	attachments[swapchainID].initialLayout = vk::ImageLayout::undefined;
	attachments[swapchainID].finalLayout = vk::ImageLayout::presentSrcKHR;

	// refs
	vk::AttachmentReference colorReference;
	colorReference.attachment = 0;
	colorReference.layout = vk::ImageLayout::colorAttachmentOptimal;

	vk::AttachmentReference resolveReference;
	resolveReference.attachment = 1;
	resolveReference.layout = vk::ImageLayout::colorAttachmentOptimal;

	// deps
	std::array<vk::SubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = vk::subpassExternal;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = vk::PipelineStageBits::bottomOfPipe;
	dependencies[0].dstStageMask = vk::PipelineStageBits::colorAttachmentOutput;
	dependencies[0].srcAccessMask = vk::AccessBits::memoryRead;
	dependencies[0].dstAccessMask = vk::AccessBits::colorAttachmentRead |
		vk::AccessBits::colorAttachmentWrite;
	dependencies[0].dependencyFlags = vk::DependencyBits::byRegion;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = vk::subpassExternal;
	dependencies[1].srcStageMask = vk::PipelineStageBits::colorAttachmentOutput;;
	dependencies[1].dstStageMask = vk::PipelineStageBits::bottomOfPipe;
	dependencies[1].srcAccessMask = vk::AccessBits::colorAttachmentRead |
		vk::AccessBits::colorAttachmentWrite;
	dependencies[1].dstAccessMask = vk::AccessBits::memoryRead;
	dependencies[1].dependencyFlags = vk::DependencyBits::byRegion;

	// only subpass
	vk::SubpassDescription subpass;
	subpass.pipelineBindPoint = vk::PipelineBindPoint::graphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	if(sampleCount != vk::SampleCountBits::e1)
		subpass.pResolveAttachments = &resolveReference;

	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = 1 + msaa;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if(msaa) {
		renderPassInfo.dependencyCount = dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();
	}

	return {dev, renderPassInfo};
}