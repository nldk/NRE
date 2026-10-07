#include <iostream>
#include <ostream>
#include "../../include/renderer.h"

#include <memory>
#include "errorHandeling.h"
#include "context.h"
namespace Renderer {
    namespace {std::unique_ptr<Context> globalContext = nullptr;}
    Context* context(){return globalContext.get();}
    Result<void> setRenderConfig(RenderConfig config) {
        auto c = context();
        if (c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::AlreadyInitialized,"Renderer already initialized"})) {
                return std::unexpected(Error{ErrorCode::AlreadyInitialized,"Renderer already initialized"});
            };
        }
        globalContext = std::make_unique<Context>();
        globalContext->config = config;
        return {};
    }
    Result<void> recreateSwapchain() {
        auto c = context();
        if (c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::AlreadyInitialized,"Renderer already initialized"})) {
                return std::unexpected(Error{ErrorCode::AlreadyInitialized,"Renderer already initialized"});
            };
        }
        auto spawchain = vkb::SwapchainBuilder(c->vulkan.device).set_old_swapchain(c->vulkan.swapchain).build();
        if (!spawchain){
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorRecreatingSwapchain,"Error recreating swapchain."})) {
                return std::unexpected(Error{ErrorCode::ErrorRecreatingSwapchain,"Error recreating swapchain."});
            };
        }
        c->vulkan.swapchain = spawchain.value();
    }
    Result<void> init() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        auto vulcanInstanceBuilder = vkb::InstanceBuilder().set_engine_name("NRE").set_app_version(1,0,0).set_engine_version(0,1,0).require_api_version(1,3,0).request_validation_layers().use_default_debug_messenger().build();
        if (!vulcanInstanceBuilder) {
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorCreatingVulcanInstance,"Error creating vulcan instance"})) {
                return std::unexpected(Error{ErrorCode::ErrorCreatingVulcanInstance,"Error creating vulcan instance"});
            };
        }
        c->vulkan.instance = vulcanInstanceBuilder.value();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        c->window = glfwCreateWindow(c->config.windowConfig.width, c->config.windowConfig.height, c->config.windowConfig.title.c_str(), nullptr, nullptr);
        glfwCreateWindowSurface(c->vulkan.instance,c->window,nullptr,&c->vulkan.surface);

        VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        f13.dynamicRendering = VK_TRUE;
        f13.synchronization2 = VK_TRUE;
        VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        f12.bufferDeviceAddress = VK_TRUE;
        f12.descriptorIndexing = VK_TRUE;

        auto phys = vkb::PhysicalDeviceSelector{c->vulkan.instance}
        .set_minimum_version(1, 3)
        .set_required_features_13(f13)
        .set_required_features_12(f12)
        .set_surface(c->vulkan.surface).select();

        if (!phys) {

            if (phys.error() == vkb::PhysicalDeviceError::no_suitable_device) {
                const auto& detailed_reasons = phys.detailed_failure_reasons();
                if (!detailed_reasons.empty()) {
                    std::cerr << "GPU Selection failure reasons:\n";
                    for (const std::string& reason : detailed_reasons) {
                        std::cerr << reason << "\n";
                    }
                }
            }
            if (ErrorHandeling::handleError(Error{ErrorCode::UnsupportedGPU,"Error creating physical device. No suitable GPU found."})) {
                return std::unexpected(Error{ErrorCode::UnsupportedGPU,"Error creating vulcan instance. No suitable GPU found."});
            };
        }
        c->vulkan.physicalDevice = phys.value();
        vkb::Device vkb_dev = vkb::DeviceBuilder{phys.value()}.build().value();
        c->vulkan.device = vkb_dev;

        auto spawchain = vkb::SwapchainBuilder(c->vulkan.device).build();
        if (!spawchain){
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorCreatingSwapchain,"Error creating swapchain."})) {
                return std::unexpected(Error{ErrorCode::ErrorCreatingSwapchain,"Error creating swapchain."});
            };
        }
        c->vulkan.swapchain = spawchain.value();

        return {};
    }

    Result<void> shutdown() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        vkb::destroy_swapchain(c->vulkan.swapchain);
        vkb::destroy_device(c->vulkan.device);
        vkb::destroy_surface(c->vulkan.instance, c->vulkan.surface);
        vkb::destroy_instance(c->vulkan.instance);
        globalContext.release();
    }

    Result<void> renderFrame() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        Frame& f = c->vulkan.frames[c->vulkan.currentFrame % max_frames_in_flight];
        vkWaitForFences(c->vulkan.device, 1, &f.in_flight,VK_TRUE,UINT64_MAX);
        uint32_t img;
        VkResult r = vkAcquireNextImageKHR(c->vulkan.device,c->vulkan.swapchain,UINT64_MAX,f.image_acquired,VK_NULL_HANDLE,&img);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain();return {};}
        vkResetFences(c->vulkan.device, 1, &f.in_flight);

    }

}

