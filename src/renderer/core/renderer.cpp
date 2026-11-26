#include <iostream>
#include <ostream>
#include "renderer.h"

#include <algorithm>
#include <format>
#include <memory>
#include "core/errorHandling.h"
#include "core/context.h"
#include "gpu/upload.h"
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include "defaultAssets/defaultAssets.h"
#include "gpu/texture.h"
#include "gpu/cubemap.h"
#include "lighting/lightingMath.h"
#include "core/frameRecorder.h"

namespace NRE {
    namespace {std::unique_ptr<Context> globalContext = nullptr;}
    Context* context(){return globalContext.get();}
    Result<void> setRenderConfig(RenderConfig config) {
        auto c = context();
        if (c) {
            auto error = Error{ErrorCode::AlreadyInitialized,"setRenderConfig() was called twice. "
                               "Call shutdown() before configuring the renderer again."};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        globalContext = std::make_unique<Context>();
        globalContext->config = config;
        return {};
    }
    void transition(VkCommandBuffer cmd, VkImage img, VkImageLayout from, VkImageLayout to,
                    VkImageAspectFlags aspect, uint32_t layerCount) {
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.srcStageMask = b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.oldLayout = from; b.newLayout = to; b.image = img;
        b.subresourceRange = {aspect, 0, 1, 0, layerCount};
        VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 1, &b};
        vkCmdPipelineBarrier2(cmd, &d);
    }
    void destroyDepthImage(Context* c) {
        DepthImage& d = c->vulkan.depth;
        if (d.view) vkDestroyImageView(c->vulkan.device, d.view, nullptr);
        if (d.image) vmaDestroyImage(c->vulkan.allocator, d.image, d.allocation);
        d = {};
    }

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
        aci.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        DepthImage& d = c->vulkan.depth;
        if (VkResult r = vmaCreateImage(c->vulkan.allocator, &ici, &aci, &d.image, &d.allocation, nullptr); r != VK_SUCCESS) {
            d = {};
            auto error = Error{ErrorCode::ImageCreationFailed, "Failed to create the " + std::to_string(e.width) + "x"
                               + std::to_string(e.height) + " depth buffer: " + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = d.image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = depthFormat;
        vci.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        if (VkResult r = vkCreateImageView(c->vulkan.device, &vci, nullptr, &d.view); r != VK_SUCCESS) {
            destroyDepthImage(c);
            auto error = Error{ErrorCode::ImageViewCreationFailed, "Failed to create the depth buffer's image view: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
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
    // Fires during glfwPollEvents, possibly several times, so it adds up instead of overwriting.
    void scrollCallback(GLFWwindow*, double, double yOffset) {
        if (auto c = context()) c->window.scrollDelta += static_cast<float>(yOffset);
    }
    // Events instead of polling glfwGetKey, so a tap that starts and ends between two pollEvents calls isn't missed.
    void keyCallback(GLFWwindow*, int key, int, int action, int) {
        auto c = context();
        if (!c || key < 0 || key > GLFW_KEY_LAST) return;   // GLFW_KEY_UNKNOWN is -1
        if (action == GLFW_PRESS) c->window.keysPressed.set(key);
        else if (action == GLFW_RELEASE) c->window.keysReleased.set(key);
    }
    void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
        auto c = context();
        if (!c || button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return;
        if (action == GLFW_PRESS) c->window.buttonsPressed.set(button);
        else if (action == GLFW_RELEASE) c->window.buttonsReleased.set(button);
    }

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

    VkExtent2D framebufferExtent(Context* c) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(c->window.window, &width, &height);
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    // The size the scene is drawn at: the unsafe render target's if one is set, otherwise the window's.
    VkExtent2D sceneExtent(Context* c) {
#ifdef NRE_UNSAFE_API
        if (c->unsafeContext.targetColor.getImage() != VK_NULL_HANDLE) return c->unsafeContext.targetColor.getExtent();
#endif
        return c->vulkan.swapchain.extent;
    }

    void updateProjection(Context* c) {
        VkExtent2D e = sceneExtent(c);
        if (e.width == 0 || e.height == 0) return;
        float width = static_cast<float>(e.width);
        float height = static_cast<float>(e.height);
        if (c->config.mode == RenderMode::Render_3D) {
            // Reverse-Z with an infinite far plane (depth 1 at the near plane, 0 at infinity); the Vulkan Y flip is included.
            c->vulkan.projection = LightingMath::reverseZPerspective(glm::radians(60.0f), width / height, 0.1f);
        } else {
            c->vulkan.projection = LightingMath::reverseZOrtho(width, height, 1000.0f);
        }
    }
    Result<void> recreateSwapchain() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        VkExtent2D extent = framebufferExtent(c);
        if (extent.width == 0 || extent.height == 0) return {};

        vkDeviceWaitIdle(c->vulkan.device);
        // The size has to be passed explicitly: Wayland leaves it up to the app, and vk-bootstrap would fall back to 256x256.
        auto spawchain = vkb::SwapchainBuilder(c->vulkan.device).set_old_swapchain(c->vulkan.swapchain)
                             .set_desired_extent(extent.width, extent.height).build();
        if (!spawchain){
            std::string msg = "Failed to recreate the swapchain at " + std::to_string(extent.width) + "x" + std::to_string(extent.height) + ": " + spawchain.error().message();
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
        if (auto hdr = c->vulkan.toneMap.resize(sceneExtent(c)); !hdr) return hdr;   // the window's size, without a render target

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
        NRE_REQUIRE_CONTEXT(c);

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
            std::string msg = "init(): failed to create the Vulkan instance (Vulkan 1.3 required): " + vulcanInstanceBuilder.error().message();
            if (ErrorHandeling::handleError(Error{ErrorCode::ErrorCreatingVulkanInstance,msg})) {
                return std::unexpected(Error{ErrorCode::ErrorCreatingVulkanInstance,msg});
            };
            return {};
        }
        c->vulkan.instance = vulcanInstanceBuilder.value();
        if (!glfwInit()) {
            auto error = Error{ErrorCode::WindowCreationFailed, "init(): glfwInit() failed. Is a display available "
                               "(DISPLAY / WAYLAND_DISPLAY set)?"};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, c->config.windowConfig.resizable);
        c->window.window = glfwCreateWindow(c->config.windowConfig.width, c->config.windowConfig.height, c->config.windowConfig.title.c_str(), nullptr, nullptr);
        if (!c->window.window) {
            auto error = Error{ErrorCode::WindowCreationFailed, "init(): failed to create the "
                               + std::to_string(c->config.windowConfig.width) + "x" + std::to_string(c->config.windowConfig.height)
                               + " window \"" + c->config.windowConfig.title + "\"."};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        if (VkResult r = glfwCreateWindowSurface(c->vulkan.instance,c->window.window,nullptr,&c->vulkan.surface); r != VK_SUCCESS) {
            auto error = Error{ErrorCode::WindowCreationFailed, "init(): failed to create the Vulkan surface for the window: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        glfwSetWindowCloseCallback(c->window.window,closeCallback);
        glfwSetFramebufferSizeCallback(c->window.window,framebufferSizeCallback);
        glfwSetScrollCallback(c->window.window,scrollCallback);
        glfwSetKeyCallback(c->window.window,keyCallback);
        glfwSetMouseButtonCallback(c->window.window,mouseButtonCallback);
        // Start from the real cursor position, so the first getMouseDelta isn't a jump from (0, 0).
        glfwGetCursorPos(c->window.window, &c->window.cursor.x, &c->window.cursor.y);
        c->timer.lastFrameTime = c->timer.windowStart = glfwGetTime();

        VkPhysicalDeviceFeatures f10{};
        f10.samplerAnisotropy = VK_TRUE;

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
        .set_required_features(f10)
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
            std::string msg = "init(): no GPU supports what the renderer needs (Vulkan 1.3, dynamic rendering, synchronization2, sampler anisotropy): " + phys.error().message();
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
            std::string msg = "init(): failed to create the " + std::to_string(extent.width) + "x" + std::to_string(extent.height) + " swapchain: " + spawchain.error().message();
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

        if (VkResult r = vmaCreateAllocator(&aci, &c->vulkan.allocator); r != VK_SUCCESS) {
            auto error = Error{ErrorCode::FailedToCreateAllocator, "init(): failed to create the VMA memory allocator: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};   // everything after this needs the allocator
        }
        if (auto depth = createDepthImage(c); !depth) return depth;

        auto err = c->vulkan.descriptorPool.initDescriptorSetPool();
        if (!err) return err;

        err = Texture::createSampler();
        if (!err) return err;

        // The order of descriptorLayouts is the set number: 0 = per frame (scene), 1 = material.
        // Set 0: 0 = FrameData (camera, sun), 1 = lights, 2 = the skybox cubemap.
        auto frameLayout = DescriptorSetLayout();
        VkDescriptorSetLayoutBinding frameBindings[] = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };
        err = frameLayout.createLayout(frameBindings);
        if (!err) return err;
        c->vulkan.descriptorLayouts.push_back(frameLayout);

        auto materialLayout = DescriptorSetLayout();
        err = materialLayout.createLayout(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT);
        if (!err) return err;
        c->vulkan.descriptorLayouts.push_back(materialLayout);

        Texture defaultTexture;
        err = defaultTexture.loadTextureFromMemory(DefaultAssets::defaultTexturePng,sizeof(DefaultAssets::defaultTexturePng));
        if (!err) return err;
        c->vulkan.defaultTexture = c->manager.create(std::move(defaultTexture));
        Material defaultMaterial;
        defaultMaterial.baseTexture = c->vulkan.defaultTexture;
        err = defaultMaterial.createDescriptorSet();
        if (!err) return err;
        c->vulkan.defaultMaterial = c->manager.create(std::move(defaultMaterial));

        // Before any scene: createScene() points every frame set's binding 2 at it.
        {
            const uint32_t black[6] = {0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000};
            Cubemap defaultCubemap;
            err = defaultCubemap.uploadFaces(black, 1);
            if (!err) return err;
            c->vulkan.defaultCubemap = c->manager.create(std::move(defaultCubemap));
        }
        {
            static constexpr uint32_t skyboxVert[] = {
#include "embeddedShaders/skybox.vert.inc"
            };
            static constexpr uint32_t skyboxFrag[] = {
#include "embeddedShaders/skybox.frag.inc"
            };
            Handel<Shader> vertex = c->manager.create(Shader(std::span(skyboxVert), ShaderType::VERTEX_SHADER));
            Handel<Shader> fragment = c->manager.create(Shader(std::span(skyboxFrag), ShaderType::FRAGMENT_SHADER));
            // No vertex input (the vertex shader makes the triangle) and no culling. The sky sits at depth 0, the
            // cleared value (reverse-Z), so GREATER_OR_EQUAL passes only where nothing was drawn, and it doesn't write depth.
            Pipeline skybox;
            err = skybox.createPipeline(vertex, fragment, {}, VK_CULL_MODE_NONE, VK_COMPARE_OP_GREATER_OR_EQUAL, false);
            c->manager.destroy(vertex, true);   // the pipeline doesn't need the modules any more
            c->manager.destroy(fragment, true);
            if (!err) return err;
            if (skybox.pipeline != VK_NULL_HANDLE) c->vulkan.skyboxPipeline = c->manager.create(std::move(skybox));
        }
        err = c->vulkan.toneMap.create();
        if (!err) return err;
        err = c->vulkan.toneMap.resize(sceneExtent(c));
        if (!err) return err;

        // FunctionDrawMode draws with the bound scene's camera and lights, so give it one: the camera
        // and light functions then work without calling createScene().
        if (c->config.apiMode == ApiMode::FunctionDrawMode) {
            if (auto scene = createScene(); !scene) return std::unexpected(scene.error());
        }
        return {};
    }

    Result<void> shutdown() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        vkDeviceWaitIdle(c->vulkan.device);
        c->manager.destroyAll();
        auto  err = Texture::destroySampler();
        if (!err) return err;

        err = c->vulkan.descriptorPool.destroy();
        if (!err) return err;
        for (auto& layout : c->vulkan.descriptorLayouts) {
            err = layout.destroy();
            if (!err) return err;
        }
        destroyDepthImage(c);
        c->vulkan.toneMap.destroy();
#ifdef NRE_UNSAFE_API
        (void)c->unsafeContext.targetColor.destroyImage();
        (void)c->unsafeContext.targetDepth.destroyImage();
#endif
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

    namespace {
        // Waits for this frame slot's fence and acquires a swapchain image. Returns false when the frame was skipped
        // (the swapchain was out of date and has been recreated); nothing is recorded then.
        Result<bool> acquireFrame(Context* c) {
            Frame& f = c->vulkan.frames[c->vulkan.currentFrame % max_frames_in_flight];
            vkWaitForFences(c->vulkan.device, 1, &f.in_flight,VK_TRUE,UINT64_MAX);
            VkResult r = vkAcquireNextImageKHR(c->vulkan.device,c->vulkan.swapchain,UINT64_MAX,f.image_acquired,VK_NULL_HANDLE,&c->vulkan.currentImageIndex);
            if (r == VK_ERROR_OUT_OF_DATE_KHR) {
                if (auto rec = recreateSwapchain(); !rec) return std::unexpected(rec.error());
                return false;
            }
            vkResetFences(c->vulkan.device, 1, &f.in_flight);
            return true;
        }

        // Records the frame from drawQueue (see core/frameRecorder.h), then submits and presents it unless
        // manualFramePresentationAndSubmission is set.
        Result<void> finishFrame(Context* c, Scene* scene) {
            auto recorded = recordFrame(c, scene, c->vulkan.drawQueue);
            if (!c->config.manualFramePresentationAndSubmission) {
                submitFrame();
                presentFrame();
            }
            return recorded;
        }

        // FunctionDrawMode checks shared by the draw functions. Returns false when the call should do nothing.
        Result<bool> canDraw(Context* c, std::source_location location) {
#ifndef NDEBUG
            if (c->config.apiMode != ApiMode::FunctionDrawMode) {
                auto error = Error{ErrorCode::WrongMode, ErrorHandeling::functionName(location) + ": draw functions only work in "
                                   "ApiMode::FunctionDrawMode. In SceneMode, add instances to a scene instead."};
                ErrorHandeling::handleError(error);
                return std::unexpected(error);
            }
            if (!c->vulkan.isDrawing) {
                auto error = Error{ErrorCode::WrongMode, ErrorHandeling::functionName(location) + ": called outside a frame. "
                                   "Call renderFrame() first and endFrame() after the last draw."};
                ErrorHandeling::handleError(error);
                return std::unexpected(error);
            }
#else
            (void)location;
#endif
            return c->vulkan.frameRecording;
        }
    }

    Result<void> renderFrame() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        const bool functionMode = c->config.apiMode == ApiMode::FunctionDrawMode;
#ifndef NDEBUG
        if (functionMode && c->vulkan.isDrawing) {
            auto error = Error{ErrorCode::WrongMode, "renderFrame(): the previous frame wasn't ended. "
                               "In ApiMode::FunctionDrawMode, call endFrame() after the frame's draw calls."};
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
#endif
        // In FunctionDrawMode the frame is open until endFrame(), even if it's skipped below:
        // the draw calls then do nothing, and endFrame() just closes it.
        c->vulkan.isDrawing = functionMode;
        c->vulkan.frameRecording = false;
        c->vulkan.drawQueue.clear();

        if (!c->manager.get(c->vulkan.pipelineBound)) {
            auto error = Error{ErrorCode::NoPipelineBound, "renderFrame(): no pipeline is bound (or the bound one was destroyed), "
                               "so nothing can be drawn. Create one with createPipeline() and bind it with bindPipeline()."};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};   // skip the frame: nothing has been acquired or recorded yet
        }
        c->vulkan.framePipeline = c->vulkan.pipelineBound;
        c->vulkan.frameScene = c->currentScene;
        Scene* scene = c->manager.get(c->currentScene);
        if (!scene) {
            // Checked before acquiring: in FunctionDrawMode the camera and lights come from the scene, so there's nothing
            // to draw with. SceneMode still shows an empty frame, as it did before.
            auto error = Error{ErrorCode::NoSceneBound, "renderFrame(): no scene is bound, so the frame is empty. "
                               "Create one with createScene() or bind one with bindScene()."};
            bool report = ErrorHandeling::handleError(error);
            if (functionMode) {
                if (report) return std::unexpected(error);
                return {};
            }
            auto acquired = acquireFrame(c);
            if (!acquired) return std::unexpected(acquired.error());
            if (*acquired) (void)finishFrame(c, nullptr);
            if (report) return std::unexpected(error);
            return {};
        }

        auto acquired = acquireFrame(c);
        if (!acquired) return std::unexpected(acquired.error());
        if (!*acquired) return {};
        // Camera and lights for this frame: its fence was just waited on, so its buffers are free.
        (void)scene->writeFrameData();
        if (functionMode) {
            // The draw calls queue into drawQueue; endFrame() records them.
            c->vulkan.frameRecording = true;
            return {};
        }
        scene->appendDraws(c->vulkan.drawQueue);
        return finishFrame(c, scene);
    }

    Result<void> endFrame() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
#ifndef NDEBUG
        if (c->config.apiMode != ApiMode::FunctionDrawMode) {
            auto error = Error{ErrorCode::WrongMode, "endFrame(): only used in ApiMode::FunctionDrawMode; in SceneMode "
                               "renderFrame() draws the whole frame. Set apiMode in the RenderConfig to use draw functions."};
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
        if (!c->vulkan.isDrawing) {
            auto error = Error{ErrorCode::WrongMode, "endFrame(): no frame was started. Call renderFrame() before the draw calls."};
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
#endif
        c->vulkan.isDrawing = false;
        if (!c->vulkan.frameRecording) return {};   // renderFrame() skipped this frame
        c->vulkan.frameRecording = false;
        // A scene destroyed since renderFrame() gives an empty frame.
        return finishFrame(c, c->manager.get(c->vulkan.frameScene));
    }

    Result<void> drawModel(Handel<Model> model, const Transform& transform) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto ok = canDraw(c, std::source_location::current());
        if (!ok || !*ok) return ok.transform([](bool) {});
        // Checked now, so the error points at this call; endFrame() skips it if it's destroyed in between.
        if (!c->manager.get(model)) {
            auto error = ErrorHandeling::badHandle("model", model, "createModel()", std::source_location::current());
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        QueuedDraw& draw = c->vulkan.drawQueue.emplace_back();
        draw.model = model;
        draw.matrix = transform.matrix();
        draw.castsShadows = true;
        return {};
    }

    Result<void> drawInstance(Handel<Instance> instance) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto ok = canDraw(c, std::source_location::current());
        if (!ok || !*ok) return ok.transform([](bool) {});
        Instance* i = c->manager.get(instance);
        if (!i) {
            auto error = ErrorHandeling::badHandle("instance", instance, "createInstance()", std::source_location::current());
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        if (!i->visible) return {};
        // Like Scene: instances of a destroyed model draw nothing (endFrame() skips them).
        QueuedDraw& draw = c->vulkan.drawQueue.emplace_back();
        draw.model = i->model;
        draw.matrix = i->transform.matrix();
        draw.castsShadows = true;
        return {};
    }

    Result<void> submitFrame() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
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
        NRE_REQUIRE_CONTEXT(c);
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
        NRE_REQUIRE_CONTEXT(c);
        return {c->window.shouldClose};
    }
    Result<void> pollEvents() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        c->window.scrollDelta = 0.0f;   // scrollCallback adds this frame's scrolling during glfwPollEvents
        c->window.keysPressed.reset();
        c->window.keysReleased.reset();
        c->window.buttonsPressed.reset();
        c->window.buttonsReleased.reset();
        glfwPollEvents();
        glm::dvec2 cursor;
        glfwGetCursorPos(c->window.window, &cursor.x, &cursor.y);
        c->window.cursorDelta = glm::vec2(cursor - c->window.cursor);
        c->window.cursor = cursor;
        return {};
    }
    Result<Handel<Shader>> createShader(std::string shaderPath,ShaderType type) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->manager.create(Shader(shaderPath,type));
    }

    Result<void> destroyShader(Handel<Shader> shader) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        c->manager.destroy(shader);
        return {};
    }

    Result<Handel<Pipeline> > createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->manager.create(Pipeline(vertexShader,fragmentShader,Vertex::layout()));
    }
    Result<void> destroyPipeline(Handel<Pipeline> pipeline) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        c->manager.destroy(pipeline);
        return {};
    }

    Result<void> bindPipeline(Handel<Pipeline> pipeline) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto pipeLine = c->manager.get(pipeline);
        if (!pipeLine) {
            auto error = ErrorHandeling::badHandle("pipeline", pipeline, "createPipeline()", std::source_location::current());
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};   // keep the pipeline that was bound
        }
        c->vulkan.pipelineBound = pipeline;
        return {};
    }

    Result<Handel<Model>> createModel(std::string modelPath) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
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
        NRE_REQUIRE_CONTEXT(c);
        // Instances keep the stale handle; Scene::draw skips them.
        if (Model* m = c->manager.get(model)) {
            // Frames in flight may still be reading the vertex/index buffers.
            vkDeviceWaitIdle(c->vulkan.device);
            m->releaseGpuMeshes();
        }
        c->manager.destroy(model);
        return {};
    }


    namespace {
        Result<Model*> getModel(Handel<Model> handle, std::source_location location = std::source_location::current()) {
            auto c = context();
            NRE_REQUIRE_CONTEXT_AT(c, location);
            Model* model = c->manager.get(handle);
            if (!model) {
                auto error = ErrorHandeling::badHandle("model", handle, "createModel()", location);
                ErrorHandeling::handleError(error);
                return std::unexpected(error);
            }
            return model;
        }

        Result<Instance*> getInstance(Handel<Instance> handle, std::source_location location = std::source_location::current()) {
            auto c = context();
            NRE_REQUIRE_CONTEXT_AT(c, location);
            Instance* instance = c->manager.get(handle);
            if (!instance) {
                auto error = ErrorHandeling::badHandle("instance", handle, "createInstance()", location);
                ErrorHandeling::handleError(error);
                return std::unexpected(error);
            }
            return instance;
        }
    }

    Result<Handel<Instance>> createInstance(Handel<Model> model) {
        auto m = getModel(model);
        if (!m) return std::unexpected(m.error());
        return context()->manager.create(Instance{model});
    }

    Result<void> destroyInstance(Handel<Instance> instance) {
        auto i = getInstance(instance);
        if (!i) return std::unexpected(i.error());
        auto c = context();
        for (Handel<Scene> scene : (*i)->scenes) {
            if (Scene* s = c->manager.get(scene)) s->removeInstance(instance);
        }
        c->manager.destroy(instance);
        return {};
    }

    Result<void> setInstanceTransform(Handel<Instance> instance, const Transform& transform) {
        return getInstance(instance).transform([&](Instance* i) { i->transform = transform; });
    }

    Result<Transform> getInstanceTransform(Handel<Instance> instance) {
        return getInstance(instance).transform([](Instance* i) { return i->transform; });
    }

    Result<glm::mat4> getInstanceMatrix(Handel<Instance> instance) {
        return getInstance(instance).transform([](Instance* i) { return i->transform.matrix(); });
    }

    Result<void> translateInstance(Handel<Instance> instance, glm::vec3 offset) {
        return getInstance(instance).transform([&](Instance* i) { i->transform.position += offset; });
    }

    Result<void> rotateInstance(Handel<Instance> instance, float angle, glm::vec3 axis) {
        return getInstance(instance).transform([&](Instance* i) {
            i->transform.rotation = glm::normalize(glm::angleAxis(angle, glm::normalize(axis)) * i->transform.rotation);
        });
    }

    Result<void> scaleInstance(Handel<Instance> instance, glm::vec3 factor) {
        return getInstance(instance).transform([&](Instance* i) { i->transform.scale *= factor; });
    }

    Result<void> setVisibility(Handel<Instance> instance, bool visible) {
        return getInstance(instance).transform([&](Instance* i) { i->visible = visible; });
    }

    namespace {
        Result<Scene*> getScene(Handel<Scene> handle, std::source_location location = std::source_location::current()) {
            auto c = context();
            NRE_REQUIRE_CONTEXT_AT(c, location);
            Scene* scene = c->manager.get(handle);
            if (!scene) {
                auto error = ErrorHandeling::badHandle("scene", handle, "createScene()", location);
                ErrorHandeling::handleError(error);
                return std::unexpected(error);
            }
            return scene;
        }

        Result<Scene*> getBoundScenePtr(std::source_location location = std::source_location::current()) {
            auto c = context();
            NRE_REQUIRE_CONTEXT_AT(c, location);
            Scene* scene = c->manager.get(c->currentScene);
            if (!scene) {
                auto error = Error{ErrorCode::NoSceneBound, ErrorHandeling::functionName(location) + ": no scene is bound. "
                                   "Create one with createScene() (the first one is bound automatically) or bind one with bindScene()."};
                ErrorHandeling::handleError(error);
                return std::unexpected(error);
            }
            return scene;
        }

        Result<Camera*> getCamera(std::source_location location = std::source_location::current()) {
            return getBoundScenePtr(location).transform([](Scene* scene) { return &scene->camera; });
        }
    }

    Result<Handel<Scene>> createScene() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        Scene scene{};
        auto err = scene.createUboAndLights();
        if (!err) return std::unexpected(err.error());
        Handel<Scene> sceneHandel = c->manager.create(std::move(scene));
        if (!c->manager.get(c->currentScene)) c->currentScene = sceneHandel;
        return sceneHandel;
    }

    Result<void> destroyScene(Handel<Scene> scene) {
        auto s = getScene(scene);
        if (!s) return std::unexpected(s.error());
        auto c = context();
        for (Handel<Instance> instance : (*s)->getInstances()) {
            if (Instance* i = c->manager.get(instance)) std::erase(i->scenes, scene);
        }
        for (Handel<Light> light : (*s)->getLights()) {
            if (Light* l = c->manager.get(light)) std::erase(l->scenes, scene);
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

    Result<void> addInstanceToScene(Handel<Instance> instance) {
        auto s = getBoundScenePtr();
        if (!s) return std::unexpected(s.error());
        auto i = getInstance(instance);
        if (!i) return std::unexpected(i.error());
        Handel<Scene> scene = context()->currentScene;
        (*s)->addInstance(instance);
        if (std::ranges::find((*i)->scenes, scene) == (*i)->scenes.end()) (*i)->scenes.push_back(scene);
        return {};
    }

    Result<void> removeInstanceFromScene(Handel<Instance> instance) {
        auto s = getBoundScenePtr();
        if (!s) return std::unexpected(s.error());
        auto i = getInstance(instance);
        if (!i) return std::unexpected(i.error());
        (*s)->removeInstance(instance);
        std::erase((*i)->scenes, context()->currentScene);
        return {};
    }

    namespace {
        Result<Light*> getLight(Handel<Light> handle, std::source_location location = std::source_location::current()) {
            auto c = context();
            NRE_REQUIRE_CONTEXT_AT(c, location);
            Light* light = c->manager.get(handle);
            if (!light) {
                auto error = ErrorHandeling::badHandle("light", handle, "createDirectionalLight() or createPointLight()", location);
                ErrorHandeling::handleError(error);
                return std::unexpected(error);
            }
            return light;
        }

        // The shader assumes a unit direction; a zero vector has none, so it falls back to straight down.
        glm::vec3 lightDirection(glm::vec3 direction) {
            return glm::length(direction) > 0.0f ? glm::normalize(direction) : glm::vec3(0.0f, -1.0f, 0.0f);
        }
    }

    Result<Handel<Light>> createDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        Light light;
        light.directional = true;
        light.direction = lightDirection(direction);
        light.color = color;
        light.intensity = intensity;
        return c->manager.create(std::move(light));
    }

    Result<Handel<Light>> createPointLight(glm::vec3 position, glm::vec3 color, float intensity) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        Light light;
        light.directional = false;
        light.position = position;
        light.color = color;
        light.intensity = intensity;
        return c->manager.create(std::move(light));
    }

    Result<void> destroyLight(Handel<Light> light) {
        auto l = getLight(light);
        if (!l) return std::unexpected(l.error());
        auto c = context();
        for (Handel<Scene> scene : (*l)->scenes) {
            if (Scene* s = c->manager.get(scene)) s->removeLight(light);
        }
        c->manager.destroy(light);
        return {};
    }

    Result<void> addLightToScene(Handel<Light> light) {
        auto s = getBoundScenePtr();
        if (!s) return std::unexpected(s.error());
        auto l = getLight(light);
        if (!l) return std::unexpected(l.error());
        Handel<Scene> scene = context()->currentScene;
        (*s)->addLight(light);
        if (std::ranges::find((*l)->scenes, scene) == (*l)->scenes.end()) (*l)->scenes.push_back(scene);
        return {};
    }

    Result<void> removeLightFromScene(Handel<Light> light) {
        auto s = getBoundScenePtr();
        if (!s) return std::unexpected(s.error());
        auto l = getLight(light);
        if (!l) return std::unexpected(l.error());
        (*s)->removeLight(light);
        std::erase((*l)->scenes, context()->currentScene);
        return {};
    }

    Result<void> setLightPosition(Handel<Light> light, glm::vec3 position) {
        return getLight(light).transform([&](Light* l) { l->position = position; });
    }

    Result<void> setLightDirection(Handel<Light> light, glm::vec3 direction) {
        return getLight(light).transform([&](Light* l) { l->direction = lightDirection(direction); });
    }

    Result<void> setLightColor(Handel<Light> light, glm::vec3 color, float intensity) {
        return getLight(light).transform([&](Light* l) {
            l->color = color;
            l->intensity = intensity;
        });
    }

    Result<void> setSun(glm::vec3 direction, glm::vec3 color, float intensity) {
        return getBoundScenePtr().transform([&](Scene* s) {
            s->sunDirection = lightDirection(direction);
            s->sunColor = color;
            s->sunIntensity = intensity;
        });
    }

    Result<void> setSunDirection(glm::vec3 direction) {
        return getBoundScenePtr().transform([&](Scene* s) { s->sunDirection = lightDirection(direction); });
    }

    Result<glm::vec3> getSunDirection() {
        return getBoundScenePtr().transform([](Scene* s) { return s->sunDirection; });
    }

    Result<void> setExposure(float exposure) {
        auto s = getBoundScenePtr();
        if (!s) return std::unexpected(s.error());
#ifndef NDEBUG
        if (!(exposure > 0.0f)) {   // also catches NaN
            auto error = Error{ErrorCode::InvalidArgument, "setExposure(): exposure must be greater than 0, got "
                               + std::to_string(exposure) + ". 1 is the default, 2 is twice as bright, 0.5 half."};
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
#endif
        (*s)->exposure = exposure;
        return {};
    }

    Result<void> setSkyColor(glm::vec3 color) {
        return getBoundScenePtr().transform([&](Scene* s) { s->skyColor = color; });
    }

    Result<Handel<Cubemap>> createCubemap(std::string right, std::string left, std::string top,
                                          std::string bottom, std::string front, std::string back) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        Cubemap cubemap;
        if (auto loaded = cubemap.load({std::move(right), std::move(left), std::move(top), std::move(bottom),
                                        std::move(front), std::move(back)}); !loaded) {
            return std::unexpected(loaded.error());
        }
        // RenderErrorPolicy::Ignore: a failed load returns success without an image. The empty handle means no skybox.
        if (cubemap.getView() == VK_NULL_HANDLE) return Handel<Cubemap>{};
        return c->manager.create(std::move(cubemap));
    }

    Result<void> destroyCubemap(Handel<Cubemap> cubemap) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        if (!c->manager.get(cubemap)) return {};
        // Frames in flight may still sample it. Scenes using it notice the stale handle and drop the skybox.
        vkDeviceWaitIdle(c->vulkan.device);
        c->manager.destroy(cubemap, true);
        return {};
    }

    Result<void> setSkybox(Handel<Cubemap> cubemap) {
        auto s = getBoundScenePtr();
        if (!s) return std::unexpected(s.error());
        if (cubemap != Handel<Cubemap>{} && !context()->manager.get(cubemap)) {
            auto error = ErrorHandeling::badHandle("cubemap", cubemap, "createCubemap()", std::source_location::current());
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
        (*s)->skybox = cubemap;   // bindFrameData points each frame's set at it once that frame is free
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
        NRE_REQUIRE_CONTEXT(c);
        return static_cast<float>(c->timer.fps);
    }

    Result<float> getDeltaTime() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return static_cast<float>(c->timer.deltaTime);
    }

    Result<bool> isMouseButtonDown(MouseButton button) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        static_assert(int(MouseButton::Left) == GLFW_MOUSE_BUTTON_LEFT && int(MouseButton::Right) == GLFW_MOUSE_BUTTON_RIGHT
                      && int(MouseButton::Middle) == GLFW_MOUSE_BUTTON_MIDDLE);
        return glfwGetMouseButton(c->window.window, int(button)) == GLFW_PRESS;
    }

    Result<glm::vec2> getMouseDelta() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->window.cursorDelta;
    }

    Result<float> getScrollDelta() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->window.scrollDelta;
    }

    Result<bool> isMouseButtonPressed(MouseButton button) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->window.buttonsPressed[int(button)];
    }

    Result<bool> isMouseButtonReleased(MouseButton button) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->window.buttonsReleased[int(button)];
    }

    Result<glm::vec2> getMousePosition() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return glm::vec2(c->window.cursor);
    }

    Result<void> setCursorMode(CursorMode mode) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        int glfwMode = GLFW_CURSOR_NORMAL;
        switch (mode) {
            case CursorMode::Normal: glfwMode = GLFW_CURSOR_NORMAL; break;
            case CursorMode::Hidden: glfwMode = GLFW_CURSOR_HIDDEN; break;
            case CursorMode::Locked: glfwMode = GLFW_CURSOR_DISABLED; break;
        }
        glfwSetInputMode(c->window.window, GLFW_CURSOR, glfwMode);
        // Locking (and unlocking) moves the cursor, so start over from where it is now: otherwise the next
        // getMouseDelta would include that jump.
        glfwGetCursorPos(c->window.window, &c->window.cursor.x, &c->window.cursor.y);
        return {};
    }

    // Key's values are GLFW's key codes, so they're passed straight through. Checks the first and last key
    // of every run of consecutive values.
    static_assert(int(Key::Space) == GLFW_KEY_SPACE && int(Key::Apostrophe) == GLFW_KEY_APOSTROPHE
                  && int(Key::Comma) == GLFW_KEY_COMMA && int(Key::Slash) == GLFW_KEY_SLASH
                  && int(Key::Zero) == GLFW_KEY_0 && int(Key::Nine) == GLFW_KEY_9
                  && int(Key::Semicolon) == GLFW_KEY_SEMICOLON && int(Key::Equal) == GLFW_KEY_EQUAL
                  && int(Key::A) == GLFW_KEY_A && int(Key::Z) == GLFW_KEY_Z
                  && int(Key::LeftBracket) == GLFW_KEY_LEFT_BRACKET && int(Key::RightBracket) == GLFW_KEY_RIGHT_BRACKET
                  && int(Key::GraveAccent) == GLFW_KEY_GRAVE_ACCENT
                  && int(Key::Escape) == GLFW_KEY_ESCAPE && int(Key::End) == GLFW_KEY_END
                  && int(Key::CapsLock) == GLFW_KEY_CAPS_LOCK && int(Key::Pause) == GLFW_KEY_PAUSE
                  && int(Key::F1) == GLFW_KEY_F1 && int(Key::F12) == GLFW_KEY_F12
                  && int(Key::Keypad0) == GLFW_KEY_KP_0 && int(Key::KeypadEqual) == GLFW_KEY_KP_EQUAL
                  && int(Key::LeftShift) == GLFW_KEY_LEFT_SHIFT && int(Key::Menu) == GLFW_KEY_MENU);

    Result<bool> isKeyDown(Key key) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return glfwGetKey(c->window.window, int(key)) == GLFW_PRESS;
    }

    Result<bool> isKeyPressed(Key key) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->window.keysPressed[int(key)];
    }

    Result<bool> isKeyReleased(Key key) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->window.keysReleased[int(key)];
    }

    namespace {
        Result<Material*> getMaterial(Handel<Material> handle, std::source_location location = std::source_location::current()) {
            auto c = context();
            NRE_REQUIRE_CONTEXT_AT(c, location);
            Material* material = c->manager.get(handle);
            if (!material) {
                auto error = ErrorHandeling::badHandle("material", handle, "createMaterial()", location);
                ErrorHandeling::handleError(error);
                return std::unexpected(error);
            }
            return material;
        }
    }

    Result<Handel<Texture>> createTexture(std::string path) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        Texture texture;
        if (auto loaded = texture.loadTexture(path); !loaded) return std::unexpected(loaded.error());
        // With RenderErrorPolicy::Ignore a failed load returns success without an image; the empty handle
        // makes materials fall back to the default texture.
        if (texture.getView() == VK_NULL_HANDLE) return Handel<Texture>{};
        return c->manager.create(std::move(texture));
    }

    Result<void> destroyTexture(Handel<Texture> texture) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        c->manager.destroy(texture);
        return {};
    }

    Result<void> destroyMaterial(Handel<Material> material) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        // The descriptor set stays allocated: the pool can't free single sets.
        c->manager.destroy(material);
        return {};
    }

    Result<void> setMaterialBaseTexture(Handel<Material> material, Handel<Texture> texture) {
        auto m = getMaterial(material);
        if (!m) return std::unexpected(m.error());
        auto c = context();
        Handel<Texture> base = texture == Handel<Texture>{} ? c->vulkan.defaultTexture : texture;
        Texture* t = c->manager.get(base);
        if (!t) {
            auto error = ErrorHandeling::badHandle("texture", texture, "createTexture()", std::source_location::current());
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
        // Frames in flight may still be reading this set, and a set can't change while the GPU uses it.
        vkDeviceWaitIdle(c->vulkan.device);
        (*m)->baseTexture = base;
        return writeImageDescriptor((*m)->descriptorSet, 0, t->getView(), c->vulkan.sampler);
    }

    Result<std::vector<Handel<Material>>> getModelMaterials(Handel<Model> model) {
        return getModel(model).transform([](Model* m) { return m->materials; });
    }

    Result<void> setModelMaterial(Handel<Model> model, Handel<Material> material) {
        auto mat = getMaterial(material);
        if (!mat) return std::unexpected(mat.error());
        return getModel(model).transform([&](Model* m) {
            // The old materials stay in the resource manager; other models may be using them.
            m->materials = {material};
            for (Mesh& mesh : m->meshes) mesh.material = 0;
        });
    }

    Result<void> replaceModelMaterial(Handel<Model> model, uint32_t index, Handel<Material> material) {
        auto mat = getMaterial(material);
        if (!mat) return std::unexpected(mat.error());
        auto m = getModel(model);
        if (!m) return std::unexpected(m.error());
        if (index >= (*m)->materials.size()) {
            auto error = Error{ErrorCode::IndexOutOfRange,"replaceModelMaterial(): material slot " + std::to_string(index)
                               + " doesn't exist; the model has " + std::to_string((*m)->materials.size())
                               + " (see getModelMaterials())."};
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
        (*m)->materials[index] = material;
        return {};
    }
}

