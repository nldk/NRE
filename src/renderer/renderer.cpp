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
    void transition(VkCommandBuffer cmd, VkImage img, VkImageLayout from, VkImageLayout to) {
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.srcStageMask = b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.oldLayout = from; b.newLayout = to; b.image = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 1, &b};
        vkCmdPipelineBarrier2(cmd, &d);
    }
    void closeCallback(GLFWwindow*) {
        auto c = context();
        if (!c) {
            ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Window close callback fired without a renderer context."});
            return;
        }
        c->window.shouldClose = true;
    }
    Result<void> recreateSwapchain() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Cannot recreate swapchain: renderer not initialized."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Cannot recreate swapchain: renderer not initialized."});
            };
        }
        vkDeviceWaitIdle(c->vulkan.device);
        auto spawchain = vkb::SwapchainBuilder(c->vulkan.device).set_old_swapchain(c->vulkan.swapchain).build();
        if (!spawchain){
            std::string msg = "Error recreating swapchain: " + spawchain.error().message();
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorRecreatingSwapchain,msg})) {
                return std::unexpected(Error{ErrorCode::ErrorRecreatingSwapchain,msg});
            };
            return {};
        }
        c->vulkan.swapchain.destroy_image_views(c->vulkan.views);
        vkb::destroy_swapchain(c->vulkan.swapchain);
        c->vulkan.swapchain = spawchain.value();
        c->vulkan.images = c->vulkan.swapchain.get_images().value();
        c->vulkan.views = c->vulkan.swapchain.get_image_views().value();

        // renderDone is indexed per swapchain image, so it has to match the new image count
        for (VkSemaphore s : c->vulkan.renderDone) vkDestroySemaphore(c->vulkan.device, s, nullptr);
        c->vulkan.renderDone.resize(c->vulkan.images.size());
        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (VkSemaphore& s : c->vulkan.renderDone)
            vkCreateSemaphore(c->vulkan.device, &sci, nullptr, &s);
        return {};
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
            std::string msg = "Error creating Vulkan instance: " + vulcanInstanceBuilder.error().message();
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorCreatingVulkanInstance,msg})) {
                return std::unexpected(Error{ErrorCode::ErrorCreatingVulkanInstance,msg});
            };
            return {};
        }
        c->vulkan.instance = vulcanInstanceBuilder.value();
        if (!glfwInit()) {
            if (ErrorHandeling::handleError(Error{ErrorCode::WindowCreationFailed,"Error initializing GLFW."})) {
                return std::unexpected(Error{ErrorCode::WindowCreationFailed,"Error initializing GLFW."});
            };
            return {};
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, c->config.windowConfig.resizable);
        c->window.window = glfwCreateWindow(c->config.windowConfig.width, c->config.windowConfig.height, c->config.windowConfig.title.c_str(), nullptr, nullptr);
        if (!c->window.window) {
            if (ErrorHandeling::handleError(Error{ErrorCode::WindowCreationFailed,"Error creating GLFW window."})) {
                return std::unexpected(Error{ErrorCode::WindowCreationFailed,"Error creating GLFW window."});
            };
            return {};
        }
        if (glfwCreateWindowSurface(c->vulkan.instance,c->window.window,nullptr,&c->vulkan.surface) != VK_SUCCESS) {
            if (ErrorHandeling::handleError(Error{ErrorCode::WindowCreationFailed,"Error creating window surface."})) {
                return std::unexpected(Error{ErrorCode::WindowCreationFailed,"Error creating window surface."});
            };
            return {};
        }
        glfwSetWindowCloseCallback(c->window.window,closeCallback);
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
            std::string msg = "Error selecting physical device: " + phys.error().message();
            if (ErrorHandeling::handleError(Error{ErrorCode::UnsupportedGPU,msg})) {
                return std::unexpected(Error{ErrorCode::UnsupportedGPU,msg});
            };
            return {};
        }

        c->vulkan.physicalDevice = phys.value();
        vkb::Device vkb_dev = vkb::DeviceBuilder{phys.value()}.build().value();
        c->vulkan.device = vkb_dev;
        c->vulkan.graphicsQueue = vkb_dev.get_queue(vkb::QueueType::graphics).value();
        c->vulkan.graphicsQueueFamily = vkb_dev.get_queue_index(vkb::QueueType::graphics).value();

        auto spawchain = vkb::SwapchainBuilder(c->vulkan.device).build();
        if (!spawchain){
            std::string msg = "Error creating swapchain: " + spawchain.error().message();
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorCreatingSwapchain,msg})) {
                return std::unexpected(Error{ErrorCode::ErrorCreatingSwapchain,msg});
            };
            return {};
        }

        c->vulkan.swapchain = spawchain.value();
        c->vulkan.images = c->vulkan.swapchain.get_images().value();
        c->vulkan.views = c->vulkan.swapchain.get_image_views().value();

        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // allows vkResetCommandBuffer
        pci.queueFamilyIndex = c->vulkan.graphicsQueueFamily;
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // so the very first wait returns immediately
        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        for (Frame& f : c->vulkan.frames) {
            vkCreateCommandPool(c->vulkan.device, &pci, nullptr, &f.pool);
            VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.commandPool = f.pool;
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandBufferCount = 1;
            vkAllocateCommandBuffers(c->vulkan.device, &ai, &f.cmd);
            vkCreateFence(c->vulkan.device, &fci, nullptr, &f.in_flight);
            vkCreateSemaphore(c->vulkan.device, &sci, nullptr, &f.image_acquired);
        }

        c->vulkan.renderDone.resize(c->vulkan.images.size());
        for (VkSemaphore& s : c->vulkan.renderDone)
            vkCreateSemaphore(c->vulkan.device, &sci, nullptr, &s);



        return {};
    }

    Result<void> shutdown() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        vkDeviceWaitIdle(c->vulkan.device);
        for (Frame& f : c->vulkan.frames) {
            vkDestroyCommandPool(c->vulkan.device, f.pool, nullptr);  // frees f.cmd along with it
            vkDestroyFence(c->vulkan.device, f.in_flight, nullptr);
            vkDestroySemaphore(c->vulkan.device, f.image_acquired, nullptr);
        }
        for (VkSemaphore s : c->vulkan.renderDone) vkDestroySemaphore(c->vulkan.device, s, nullptr);

        c->vulkan.swapchain.destroy_image_views(c->vulkan.views);
        vkb::destroy_swapchain(c->vulkan.swapchain);
        vkb::destroy_device(c->vulkan.device);
        vkb::destroy_surface(c->vulkan.instance, c->vulkan.surface);
        vkb::destroy_instance(c->vulkan.instance);
        glfwDestroyWindow(c->window.window);
        glfwTerminate();
        globalContext.reset();
        return {};
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
        VkResult r = vkAcquireNextImageKHR(c->vulkan.device,c->vulkan.swapchain,UINT64_MAX,f.image_acquired,VK_NULL_HANDLE,&c->vulkan.currentImageIndex);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain();return {};}
        vkResetFences(c->vulkan.device, 1, &f.in_flight);

        vkResetCommandBuffer(f.cmd, 0);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(f.cmd,&bi);
        transition(f.cmd,c->vulkan.images[c->vulkan.currentImageIndex],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo color = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        color.imageView = c->vulkan.views[c->vulkan.currentImageIndex];
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color = {{0.1f, 0.3f, 0.6f, 1.0f}};
        VkRenderingInfo ri = {VK_STRUCTURE_TYPE_RENDERING_INFO};
        ri.renderArea = {{0,0},c->vulkan.swapchain.extent};
        ri.layerCount = 1;
        ri.colorAttachmentCount = 1;
        ri.pColorAttachments = &color;
        vkCmdBeginRendering(f.cmd,&ri);



        vkCmdEndRendering(f.cmd);

        transition(f.cmd,c->vulkan.images[c->vulkan.currentImageIndex],VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkEndCommandBuffer(f.cmd);
        if (!c->config.manualFramePresentationAndSubmission) {
            submitFrame();
            presentFrame();
        }
        return {};
    }

    Result<void> submitFrame() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        Frame& f = c->vulkan.frames[c->vulkan.currentFrame % max_frames_in_flight];
        VkCommandBufferSubmitInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, nullptr, f.cmd};
        VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, f.image_acquired, 0,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, c->vulkan.renderDone[c->vulkan.currentImageIndex], 0,
                                     VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT};
        VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        si.waitSemaphoreInfoCount = 1;   si.pWaitSemaphoreInfos = &wait;
        si.commandBufferInfoCount = 1;   si.pCommandBufferInfos = &cbi;
        si.signalSemaphoreInfoCount = 1; si.pSignalSemaphoreInfos = &signal;
        vkQueueSubmit2(c->vulkan.graphicsQueue, 1, &si, f.in_flight);
        return {};
    }

    Result<void> presentFrame() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &c->vulkan.renderDone[c->vulkan.currentImageIndex];
        pi.swapchainCount = 1; pi.pSwapchains = &c->vulkan.swapchain.swapchain; pi.pImageIndices = &c->vulkan.currentImageIndex;
        VkResult r = vkQueuePresentKHR(c->vulkan.graphicsQueue, &pi);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) recreateSwapchain();
        c->vulkan.currentFrame++;
        return {};
    }

    Result<bool> windowShouldClose() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        return {c->window.shouldClose};
    }

    Result<void> pollEvents() {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        glfwPollEvents();
        return {};
    }
}

