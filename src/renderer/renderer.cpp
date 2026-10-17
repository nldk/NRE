#include <iostream>
#include <ostream>
#include "../../include/renderer.h"

#include <algorithm>
#include <format>
#include <memory>
#include "errorHandeling.h"
#include "context.h"
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
namespace NRE {
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
    void transition(VkCommandBuffer cmd, VkImage img, VkImageLayout from, VkImageLayout to,
                    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.srcStageMask = b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.oldLayout = from; b.newLayout = to; b.image = img;
        b.subresourceRange = {aspect, 0, 1, 0, 1};
        VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 1, &b};
        vkCmdPipelineBarrier2(cmd, &d);
    }
    void destroyDepthImage(Context* c) {
        DepthImage& d = c->vulkan.depth;
        if (d.view) vkDestroyImageView(c->vulkan.device, d.view, nullptr);
        if (d.image) vmaDestroyImage(c->vulkan.allocator, d.image, d.allocation);
        d = {};
    }
    // (Re)creates the depth buffer at the swapchain's size. Needs the allocator, so call it after vmaCreateAllocator.
    Result<void> createDepthImage(Context* c) {
        destroyDepthImage(c);
        VkExtent2D e = c->vulkan.swapchain.extent;
        VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = depthFormat;
        ici.extent = {e.width, e.height, 1};
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_AUTO;
        aci.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;   // render targets do best in their own allocation
        DepthImage& d = c->vulkan.depth;
        if (vmaCreateImage(c->vulkan.allocator, &ici, &aci, &d.image, &d.allocation, nullptr) != VK_SUCCESS) {
            d = {};
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorCreatingSwapchain,"Failed to create depth image."})) {
                return std::unexpected(Error{ErrorCode::ErrorCreatingSwapchain,"Failed to create depth image."});
            };
            return {};
        }
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = d.image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = depthFormat;
        vci.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(c->vulkan.device, &vci, nullptr, &d.view) != VK_SUCCESS) {
            destroyDepthImage(c);
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorCreatingSwapchain,"Failed to create depth image view."})) {
                return std::unexpected(Error{ErrorCode::ErrorCreatingSwapchain,"Failed to create depth image view."});
            };
        }
        return {};
    }
    void closeCallback(GLFWwindow*) {
        auto c = context();
        if (!c) {
            ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Window close callback fired without a renderer context."});
            return;
        }
        c->window.shouldClose = true;
    }
    void framebufferSizeCallback(GLFWwindow*, int, int) {
        if (auto c = context()) c->window.framebufferResized = true;
    }
    // Called once per presented frame. Updates deltaTime every frame and the FPS (plus title) every half second.
    void updateFrameTimer(Context* c) {
        constexpr double fpsWindow = 0.5;
        FrameTimer& t = c->timer;
        double now = glfwGetTime();
        t.deltaTime = now - t.lastFrameTime;
        t.lastFrameTime = now;
        t.framesInWindow++;
        double elapsed = now - t.windowStart;
        if (elapsed < fpsWindow) return;

        t.fps = t.framesInWindow / elapsed;
        t.framesInWindow = 0;
        t.windowStart = now;
        if (c->config.windowConfig.showFpsInTitle) {
            std::string title = std::format("{} - {:.0f} FPS ({:.2f} ms)", c->config.windowConfig.title, t.fps, 1000.0 / t.fps);
            glfwSetWindowTitle(c->window.window, title.c_str());
        }
    }
    // Size of the window in pixels. Can differ from the window size on scaled/HiDPI displays, and is 0x0 while minimized.
    VkExtent2D framebufferExtent(Context* c) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(c->window.window, &width, &height);
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }
    // Rebuilds c->vulkan.projection for the current swapchain size. Call whenever the swapchain changes.
    // 3D: perspective, Y up. 2D: orthographic in pixels, origin at the top-left corner and Y pointing down.
    void updateProjection(Context* c) {
        VkExtent2D e = c->vulkan.swapchain.extent;
        if (e.width == 0 || e.height == 0) return;   // minimized: keep the old matrix instead of dividing by zero
        float width = static_cast<float>(e.width);
        float height = static_cast<float>(e.height);
        if (c->config.mode == RenderMode::Render_3D) {
            constexpr float fovY = glm::radians(60.0f), nearPlane = 0.1f, farPlane = 1000.0f;
            c->vulkan.projection = glm::perspective(fovY, width / height, nearPlane, farPlane);
            c->vulkan.projection[1][1] *= -1;   // Vulkan's clip-space Y points down; flip so world Y is up
        } else {
            // Vulkan's clip-space Y already points down, so mapping y = 0 to -1 puts it at the top edge.
            // The depth range is wide so the camera's z position doesn't clip 2D geometry.
            c->vulkan.projection = glm::ortho(0.0f, width, 0.0f, height, -1000.0f, 1000.0f);
        }
    }
    Result<void> recreateSwapchain() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Cannot recreate swapchain: renderer not initialized.");
        // A minimized window has no size and a 0x0 swapchain isn't allowed. Keep the old one; the resize flag
        // stays set, so this is tried again after the next present.
        VkExtent2D extent = framebufferExtent(c);
        if (extent.width == 0 || extent.height == 0) return {};

        vkDeviceWaitIdle(c->vulkan.device);
        // The size has to be passed explicitly: Wayland leaves it up to the app, and vk-bootstrap would fall back to 256x256.
        auto spawchain = vkb::SwapchainBuilder(c->vulkan.device).set_old_swapchain(c->vulkan.swapchain)
                             .set_desired_extent(extent.width, extent.height).build();
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
        c->window.framebufferResized = false;
        updateProjection(c);
        if (auto depth = createDepthImage(c); !depth) return depth;

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
        NRE_REQUIRE_CONTEXT(c, "Renderer config not created. create with setRenderConfig.");
        // Validation checks every Vulkan call, which costs ~60% of the frame rate, so only Debug builds turn it on.
#ifdef RENDERER_ENABLE_VALIDATION
        constexpr bool enableValidation = true;
#else
        constexpr bool enableValidation = false;
#endif
        vkb::InstanceBuilder instanceBuilder;
        instanceBuilder.set_engine_name("NRE").set_app_version(1,0,0).set_engine_version(0,1,0).require_api_version(1,3,0)
                       .request_validation_layers(enableValidation);
        if (enableValidation) instanceBuilder.use_default_debug_messenger();   // prints the validation messages
        auto vulcanInstanceBuilder = instanceBuilder.build();
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
        glfwSetFramebufferSizeCallback(c->window.window,framebufferSizeCallback);
        c->timer.lastFrameTime = c->timer.windowStart = glfwGetTime();
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

        VkExtent2D extent = framebufferExtent(c);
        auto spawchain = vkb::SwapchainBuilder(c->vulkan.device).set_desired_extent(extent.width, extent.height).build();
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
        updateProjection(c);

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

        {
            UploadContext& u = c->vulkan.upload;
            vkCreateCommandPool(c->vulkan.device, &pci, nullptr, &u.pool);
            VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.commandPool = u.pool;
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandBufferCount = 1;
            vkAllocateCommandBuffers(c->vulkan.device, &ai, &u.cmd);
            VkFenceCreateInfo ufci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};  // unsignaled: reset only after each wait
            vkCreateFence(c->vulkan.device, &ufci, nullptr, &u.fence);
        }

        VmaAllocatorCreateInfo aci{};
        aci.instance         = c->vulkan.instance.instance;
        aci.physicalDevice   = c->vulkan.physicalDevice.physical_device;
        aci.device           = c->vulkan.device.device;
        aci.vulkanApiVersion = VK_API_VERSION_1_3;

        if (vmaCreateAllocator(&aci, &c->vulkan.allocator) != VK_SUCCESS) {
            if (ErrorHandeling::handleError(Error{ErrorCode::FailedToCreateAllocator,"Failed to create vma allocator."})) {
                return std::unexpected(Error{ErrorCode::FailedToCreateAllocator,"Failed to create vma allocator."});
            };
        }
        if (auto depth = createDepthImage(c); !depth) return depth;

        return {};
    }

    Result<void> shutdown() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        vkDeviceWaitIdle(c->vulkan.device);
        c->manager.destroyAll();
        destroyDepthImage(c);
        vmaDestroyAllocator(c->vulkan.allocator);
        for (Frame& f : c->vulkan.frames) {
            vkDestroyCommandPool(c->vulkan.device, f.pool, nullptr);  // frees f.cmd along with it
            vkDestroyFence(c->vulkan.device, f.in_flight, nullptr);
            vkDestroySemaphore(c->vulkan.device, f.image_acquired, nullptr);
        }
        for (VkSemaphore s : c->vulkan.renderDone) vkDestroySemaphore(c->vulkan.device, s, nullptr);
        vkDestroyCommandPool(c->vulkan.device, c->vulkan.upload.pool, nullptr);
        vkDestroyFence(c->vulkan.device, c->vulkan.upload.fence, nullptr);

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
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        Pipeline* pipeline = c->manager.get(c->vulkan.pipelineBound);
        if (!pipeline) {
            if (ErrorHandeling::handleError(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to vertex shader out of date or non-existing"})) {
                return std::unexpected(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to vertex shader out of date or non-existing"});
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
        // Last frame's depth values aren't needed (it's cleared below), so UNDEFINED lets the driver discard them.
        transition(f.cmd,c->vulkan.depth.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,VK_IMAGE_ASPECT_DEPTH_BIT);

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
        VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depth.imageView = c->vulkan.depth.view;
        depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;   // only needed during this frame
        depth.clearValue.depthStencil = {1.0f, 0};           // 1.0 = far plane, so anything drawn is closer
        ri.pDepthAttachment = &depth;
        vkCmdBeginRendering(f.cmd,&ri);

        vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline->pipeline);

        VkExtent2D e = c->vulkan.swapchain.extent;
        VkViewport viewport{0.0f, 0.0f, (float)e.width, (float)e.height, 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, e};
        vkCmdSetViewport(f.cmd, 0, 1, &viewport);
        vkCmdSetScissor(f.cmd, 0, 1, &scissor);

        // Without a bound scene the frame is only cleared. The error is returned after the frame is
        // finished: returning here would leave the fence reset and the next renderFrame waiting forever.
        bool noScene = false;
        if (Scene* scene = c->manager.get(c->currentScene)) {
            (void)scene->draw(f.cmd);
        } else {
            noScene = ErrorHandeling::handleError(Error{ErrorCode::NoSceneBound,"No scene was bound. Create one with createScene or bind one with bindScene."});
        }

        vkCmdEndRendering(f.cmd);

        transition(f.cmd,c->vulkan.images[c->vulkan.currentImageIndex],VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkEndCommandBuffer(f.cmd);
        if (!c->config.manualFramePresentationAndSubmission) {
            submitFrame();
            presentFrame();
        }
        if (noScene) {
            return std::unexpected(Error{ErrorCode::NoSceneBound,"No scene was bound. Create one with createScene or bind one with bindScene."});
        }
        return {};
    }

    Result<void> submitFrame() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
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
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &c->vulkan.renderDone[c->vulkan.currentImageIndex];
        pi.swapchainCount = 1; pi.pSwapchains = &c->vulkan.swapchain.swapchain; pi.pImageIndices = &c->vulkan.currentImageIndex;
        VkResult r = vkQueuePresentKHR(c->vulkan.graphicsQueue, &pi);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR || c->window.framebufferResized) recreateSwapchain();
        c->vulkan.currentFrame++;
        updateFrameTimer(c);
        return {};
    }

    Result<bool> windowShouldClose() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        return {c->window.shouldClose};
    }

    Result<void> pollEvents() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        glfwPollEvents();
        return {};
    }
    Result<Handel<Shader>> createShader(std::string shaderPath,ShaderType type) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        return c->manager.create(Shader(shaderPath,type));
    }

    Result<void> destroyShader(Handel<Shader> shader) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        c->manager.destroy(shader);
        return {};
    }

    Result<Handel<Pipeline> > createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        return c->manager.create(Pipeline(vertexShader,fragmentShader,Vertex::layout()));
    }
    Result<void> destroyPipeline(Handel<Pipeline> pipeline) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        c->manager.destroy(pipeline);
        return {};
    }

    Result<void> bindPipeline(Handel<Pipeline> pipeline) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        auto pipeLine = c->manager.get(pipeline);
        if (!pipeLine) {
            if (ErrorHandeling::handleError(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to vertex shader out of date or non-existing"})) {
                return std::unexpected(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to vertex shader out of date or non-existing"});
            };
        }
        c->vulkan.pipelineBound = pipeline;
        return {};
    }

    Result<Handel<Model>> createModel(std::string modelPath) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        auto model = loadModel(modelPath);
        if (!model) {
            ErrorHandeling::handleError(model.error());
            return std::unexpected(model.error());
        }
        if (auto uploaded = model->uploadMeshes(); !uploaded) return std::unexpected(uploaded.error());
        return c->manager.create(std::move(*model));
    }

    Result<void> destroyModel(Handel<Model> model) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        if (Model* m = c->manager.get(model)) {
            m->releaseGpuMeshes();
            for (Handel<Scene> scene : m->scenes) {
                if (Scene* s = c->manager.get(scene)) s->removeModel(model);
            }
        }
        c->manager.destroy(model);
        return {};
    }


    namespace {
        Result<Model*> getModel(Handel<Model> handle) {
            auto c = context();
            NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
            Model* model = c->manager.get(handle);
            if (!model) {
                ErrorHandeling::handleError(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to model out of date or non-existing"});
                return std::unexpected(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to model out of date or non-existing"});
            }
            return model;
        }
    }

    Result<void> setModelTransform(Handel<Model> model, const Transform& transform) {
        return getModel(model).transform([&](Model* m) { m->transform = transform; });
    }

    Result<Transform> getModelTransform(Handel<Model> model) {
        return getModel(model).transform([](Model* m) { return m->transform; });
    }

    Result<glm::mat4> getModelMatrix(Handel<Model> model) {
        return getModel(model).transform([](Model* m) { return m->transform.matrix(); });
    }

    Result<void> translateModel(Handel<Model> model, glm::vec3 offset) {
        return getModel(model).transform([&](Model* m) { m->transform.position += offset; });
    }

    Result<void> rotateModel(Handel<Model> model, float angle, glm::vec3 axis) {
        return getModel(model).transform([&](Model* m) {
            m->transform.rotation = glm::normalize(glm::angleAxis(angle, glm::normalize(axis)) * m->transform.rotation);
        });
    }

    Result<void> scaleModel(Handel<Model> model, glm::vec3 factor) {
        return getModel(model).transform([&](Model* m) { m->transform.scale *= factor; });
    }

    namespace {
        Result<Scene*> getScene(Handel<Scene> handle) {
            auto c = context();
            NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
            Scene* scene = c->manager.get(handle);
            if (!scene) {
                ErrorHandeling::handleError(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to scene out of date or non-existing"});
                return std::unexpected(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to scene out of date or non-existing"});
            }
            return scene;
        }

        Result<Scene*> getBoundScenePtr() {
            auto c = context();
            NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
            Scene* scene = c->manager.get(c->currentScene);
            if (!scene) {
                ErrorHandeling::handleError(Error{ErrorCode::NoSceneBound,"No scene was bound. Create one with createScene or bind one with bindScene."});
                return std::unexpected(Error{ErrorCode::NoSceneBound,"No scene was bound. Create one with createScene or bind one with bindScene."});
            }
            return scene;
        }

        Result<Camera*> getCamera() {
            return getBoundScenePtr().transform([](Scene* scene) { return &scene->camera; });
        }
    }

    Result<Handel<Scene>> createScene() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        Handel<Scene> scene = c->manager.create(Scene{});
        if (!c->manager.get(c->currentScene)) c->currentScene = scene;
        return scene;
    }

    Result<void> destroyScene(Handel<Scene> scene) {
        auto s = getScene(scene);
        if (!s) return std::unexpected(s.error());
        auto c = context();
        for (Handel<Model> model : (*s)->getModels()) {
            if (Model* m = c->manager.get(model)) std::erase(m->scenes, scene);
        }
        if (c->currentScene == scene) c->currentScene = {};
        c->manager.destroy(scene);
        return {};
    }

    Result<void> bindScene(Handel<Scene> scene) {
        return getScene(scene).transform([&](Scene*) { context()->currentScene = scene; });
    }

    Result<Handel<Scene>> getBoundScene() {
        return getBoundScenePtr().transform([](Scene*) { return context()->currentScene; });
    }

    Result<void> addModelToScene(Handel<Model> model) {
        auto s = getBoundScenePtr();
        if (!s) return std::unexpected(s.error());
        auto m = getModel(model);
        if (!m) return std::unexpected(m.error());
        Handel<Scene> scene = context()->currentScene;
        (*s)->addModel(model);
        if (std::ranges::find((*m)->scenes, scene) == (*m)->scenes.end()) (*m)->scenes.push_back(scene);
        return {};
    }

    Result<void> removeModelFromScene(Handel<Model> model) {
        auto s = getBoundScenePtr();
        if (!s) return std::unexpected(s.error());
        auto m = getModel(model);
        if (!m) return std::unexpected(m.error());
        (*s)->removeModel(model);
        std::erase((*m)->scenes, context()->currentScene);
        return {};
    }

    Result<void> setCameraPosition(glm::vec3 position) {
        return getCamera().transform([&](Camera* cam) { cam->position = position; });
    }

    Result<glm::vec3> getCameraPosition() {
        return getCamera().transform([](Camera* cam) { return cam->position; });
    }

    Result<void> moveCamera(glm::vec3 offset) {
        return getCamera().transform([&](Camera* cam) { cam->position += offset; });
    }

    Result<void> moveCameraRelative(glm::vec3 offset) {
        return getCamera().transform([&](Camera* cam) {
            cam->position += cam->right() * offset.x + cam->up() * offset.y + cam->forward() * offset.z;
        });
    }

    Result<void> setCameraRotation(float yaw, float pitch) {
        return getCamera().transform([&](Camera* cam) {
            cam->yaw = yaw;
            cam->pitch = glm::clamp(pitch, -Camera::maxPitch, Camera::maxPitch);
        });
    }

    Result<void> rotateCamera(float yawDelta, float pitchDelta) {
        return getCamera().transform([&](Camera* cam) { cam->rotate(yawDelta, pitchDelta); });
    }

    Result<void> cameraLookAt(glm::vec3 target) {
        return getCamera().transform([&](Camera* cam) { cam->lookAt(target); });
    }

    Result<glm::vec3> getCameraForward() {
        return getCamera().transform([](Camera* cam) { return cam->forward(); });
    }

    Result<glm::mat4> getViewMatrix() {
        return getCamera().transform([](Camera* cam) { return cam->viewMatrix(); });
    }

    Result<float> getFPS() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        return static_cast<float>(c->timer.fps);
    }

    Result<float> getDeltaTime() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        return static_cast<float>(c->timer.deltaTime);
    }

}

