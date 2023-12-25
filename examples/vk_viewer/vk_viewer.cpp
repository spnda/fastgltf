#include <deque>
#include <functional>
#include <iostream>
#include <string_view>

#include <TaskScheduler.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <fastgltf/base64.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/parser.hpp>

struct Viewer;
class FileLoadTask;
void rebuildSwapchain(Viewer& viewer, std::uint32_t width, std::uint32_t height);

void glfwErrorCallback(int errorCode, const char* description) {
	if (errorCode != GLFW_NO_ERROR) {
		std::cout << "GLFW error: " << errorCode;

		if (description != nullptr) {
			std::cout << ": " << description;
		}

		std::cout << '\n';
	}
}

void glfwResizeCallback(GLFWwindow* window, int width, int height) {
	if (width > 0 && height > 0) {
		auto* viewer = static_cast<Viewer*>(glfwGetWindowUserPointer(window));
		rebuildSwapchain(*viewer, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
	}
}

VkBool32 vulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT          messageSeverity,
                             VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
                             const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
                             void*                                            pUserData) {
	std::cout << pCallbackData->pMessage << '\n';
	return VK_FALSE; // Beware: VK_TRUE here and the layers will kill the app instantly.
}

class vulkan_error : public std::runtime_error {
	VkResult result;

public:
	vulkan_error(const std::string& message, VkResult result) : std::runtime_error(message), result(result) {}
	vulkan_error(const char* message, VkResult result) : std::runtime_error(message), result(result) {}

	[[nodiscard]] VkResult what_result() const noexcept { return result; }
};

template <typename T>
void checkResult(vkb::Result<T> result) noexcept(false) {
	if (!result) {
		throw vulkan_error(result.error().message(), result.vk_result());
	}
}

struct FrameSyncData {
	VkSemaphore imageAvailable;
	VkSemaphore renderingFinished;
	VkFence presentFinished;
};

struct FrameCommandPools {
	VkCommandPool pool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers;
};

static constexpr const std::size_t frameOverlap = 2;

struct Viewer {
	enki::TaskScheduler taskScheduler;

	vkb::Instance instance;
	vkb::Device device;

	//using queue_type = std::pair<std::uint32_t, VkQueue>;
	//queue_type graphicsQueue;
	//queue_type transferQueue;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue transferQueue = VK_NULL_HANDLE;

	GLFWwindow* window = nullptr;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	vkb::Swapchain swapchain;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	bool swapchainNeedsRebuild = false;

	std::vector<FrameSyncData> frameSyncData;
	std::vector<FrameCommandPools> frameCommandPools;

	fastgltf::Asset asset {};
	std::vector<std::shared_ptr<FileLoadTask>> fileLoadTasks;

	// This is the same paradigm as used by vkguide.dev. This makes sure every object
	// is properly destroyed in reverse-order to creation.
	class DeletionQueue {
		friend struct Viewer;
		std::deque<std::function<void()>> deletors;

	public:
		void push(std::function<void()>&& function) {
			deletors.emplace_back(function);
		}

		void flush() {
			for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
				(*it)();
			}
			deletors.clear();
		}
	};
	DeletionQueue deletionQueue;

	Viewer() = default;
	~Viewer() = default;

	void flushObjects() {
		vkDeviceWaitIdle(device);
		deletionQueue.flush();
	}
};

void setupVulkanInstance(Viewer& viewer) {
	if (auto result = volkInitialize(); result != VK_SUCCESS) {
		throw vulkan_error("No compatible Vulkan loader or driver found.", result);
	}

	auto version = volkGetInstanceVersion();
	if (version < VK_API_VERSION_1_1) {
		throw std::runtime_error("The Vulkan loader only supports version 1.0.");
	}

	vkb::InstanceBuilder builder;

	// Enable GLFW extensions
	{
		std::uint32_t glfwExtensionCount = 0;
		const auto* glfwExtensionArray = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		builder.enable_extensions(glfwExtensionCount, glfwExtensionArray);
	}

	auto instanceResult = builder
		.set_app_name("vk_viewer")
		.require_api_version(1, 3, 0)
		.request_validation_layers()
		.set_debug_callback(vulkanDebugCallback)
		.build();
	checkResult(instanceResult);

	viewer.instance = instanceResult.value();
	viewer.deletionQueue.push([&]() {
		vkb::destroy_instance(viewer.instance);
	});

	volkLoadInstanceOnly(viewer.instance);
}

void setupVulkanDevice(Viewer& viewer) {
	VkPhysicalDeviceVulkan13Features vulkan13Features {};
	vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	vulkan13Features.dynamicRendering = VK_TRUE;
	vulkan13Features.synchronization2 = VK_TRUE;

	vkb::PhysicalDeviceSelector selector(viewer.instance);
	auto selectionResult = selector
		.set_surface(viewer.surface)
		.set_minimum_version(1, 3) // We want Vulkan 1.3.
		.set_required_features_13(vulkan13Features)
		.require_present()
		.require_dedicated_transfer_queue()
		.select();
	checkResult(selectionResult);

	vkb::DeviceBuilder deviceBuilder(selectionResult.value());
	auto creationResult = deviceBuilder
		.build();
	checkResult(creationResult);

	viewer.device = creationResult.value();
	viewer.deletionQueue.push([&]() {
		vkb::destroy_device(viewer.device);
	});

	volkLoadDevice(viewer.device);

	auto graphicsQueue = viewer.device.get_queue(vkb::QueueType::graphics);
	checkResult(graphicsQueue);
	viewer.graphicsQueue = graphicsQueue.value();

	auto transferQueue = viewer.device.get_dedicated_queue(vkb::QueueType::transfer);
	checkResult(transferQueue);
	viewer.transferQueue = transferQueue.value();
}

void rebuildSwapchain(Viewer& viewer, std::uint32_t width, std::uint32_t height) {
	vkb::SwapchainBuilder swapchainBuilder(viewer.device);
	auto swapchainResult = swapchainBuilder
		.set_old_swapchain(viewer.swapchain)
		.build();
	checkResult(swapchainResult);

	// The swapchain is not added to the deletionQueue, as it gets recreated throughout the application's lifetime.
	vkb::destroy_swapchain(viewer.swapchain);
	viewer.swapchain = swapchainResult.value();

	auto imageResult = viewer.swapchain.get_images();
	checkResult(imageResult);
	viewer.swapchainImages = std::move(imageResult.value());

	auto imageViewResult = viewer.swapchain.get_image_views();
	checkResult(imageViewResult);
	viewer.swapchainImageViews = std::move(imageViewResult.value());
}

void createFrameData(Viewer& viewer) {
	viewer.frameSyncData.resize(frameOverlap);
	for (auto& frame : viewer.frameSyncData) {
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		auto semaphoreResult = vkCreateSemaphore(viewer.device, &semaphoreCreateInfo, nullptr, &frame.imageAvailable);
		if (semaphoreResult != VK_SUCCESS) {
			throw vulkan_error("Failed to create image semaphore", semaphoreResult);
		}
		semaphoreResult = vkCreateSemaphore(viewer.device, &semaphoreCreateInfo, nullptr, &frame.renderingFinished);
		if (semaphoreResult != VK_SUCCESS) {
			throw vulkan_error("Failed to create rendering semaphore", semaphoreResult);
		}

		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		auto fenceResult = vkCreateFence(viewer.device, &fenceCreateInfo, nullptr, &frame.presentFinished);
		if (fenceResult != VK_SUCCESS) {
			throw vulkan_error("Failed to create present fence", fenceResult);
		}

		viewer.deletionQueue.push([&]() {
			vkDestroyFence(viewer.device, frame.presentFinished, nullptr);
			vkDestroySemaphore(viewer.device, frame.renderingFinished, nullptr);
			vkDestroySemaphore(viewer.device, frame.imageAvailable, nullptr);
		});
	}

	viewer.frameCommandPools.resize(frameOverlap);
	for (auto& frame : viewer.frameCommandPools) {
		VkCommandPoolCreateInfo commandPoolInfo = {};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		// commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolInfo.queueFamilyIndex = 0;
		auto createResult = vkCreateCommandPool(viewer.device, &commandPoolInfo, nullptr, &frame.pool);
		if (createResult != VK_SUCCESS) {
			throw vulkan_error("Failed to create command pool", createResult);
		}

		VkCommandBufferAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = frame.pool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = 1;
		frame.commandBuffers.resize(1);
		auto allocateResult = vkAllocateCommandBuffers(viewer.device, &allocateInfo, frame.commandBuffers.data());
		if (allocateResult != VK_SUCCESS) {
			throw vulkan_error("Failed to allocate command buffers", allocateResult);
		}
		viewer.deletionQueue.push([&]() {
			vkDestroyCommandPool(viewer.device, frame.pool, nullptr);
		});
	}
}

class Base64DecodeTask final : public enki::ITaskSet {
	std::string_view encodedData;
	uint8_t* outputData;

public:
	// Arbitrarily chosen 1MB. Lower values will cause too many tasks to spawn, slowing down the process.
	// Perhaps even larger values would be necessary, as even this gets decoded incredibly quick and the
	// overhead of launching threaded tasks gets noticeable.
	static constexpr const size_t minBase64DecodeSetSize = 1 * 1024 * 1024; // 1MB.

	explicit Base64DecodeTask(uint32_t dataSize, std::string_view encodedData, uint8_t* outputData)
		: enki::ITaskSet(dataSize, minBase64DecodeSetSize), encodedData(encodedData), outputData(outputData) {}

	void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override {
		fastgltf::base64::decode_inplace(encodedData.substr(static_cast<size_t>(range.start) * 4, static_cast<size_t>(range.end) * 4),
		                                 &outputData[range.start * 3], 0);
	}
};

// The custom base64 callback for fastgltf to multithread base64 decoding, to divide the (possibly) large
// input buffer into smaller chunks that can be worked on by multiple threads.
void multithreadedBase64Decoding(std::string_view encodedData, uint8_t* outputData,
								 std::size_t padding, std::size_t outputSize, void* userPointer) {
	assert(fastgltf::base64::getOutputSize(encodedData.size(), padding) <= outputSize);
	assert(userPointer != nullptr);
	assert(encodedData.size() % 4 == 0);

	// Check if the data is smaller than minBase64DecodeSetSize, and if so just decode it on the main thread.
	// TaskSetPartition start and end is currently an uint32_t, so we'll check if we exceed that for safety.
	if (encodedData.size() < Base64DecodeTask::minBase64DecodeSetSize
	    || encodedData.size() > std::numeric_limits<decltype(enki::TaskSetPartition::start)>::max()) {
		fastgltf::base64::decode_inplace(encodedData, outputData, padding);
		return;
	}

	// We divide by 4 to essentially create as many sets as there are decodable base64 blocks.
	Base64DecodeTask task(encodedData.size() / 4, encodedData, outputData);
	auto* editor = static_cast<Viewer*>(userPointer);
	editor->taskScheduler.AddTaskSetToPipe(&task);

	// Finally, wait for all other tasks to finish. enkiTS will use this thread as well to process the tasks.
	editor->taskScheduler.WaitforTask(&task);
}


void loadGltf(Viewer& viewer, std::string_view file) {
	const std::filesystem::path filePath(file);

	fastgltf::GltfDataBuffer fileBuffer;
	if (!fileBuffer.loadFromFile(filePath)) {
		throw std::runtime_error("Failed to load file");
	}

	fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);
	parser.setUserPointer(&viewer);
	parser.setBase64DecodeCallback(multithreadedBase64Decoding);

	auto type = fastgltf::determineGltfFileType(&fileBuffer);
	fastgltf::Expected<fastgltf::Asset> asset(fastgltf::Error::None);
	if (type == fastgltf::GltfType::glTF) {
		asset = parser.loadGLTF(&fileBuffer, filePath.parent_path());
	} else if (type == fastgltf::GltfType::GLB) {
		asset = parser.loadBinaryGLTF(&fileBuffer, filePath.parent_path());
	} else {
		throw std::runtime_error("Failed to determine glTF container");
	}

	if (asset.error() != fastgltf::Error::None) {
		auto message = fastgltf::getErrorMessage(asset.error());
		throw std::runtime_error(std::string("Failed to load glTF") + std::string(message));
	}

	viewer.asset = std::move(asset.get());

	// We'll always do additional validation
	if (auto validation = fastgltf::validate(viewer.asset); validation != fastgltf::Error::None) {
		auto message = fastgltf::getErrorMessage(asset.error());
		throw std::runtime_error(std::string("Asset failed validation") + std::string(message));
	}
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "No gltf file specified." << '\n';
		return -1;
	}
	auto gltfFile = std::string_view { argv[1] };

	Viewer viewer {};
	viewer.taskScheduler.Initialize();

	glfwSetErrorCallback(glfwErrorCallback);

	try {
		// Initialize GLFW
		if (glfwInit() != GLFW_TRUE) {
			throw std::runtime_error("Failed to initialize glfw");
		}

		// Load the glTF asset
		loadGltf(viewer, gltfFile);

		// Setup the Vulkan instance
		setupVulkanInstance(viewer);

		// Create the window
		auto* mainMonitor = glfwGetPrimaryMonitor();
		const auto* videoMode = glfwGetVideoMode(mainMonitor);

		glfwDefaultWindowHints();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		viewer.window = glfwCreateWindow(
			static_cast<int>(static_cast<float>(videoMode->width) * 0.9f),
			static_cast<int>(static_cast<float>(videoMode->height) * 0.9f),
			"vk_viewer", nullptr, nullptr);

		if (viewer.window == nullptr) {
			throw std::runtime_error("Failed to create window");
		}

		glfwSetWindowUserPointer(viewer.window, &viewer);
		glfwSetWindowSizeCallback(viewer.window, glfwResizeCallback);

		// Create the Vulkan surface
		auto surfaceResult = glfwCreateWindowSurface(viewer.instance, viewer.window, nullptr, &viewer.surface);
		if (surfaceResult != VK_SUCCESS) {
			throw vulkan_error("Failed to create window surface", surfaceResult);
		}
		viewer.deletionQueue.push([&]() {
			vkDestroySurfaceKHR(viewer.instance, viewer.surface, nullptr);
		});

		// Create the Vulkan device
		setupVulkanDevice(viewer);

		// Create the swapchain
		rebuildSwapchain(viewer, videoMode->width, videoMode->height);

		// Creates the required fences and semaphores for frame sync
		createFrameData(viewer);

		// The render loop
		std::size_t currentFrame = 0;
		while (glfwWindowShouldClose(viewer.window) != GLFW_TRUE) {
			if (!viewer.swapchainNeedsRebuild) {
				glfwPollEvents();
			} else {
				// This will wait until we get an event, like the resize event which will recreate the swapchain.
				glfwWaitEvents();
				continue;
			}

			currentFrame = ++currentFrame % frameOverlap;
			auto& frameSyncData = viewer.frameSyncData[currentFrame];

			// Wait for the last frame with the current index to have finished presenting, so that we can start
			// using the semaphores and command buffers.
			vkWaitForFences(viewer.device, 1, &frameSyncData.presentFinished, VK_TRUE, UINT64_MAX);
			vkResetFences(viewer.device, 1, &frameSyncData.presentFinished);

			// Reset the command pool
			auto& commandPool = viewer.frameCommandPools[currentFrame];
			vkResetCommandPool(viewer.device, commandPool.pool, 0);
			auto& cmd = commandPool.commandBuffers.front();

			// Acquire the next swapchain image
			std::uint32_t swapchainImageIndex = 0;
			auto acquireResult = vkAcquireNextImageKHR(viewer.device, viewer.swapchain, UINT64_MAX,
			                                           frameSyncData.imageAvailable,
													   VK_NULL_HANDLE, &swapchainImageIndex);
			if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
				viewer.swapchainNeedsRebuild = true;
				continue;
			}
			if (acquireResult != VK_SUCCESS) {
				throw vulkan_error("Failed to acquire swapchain image", acquireResult);
			}

			// Begin the command buffer
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // We're only using once, then resetting.
			vkBeginCommandBuffer(cmd, &beginInfo);

			// Transition the swapchain image from (possibly) UNDEFINED -> PRESENT_SRC_KHR
			VkImageMemoryBarrier2 swapchainImageBarrier {};
			swapchainImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			swapchainImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
			swapchainImageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
			swapchainImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
			swapchainImageBarrier.dstAccessMask = VK_ACCESS_2_NONE;
			swapchainImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			swapchainImageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			swapchainImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			swapchainImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			swapchainImageBarrier.image = viewer.swapchainImages[swapchainImageIndex];
			swapchainImageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			swapchainImageBarrier.subresourceRange.levelCount = 1;
			swapchainImageBarrier.subresourceRange.layerCount = 1;

			VkDependencyInfo dependencyInfo {};
			dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			dependencyInfo.imageMemoryBarrierCount = 1;
			dependencyInfo.pImageMemoryBarriers = &swapchainImageBarrier;
			vkCmdPipelineBarrier2(cmd, &dependencyInfo);

			vkEndCommandBuffer(cmd);

			// Submit the command buffer
			const VkPipelineStageFlags submitWaitStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			VkSubmitInfo submitInfo {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &frameSyncData.imageAvailable;
			submitInfo.pWaitDstStageMask = &submitWaitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &frameSyncData.renderingFinished;
			auto submitResult = vkQueueSubmit(viewer.graphicsQueue, 1, &submitInfo, frameSyncData.presentFinished);
			if (submitResult != VK_SUCCESS) {
				throw vulkan_error("Failed to submit to queue", submitResult);
			}

			// Present the rendered image
			VkPresentInfoKHR presentInfo {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &frameSyncData.renderingFinished;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &viewer.swapchain.swapchain;
			presentInfo.pImageIndices = &swapchainImageIndex;
			auto presentResult = vkQueuePresentKHR(viewer.graphicsQueue, &presentInfo);
			if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
				viewer.swapchainNeedsRebuild = true;
				continue;
			}
			if (presentResult != VK_SUCCESS) {
				throw vulkan_error("Failed to present to queue", presentResult);
			}
		}
	} catch (const vulkan_error& error) {
		std::cerr << error.what() << ": " << error.what_result() << '\n';
	} catch (const std::runtime_error& error) {
		std::cerr << error.what() << '\n';
	}

	vkDeviceWaitIdle(viewer.device); // Make sure everything is done

	viewer.taskScheduler.WaitforAll();

	// Destroys everything. We leave this out of the try-catch block to make sure it gets executed.
	// The swapchain is the only exception, as that gets recreated within the render loop. Managing it
	// with this paradigm is quite hard.
	viewer.swapchain.destroy_image_views(viewer.swapchainImageViews);
	vkb::destroy_swapchain(viewer.swapchain);
	viewer.flushObjects();
	glfwDestroyWindow(viewer.window);
	glfwTerminate();

	viewer.taskScheduler.WaitforAllAndShutdown();

	return 0;
}
