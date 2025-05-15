#include <iostream>  // For std::cout, std::cerr, std::endl
#include <stdexcept> // For std::runtime_error
#include <vector>    // For std::vector
#include <string>    // For std::string
#include <cstring>   // For strcmp
#include <optional>  // For std::optional (used in QueueFamilyIndices)
#include <set>       // For std::set to manage unique device extensions
#include <algorithm> // For std::min, std::max, std::clamp
#include <limits>    // For std::numeric_limits
#include <fstream>   // For std::ifstream to read shader files
#include <filesystem> // For std::filesystem::current_path() (C++17)

// SDL2: Needs to be included before Vulkan to prevent potential conflicts on some platforms,
// and SDL_vulkan.h needs SDL.h
#define SDL_MAIN_HANDLED // Tell SDL not to provide its own main function, we have our own.
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

// Vulkan
#include <vulkan/vulkan.h>

// Define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME if not already defined by headers.
#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

// Window dimensions
const int INITIAL_WINDOW_WIDTH = 800;
const int INITIAL_WINDOW_HEIGHT = 600;
const int MAX_FRAMES_IN_FLIGHT = 2; // For synchronization objects

// Validation Layers (for debugging)
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Required device extensions
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG // NDEBUG is defined in release builds
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// --- Helper function to read shader files ---
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::string currentPathStr = "";
        try {
            currentPathStr = std::filesystem::current_path().string();
        } catch (const std::filesystem::filesystem_error& e) {
            currentPathStr = "(failed to get current path: " + std::string(e.what()) + ")";
        }
        throw std::runtime_error("Failed to open shader file: " + filename + " (tried from CWD: " + currentPathStr + ")");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    std::cout << "Successfully read shader file: " << filename << " (" << fileSize << " bytes)" << std::endl;
    return buffer;
}


// --- Debug Messenger Setup ---
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    std::cerr << "Validation layer: ";
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) { std::cerr << "[WARNING or ERROR] "; }
    if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) { std::cerr << "[GENERAL] "; }
    else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) { std::cerr << "[VALIDATION] "; }
    else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) { std::cerr << "[PERFORMANCE] "; }
    std::cerr << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    std::cout << "Available validation layers:" << std::endl;
    for (const auto& layer : availableLayers) {
        std::cout << "\t" << layer.layerName << std::endl;
    }
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            std::cerr << "Validation layer not found: " << layerName << std::endl;
            return false;
        }
    }
    std::cout << "All requested validation layers are available." << std::endl;
    return true;
}

std::vector<const char*> getRequiredExtensions() {
    unsigned int sdlExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlExtensionCount, nullptr)) {
        throw std::runtime_error("Failed to get SDL Vulkan instance extension count! SDL_Error: " + std::string(SDL_GetError()));
    }
    std::vector<const char*> sdlExtensions(sdlExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlExtensionCount, sdlExtensions.data())) {
        throw std::runtime_error("Failed to get SDL Vulkan instance extensions! SDL_Error: " + std::string(SDL_GetError()));
    }
    std::cout << "SDL required Vulkan instance extensions:" << std::endl;
    for (const char* extName : sdlExtensions) {
        std::cout << "\t" << extName << std::endl;
    }
    #if defined(__APPLE__) || defined(__MACH__)
        sdlExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        std::cout << "\tAdded VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME for macOS." << std::endl;
    #endif
    if (enableValidationLayers) {
        sdlExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
         std::cout << "\tAdded VK_EXT_DEBUG_UTILS_EXTENSION_NAME for validation layers." << std::endl;
    }
    return sdlExtensions;
}

// Structure to hold queue family indices
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

// Structure to hold swap chain support details
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};


class EngineApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    SDL_Window* window = nullptr;
    int windowWidth = INITIAL_WINDOW_WIDTH;
    int windowHeight = INITIAL_WINDOW_HEIGHT;
    bool framebufferResized = false; // Flag to indicate if window was resized

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE; 
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE; 
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkCommandPool commandPool = VK_NULL_HANDLE; 
    std::vector<VkCommandBuffer> commandBuffers; 

    // Synchronization objects
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0; // To keep track of the current frame for sync objects


    void initWindow() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::runtime_error("Failed to initialize SDL! SDL_Error: " + std::string(SDL_GetError()));
        }
        window = SDL_CreateWindow( "IWEngine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (window == nullptr) {
            SDL_Quit();
            throw std::runtime_error("Failed to create SDL window! SDL_Error: " + std::string(SDL_GetError()));
        }
        std::cout << "SDL Window created successfully." << std::endl;
    }

    void initVulkan() {
        std::cout << "Initializing Vulkan..." << std::endl;
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline(); 
        createFramebuffers(); 
        createCommandPool(); 
        createCommandBuffers(); 
        createSyncObjects(); 
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "IWEngine";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "IWEngineCore";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_1; 

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        auto requiredExtensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
            createInfo.pNext = nullptr;
        }
        #if defined(__APPLE__) || defined(__MACH__)
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        #endif
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance! Error code: " + std::to_string(result));
        }
        std::cout << "Vulkan Instance created successfully." << std::endl;
    }

    void setupDebugMessenger() { 
        if (!enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up debug messenger!");
        }
        std::cout << "Vulkan Debug Messenger set up successfully." << std::endl;
    }

    void createSurface() {
        if (SDL_Vulkan_CreateSurface(window, instance, &surface) != SDL_TRUE) {
            throw std::runtime_error("Failed to create window surface using SDL! SDL_Error: " + std::string(SDL_GetError()));
        }
        std::cout << "Vulkan window surface created successfully." << std::endl;
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice queryDevice) {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(queryDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(queryDevice, &queueFamilyCount, queueFamilies.data());
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(queryDevice, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
            if (indices.isComplete()) {
                break;
            }
            i++;
        }
        return indices;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice queryDevice) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(queryDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(queryDevice, nullptr, &extensionCount, availableExtensions.data());
        std::set<std::string> requiredDeviceExts(deviceExtensions.begin(), deviceExtensions.end());
        #if defined(__APPLE__) || defined(__MACH__)
            requiredDeviceExts.insert(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        #endif
        for (const auto& extension : availableExtensions) {
            requiredDeviceExts.erase(extension.extensionName);
        }
        if (!requiredDeviceExts.empty()) {
            std::cout << "\tMissing required device extensions for " << queryDevice << ":" << std::endl;
            for(const auto& ext : requiredDeviceExts) {
                std::cout << "\t\t" << ext << std::endl;
            }
        }
        return requiredDeviceExts.empty();
    }
    
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice queryDevice) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(queryDevice, surface, &details.capabilities);
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(queryDevice, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(queryDevice, surface, &formatCount, details.formats.data());
        }
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(queryDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(queryDevice, surface, &presentModeCount, details.presentModes.data());
        }
        return details;
    }

    bool isDeviceSuitable(VkPhysicalDevice queryDevice) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(queryDevice, &deviceProperties);
        QueueFamilyIndices indices = findQueueFamilies(queryDevice);
        bool extensionsSupported = checkDeviceExtensionSupport(queryDevice); 
        bool swapChainAdequate = false;
        if (extensionsSupported) { 
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(queryDevice);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }
        std::cout << "Evaluating device: " << deviceProperties.deviceName << std::endl;
        std::cout << "\tType: ";
        switch(deviceProperties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: std::cout << "Integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   std::cout << "Discrete GPU"; break;
            default:                                     std::cout << "Other (" << deviceProperties.deviceType << ")"; break;
        }
        std::cout << " (API Version: " << VK_VERSION_MAJOR(deviceProperties.apiVersion) << "."
                  << VK_VERSION_MINOR(deviceProperties.apiVersion) << "."
                  << VK_VERSION_PATCH(deviceProperties.apiVersion) << ")" << std::endl;
        std::cout << "\tSupports graphics queue: " << (indices.graphicsFamily.has_value() ? "Yes" : "No") << std::endl;
        std::cout << "\tSupports present queue: " << (indices.presentFamily.has_value() ? "Yes" : "No") << std::endl;
        std::cout << "\tSupports required device extensions: " << (extensionsSupported ? "Yes" : "No") << std::endl;
        std::cout << "\tSwap chain adequate: " << (swapChainAdequate ? "Yes" : "No") << std::endl;
        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        std::cout << "Found " << deviceCount << " physical device(s):" << std::endl;
        for (const auto& currentDevice : devices) {
            if (isDeviceSuitable(currentDevice)) {
                physicalDevice = currentDevice;
                break;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        std::cout << "Selected physical device: " << properties.deviceName << std::endl;
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value()};
        if (indices.presentFamily.has_value()) { 
             uniqueQueueFamilies.insert(indices.presentFamily.value());
        }
        float queuePriority = 1.0f;
        for (uint32_t queueFamilyIndex : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }
        VkPhysicalDeviceFeatures deviceFeatures{}; 
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        std::vector<const char*> enabledDeviceExtensionsVec = deviceExtensions; 
        #if defined(__APPLE__) || defined(__MACH__)
            uint32_t extCount;
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
            std::vector<VkExtensionProperties> availExtensions(extCount);
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availExtensions.data());
            bool portabilitySubsetSupported = false;
            for(const auto& ext : availExtensions) {
                if (strcmp(ext.extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0) {
                    portabilitySubsetSupported = true;
                    break;
                }
            }
            if (portabilitySubsetSupported) {
                enabledDeviceExtensionsVec.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
                std::cout << "Enabling device extension for logical device: " << VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME << std::endl;
            } else {
                 std::cout << "Warning: Device extension " << VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME << " not supported by selected physical device, though typically expected on macOS if portability enumeration was used." << std::endl;
            }
        #endif
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensionsVec.size());
        createInfo.ppEnabledExtensionNames = enabledDeviceExtensionsVec.data();
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }
        std::cout << "Logical device created successfully." << std::endl;
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
        std::cout << "Graphics and Present queue handles retrieved." << std::endl;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                std::cout << "Chosen swap surface format: B8G8R8A8_SRGB, Color space: SRGB_NONLINEAR_KHR" << std::endl;
                return availableFormat;
            }
        }
        if (!availableFormats.empty()) {
            std::cout << "Chosen swap surface format (fallback): " << availableFormats[0].format << ", Color space: " << availableFormats[0].colorSpace << std::endl;
            return availableFormats[0];
        }
        throw std::runtime_error("No suitable surface formats found!");
    }
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                std::cout << "Chosen swap present mode: MAILBOX_KHR" << std::endl;
                return availablePresentMode;
            }
        }
        std::cout << "Chosen swap present mode (fallback): FIFO_KHR" << std::endl;
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            std::cout << "Chosen swap extent (from surface capabilities): " << capabilities.currentExtent.width << "x" << capabilities.currentExtent.height << std::endl;
            return capabilities.currentExtent;
        } else {
            int currentWidth, currentHeight;
            SDL_Vulkan_GetDrawableSize(window, &currentWidth, &currentHeight); 
            VkExtent2D actualExtent = { static_cast<uint32_t>(currentWidth), static_cast<uint32_t>(currentHeight) };
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            std::cout << "Chosen swap extent (from window drawable size, clamped): " << actualExtent.width << "x" << actualExtent.height << std::endl;
            return actualExtent;
        }
    }

    void createSwapChain() { // This is the function that was cut off
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }
        std::cout << "Requesting swap chain image count: " << imageCount << std::endl;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndicesArr[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndicesArr;
            std::cout << "Swap chain image sharing mode: CONCURRENT" << std::endl;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            std::cout << "Swap chain image sharing mode: EXCLUSIVE" << std::endl;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE; 

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain!");
        }
        std::cout << "Vulkan Swap Chain created successfully." << std::endl;

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr); 
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        std::cout << "Retrieved " << imageCount << " swap chain images." << std::endl;

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view for swap chain image " + std::to_string(i) + "!");
            }
        }
        std::cout << "Swap chain image views created successfully (" << swapChainImageViews.size() << ")." << std::endl;
    }

    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; 
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; 

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0; 
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; 
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef; 

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL; 
        dependency.dstSubpass = 0;                   
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; 
        dependency.srcAccessMask = 0;                
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; 
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; 

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
        std::cout << "Vulkan Render Pass created successfully." << std::endl;
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }
        std::cout << "Shader module created successfully." << std::endl;
        return shaderModule;
    }

    void createGraphicsPipeline() {
        std::cout << "Current working directory when creating graphics pipeline: " 
                  << std::filesystem::current_path() << std::endl;
        
        // Corrected relative path for shaders, assuming CWD is project root
        auto vertShaderCode = readFile("build/bin/shaders/vert.spv"); 
        auto fragShaderCode = readFile("build/bin/shaders/frag.spv"); 

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; 

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; 
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; 

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; 
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE; 
        rasterizer.rasterizerDiscardEnable = VK_FALSE; 
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL; 
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; 
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; 
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; 

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE; 
        colorBlending.logicOp = VK_LOGIC_OP_COPY; 
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; 
        pipelineLayoutInfo.pSetLayouts = nullptr; 
        pipelineLayoutInfo.pushConstantRangeCount = 0; 
        pipelineLayoutInfo.pPushConstantRanges = nullptr; 

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }
        std::cout << "Pipeline Layout created successfully." << std::endl;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2; 
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil; 
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; 
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass; 
        pipelineInfo.subpass = 0; 
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; 
        pipelineInfo.basePipelineIndex = -1; 

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
        std::cout << "Graphics Pipeline created successfully." << std::endl;

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        std::cout << "Shader modules destroyed." << std::endl;
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = { swapChainImageViews[i] };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer " + std::to_string(i) + "!");
            }
        }
        std::cout << "Swap chain framebuffers created successfully (" << swapChainFramebuffers.size() << ")." << std::endl;
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; 
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }
        std::cout << "Command Pool created successfully." << std::endl;
    }

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT); 
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; 
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
        std::cout << "Command Buffers allocated successfully (" << commandBuffers.size() << ")." << std::endl;
    }
    
    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; 
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create synchronization objects for a frame!");
            }
        }
        std::cout << "Synchronization objects (semaphores and fences) created successfully." << std::endl;
    }


    void mainLoop() {
        bool bShouldClose = false;
        SDL_Event e;
        while (!bShouldClose) {
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    bShouldClose = true;
                }
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    SDL_GetWindowSize(window, &windowWidth, &windowHeight); 
                    std::cout << "Window resized to: " << windowWidth << "x" << windowHeight << std::endl;
                    framebufferResized = true; 
                }
            }
            drawFrame(); 
        }
        vkDeviceWaitIdle(device); 
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex]; 
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}}; 
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapChainExtent.width);
            viewport.height = static_cast<float>(swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swapChainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdDraw(commandBuffer, 3, 1, 0, 0); 

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain(); 
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }
        
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0); 
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; 
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores; 

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        
        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain(); 
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void recreateSwapChain() {
        // Handle minimization (pause rendering if window is 0x0)
        int width = 0, height = 0;
        SDL_GetWindowSize(window, &width, &height);
        while (width == 0 || height == 0) {
            SDL_GetWindowSize(window, &width, &height);
            SDL_WaitEvent(nullptr); // Wait for an event to avoid busy-looping
        }
        
        vkDeviceWaitIdle(device); // Wait until current operations are finished

        std::cout << "Recreating swap chain..." << std::endl;

        // Cleanup old resources (order matters)
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        swapChainFramebuffers.clear();

        // Graphics pipeline and render pass might need recreation if format/extent changes significantly,
        // but for a simple case, often only swap chain related resources are enough.
        // For robustness, they are often recreated.
        if (graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, graphicsPipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        if (renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, renderPass, nullptr);


        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        swapChainImageViews.clear();
        
        if (swapChain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapChain, nullptr);

        // Recreate them
        createSwapChain();
        createImageViews();
        createRenderPass(); // Recreate render pass as it depends on swapChainImageFormat
        createGraphicsPipeline(); // Recreate pipeline as it depends on renderPass and swapChainExtent
        createFramebuffers();

        std::cout << "Swap chain recreated successfully." << std::endl;
    }


    void cleanup() {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (renderFinishedSemaphores.size() > i && renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            }
            if (imageAvailableSemaphores.size() > i && imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            }
            if (inFlightFences.size() > i && inFlightFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(device, inFlightFences[i], nullptr);
            }
        }
        // Clear vectors after destroying elements to avoid double frees if cleanup is called multiple times
        renderFinishedSemaphores.clear();
        imageAvailableSemaphores.clear();
        inFlightFences.clear();
        std::cout << "Synchronization objects destroyed." << std::endl;


        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
            std::cout << "Command Pool (and Command Buffers) destroyed." << std::endl;
        }

        for (auto framebuffer : swapChainFramebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
        }
        swapChainFramebuffers.clear();
        if (graphicsPipeline != VK_NULL_HANDLE || renderPass != VK_NULL_HANDLE) { // Check if they were created
             std::cout << "Swap chain framebuffers destroyed." << std::endl;
        }
        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
            std::cout << "Graphics Pipeline destroyed." << std::endl;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
            std::cout << "Pipeline Layout destroyed." << std::endl;
        }
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
            std::cout << "Vulkan Render Pass destroyed." << std::endl;
        }
        for (auto imageView : swapChainImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(device, imageView, nullptr);
            }
        }
        swapChainImageViews.clear();
        if (!swapChainImages.empty() || swapChain != VK_NULL_HANDLE) { 
             std::cout << "Swap chain image views destroyed." << std::endl;
        }
        if (swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapChain, nullptr);
            swapChain = VK_NULL_HANDLE;
            std::cout << "Vulkan Swap Chain destroyed." << std::endl;
        }
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr); 
            device = VK_NULL_HANDLE;
            std::cout << "Logical device destroyed." << std::endl;
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
            std::cout << "Vulkan surface destroyed." << std::endl;
        }
        if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
            std::cout << "Vulkan Debug Messenger destroyed." << std::endl;
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
            std::cout << "Vulkan Instance destroyed." << std::endl;
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
            window = nullptr;
            std::cout << "SDL Window destroyed." << std::endl;
        }
        SDL_Quit();
        std::cout << "SDL quit." << std::endl;
    }
};

int main(int argc, char* argv[]) {
    EngineApp app;
    try {
        std::cout << "Starting IWEngine..." << std::endl;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Unhandled Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Shutting down IWEngine." << std::endl;
    return EXIT_SUCCESS;
}

