#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 300

#ifdef DEBUG
#define ENABLE_VALIDATION_LAYERS true
#define NB_VALIDATION_LAYERS 1
const char* VALIDATION_LAYERS[NB_VALIDATION_LAYERS] = {"VK_LAYER_KHRONOS_validation"};
#else
#define ENABLE_VALIDATION_LAYERS false
#endif

#define NB_REQUIRED_DEVICE_EXTENSIONS 1
const char* REQUIRED_DEVICE_EXTENSIONS[NB_REQUIRED_DEVICE_EXTENSIONS] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#define QUEUE_FAMILY_COUNT 2
struct QueueFamilyIndices {
    uint32_t graphics_family;
    bool graphics_family_found;

    uint32_t present_family;
    bool present_family_found;
} typedef QueueFamilyIndices;

void build_indices_set(QueueFamilyIndices indices, uint32_t* set_size,
                       uint32_t output[QUEUE_FAMILY_COUNT]) {
    *set_size = 0;
    uint32_t all_indices[QUEUE_FAMILY_COUNT] = {indices.graphics_family, indices.present_family};
    uint32_t indice_found[QUEUE_FAMILY_COUNT] = {indices.graphics_family_found,
                                                 indices.present_family_found};
    for(size_t i = 0; i < QUEUE_FAMILY_COUNT; i++) {
        bool not_in_set = true;
        if(indice_found[i]) {
            for(size_t j = 0; j < *set_size; j++) {
                if(output[j] == all_indices[i]) {
                    not_in_set = false;
                    break;
                }
            }
            if(not_in_set) {
                output[*set_size] = all_indices[i];
                (*set_size)++;
            }
        }
    }
}

bool is_queue_family_complete(QueueFamilyIndices q) {
    return q.graphics_family_found && q.present_family_found;
}

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;

    uint32_t format_count;
    VkSurfaceFormatKHR* formats;

    uint32_t present_mode_count;
    VkPresentModeKHR* present_modes;
} typedef SwapchainSupportDetails;

struct SimpleVkApp {
    GLFWwindow* window;
    VkInstance instance;
    bool validation_layers_available;

    VkDebugUtilsMessengerEXT debug_messenger;

    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSurfaceKHR surface;

    /* swapchain stuff */
    VkSwapchainKHR swapchain;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
    uint32_t image_count;
    VkImage* swapchain_images;
    VkImageView* swapchain_images_views;

} typedef SimpleVkApp;

QueueFamilyIndices find_queue_families(SimpleVkApp* app, VkPhysicalDevice device) {
    QueueFamilyIndices indices = {0};

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    VkBool32 present_support;
    for(size_t i = 0; i < queue_family_count && !is_queue_family_complete(indices); i++) {
        present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, app->surface, &present_support);
        if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.graphics_family_found = true;
        }
        if(present_support) {
            indices.present_family = i;
            indices.present_family_found = true;
        }
    }
    return indices;
}

// WARNING: allocates memory when creating struct
SwapchainSupportDetails query_swapchain_support(SimpleVkApp* app, VkPhysicalDevice device) {
    SwapchainSupportDetails details = {0};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, app->surface, &(details.capabilities));

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->surface, &(details.format_count), NULL);
    if(details.format_count != 0) {
        details.formats = calloc(details.format_count, sizeof(VkSurfaceFormatKHR));
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->surface, &(details.format_count),
                                         details.formats);

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->surface, &(details.present_mode_count),
                                              NULL);
    if(details.present_mode_count != 0) {
        details.present_modes = calloc(details.present_mode_count, sizeof(VkPresentModeKHR));
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->surface, &(details.present_mode_count),
                                              details.present_modes);

    return details;
}

VkSurfaceFormatKHR choose_swap_surface_format(uint32_t available_format_count,
                                              const VkSurfaceFormatKHR* available_formats) {
    for(uint32_t i = 0; i < available_format_count; i++) {
        if(available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
           available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_formats[i];
        }
    }

    return available_formats[0];
}

VkPresentModeKHR choose_swap_present_mode(uint32_t available_present_mode_count,
                                          VkPresentModeKHR* available_present_modes) {
    for(uint32_t i = 0; i < available_present_mode_count; i++) {
        if(available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

// Resolution of the swap chain image
VkExtent2D choose_swap_extent(SimpleVkApp* app, VkSurfaceCapabilitiesKHR* capabilities) {
    if(capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(app->window, &width, &height);

        VkExtent2D actual_extent = {(uint32_t)width, (uint32_t)height};
        actual_extent.width = actual_extent.width < capabilities->maxImageExtent.width
                                  ? actual_extent.width
                                  : capabilities->maxImageExtent.width;
        actual_extent.width = actual_extent.width > capabilities->minImageExtent.width
                                  ? actual_extent.width
                                  : capabilities->minImageExtent.width;
        actual_extent.height = actual_extent.height < capabilities->maxImageExtent.height
                                   ? actual_extent.height
                                   : capabilities->maxImageExtent.height;
        actual_extent.height = actual_extent.height > capabilities->minImageExtent.height
                                   ? actual_extent.height
                                   : capabilities->minImageExtent.height;
        return actual_extent;
    }
}

/* WINDOW CREATION *************************************************/

void init_window(SimpleVkApp* app) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    app->window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "jubilant", NULL, NULL);
}

/* VkInstance CREATION *********************************************/

/* Debug messages ********************/
// PFN_vkDebugUtilsMessengerCallbackEXT
VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_types,
               const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
    // The enum for message severity allows comparaison, so I can check >= warning e.g
    /* Message type also has interesting information:
                1. general message
                2. message from validation -> violating the specs or possible mistake
                3. performance -> potential non-optimal use of vulkan
    */
    switch(message_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        /* Diagnostic */
        printf("\t[DIAG]");
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        /* Informational */
        printf("\t[INFO]");
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        /* Not an error but likely to be a bug */
        printf("\t[WARN]");
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        /* Invalid, may cause crash */
        printf("\t[ERRR]");
        break;
    default:
        break;
    }
    // Callback data has the message, but also information about the object (array) related to the
    // message, and the nb of objects in said array
    printf("Validation Layer: %s\n", callback_data->pMessage);
    // user data is unused here, but I can define stuff and pass it at every callback.
    return VK_FALSE;
}

void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT* create_info) {
    create_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info->pfnUserCallback = debug_callback;
    create_info->pUserData = NULL; // optionnal
}

void setup_debug_messenger(SimpleVkApp* app) {
    if(!ENABLE_VALIDATION_LAYERS || !(app->validation_layers_available))
        return;
    VkDebugUtilsMessengerCreateInfoEXT create_info = {0};
    populate_debug_messenger_create_info(&create_info);

    // this function is in an extension: we need to load it manually
    PFN_vkCreateDebugUtilsMessengerEXT function =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(app->instance,
                                                                  "vkCreateDebugUtilsMessengerEXT");
    if(function != NULL) {
        if(function(app->instance, &create_info, NULL, &(app->debug_messenger)) != VK_SUCCESS) {
            printf("failed to setup debug messager");
        }
    } else {
        printf("failed to setup debug messager: extension not present");
    }
}

#ifdef DEBUG
bool check_validation_layer_support() {

    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties layers[layer_count];
    vkEnumerateInstanceLayerProperties(&layer_count, layers);

    for(size_t i = 0; i < NB_VALIDATION_LAYERS; i++) {
        bool validation_layer_found = false;
        for(size_t j = 0; j < layer_count; j++) {
            if(strcmp(layers[j].layerName, VALIDATION_LAYERS[i]) == 0) {
                validation_layer_found = true;
                break;
            }
        }
        if(!validation_layer_found) {
            printf("validation layer %s not found\n", VALIDATION_LAYERS[i]);
            return false;
        }
    }
    return true;
}
#endif

/* Instance Creation *****************/

void get_required_extensions(uint32_t* nb_extensions, const char** required_extensions,
                             bool validation_layers_available) {
    uint32_t nb_glfw_extensions;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&nb_glfw_extensions);
    *nb_extensions = nb_glfw_extensions;
    for(size_t i = 0; i < nb_glfw_extensions; i++) {
        required_extensions[i] = glfw_extensions[i];
    }
    if(ENABLE_VALIDATION_LAYERS && validation_layers_available) {
        *nb_extensions += 1;
        required_extensions[nb_glfw_extensions] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }
}

void create_instance(SimpleVkApp* app) {
    // fill with information about application. This can help the driver
    // optimize our application, eg if it uses a certain well-know graphics
    // engine with special behaviour.
    if(ENABLE_VALIDATION_LAYERS) {
        if(!check_validation_layer_support()) {
            app->validation_layers_available = false;
            printf("validation layers requested but not found \n");
        } else {
            app->validation_layers_available = true;
        }
    }
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "jubilant triangle";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);
    app_info.pEngineName = "none";
    app_info.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    // Fetch required extensions: count and their names
    const char* enabled_extensions[255];
    get_required_extensions(&(create_info.enabledExtensionCount), enabled_extensions,
                            app->validation_layers_available);
    create_info.ppEnabledExtensionNames = enabled_extensions;

    // Request validation layers
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {0};
    if(ENABLE_VALIDATION_LAYERS && app->validation_layers_available) {
        create_info.enabledLayerCount = NB_VALIDATION_LAYERS;
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
        populate_debug_messenger_create_info(&debug_create_info);
        create_info.pNext = &debug_create_info;
    }

    VkResult result = vkCreateInstance(&create_info, NULL, &(app->instance));
    if(result != VK_SUCCESS) {
        printf("failed to create instance\n");
    }
};

/* Physical Device Choice ************/

bool check_device_extension_support(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    VkExtensionProperties available_extensions[extension_count];
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    bool required_extension_available[NB_REQUIRED_DEVICE_EXTENSIONS] = {0};
    for(uint32_t i = 0; i < extension_count; i++) {
        for(uint32_t j = 0; j < NB_REQUIRED_DEVICE_EXTENSIONS; j++) {
            required_extension_available[j] |=
                strcmp(available_extensions[i].extensionName, REQUIRED_DEVICE_EXTENSIONS[j]) == 0;
        }
    }
    bool all_required_present = true;
    for(uint32_t i = 0; i < NB_REQUIRED_DEVICE_EXTENSIONS; i++) {
        all_required_present &= required_extension_available[i];
    }
    return all_required_present;
}

bool is_device_suitable(SimpleVkApp* app, VkPhysicalDevice device) {
    // Properties: name, type, supported vulkan version...
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    // Texture compression, 64 bits floats, multi viewport rendering...
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    bool extension_supported;
    bool swapchain_adequate = false;

    extension_supported = check_device_extension_support(device);
    if(extension_supported) {
        SwapchainSupportDetails swapchain_support = query_swapchain_support(app, device);
        swapchain_adequate =
            (swapchain_support.format_count != 0) && (swapchain_support.present_mode_count != 0);
        if(swapchain_adequate) {
            free(swapchain_support.formats);
            free(swapchain_support.present_modes);
        }
    }

    if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
       device_features.geometryShader &&
       is_queue_family_complete(find_queue_families(app, device)) && extension_supported &&
       swapchain_adequate) {

        printf("device %s is suitable\n", device_properties.deviceName);
        return true;
    }
    return false;
}

void pick_physical_device(SimpleVkApp* app) {
    app->physical_device = VK_NULL_HANDLE;
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);
    if(device_count == 0) {
        printf("no GPU with vulkan support found\n");
    }
    VkPhysicalDevice devices[device_count];
    vkEnumeratePhysicalDevices(app->instance, &device_count, devices);
    for(size_t i = 0; i < device_count; i++) {
        if(is_device_suitable(app, devices[i])) {
            app->physical_device = devices[i];
            break;
        }
    }

    if(app->physical_device == VK_NULL_HANDLE) {
        printf("no suitable GPU found\n");
    }
}

void make_logical_device(SimpleVkApp* app) {
    QueueFamilyIndices indices = find_queue_families(app, app->physical_device);
    uint32_t unique_indices_count = 0;
    uint32_t indices_set[QUEUE_FAMILY_COUNT];
    build_indices_set(indices, &unique_indices_count, indices_set);

    VkDeviceQueueCreateInfo all_queues_create_infos[unique_indices_count];
    float queue_priority = 1.0f;
    for(size_t i = 0; i < unique_indices_count; i++) {
        VkDeviceQueueCreateInfo queue_create_info = {0};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        queue_create_info.queueFamilyIndex = indices_set[i];
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        all_queues_create_infos[i] = queue_create_info;
    }

    VkPhysicalDeviceFeatures device_features = {0};
    VkDeviceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = unique_indices_count;
    create_info.pQueueCreateInfos = all_queues_create_infos;
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = NB_REQUIRED_DEVICE_EXTENSIONS;
    create_info.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS;

    // instance and device specific validation layers used to be separate. This is no longer the
    // case, and I don't really care about older implementations compatibility, so the code is left
    // commented, but there for reference.
    // create_info.enabledExtensionCount = 0;
    // if(ENABLE_VALIDATION_LAYERS && app->validation_layers_available) {
    //     create_info.enabledLayerCount = NB_VALIDATION_LAYERS;
    //     create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    // }
    if(vkCreateDevice(app->physical_device, &create_info, NULL, &(app->device)) != VK_SUCCESS) {
        printf("failed to create logical device \n");
    }

    vkGetDeviceQueue(app->device, indices.graphics_family, 0, &(app->graphics_queue));
    vkGetDeviceQueue(app->device, indices.present_family, 0, &(app->present_queue));
}

/* Window surface creation ***********/

void create_surface(SimpleVkApp* app) {
    if(glfwCreateWindowSurface(app->instance, app->window, NULL, &(app->surface)) != VK_SUCCESS) {
        printf("failed to create window surface");
    }
}

/* Swap chain creation ***************/

void create_swapchain(SimpleVkApp* app) {
    SwapchainSupportDetails swapchain_support = query_swapchain_support(app, app->physical_device);

    VkSurfaceFormatKHR surface_format =
        choose_swap_surface_format(swapchain_support.format_count, swapchain_support.formats);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support.present_mode_count,
                                                             swapchain_support.present_modes);
    VkExtent2D extent = choose_swap_extent(app, &(swapchain_support.capabilities));

    free(swapchain_support.formats);
    free(swapchain_support.present_modes);

    // add an extra image if the driver has to handle some internal op or smtg
    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if(swapchain_support.capabilities.maxImageCount > 0 &&
       swapchain_support.capabilities.maxImageCount < image_count) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = app->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;                             // amount of layers per image
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // what are the layers

    QueueFamilyIndices indices = find_queue_families(app, app->physical_device);
    uint32_t queue_family_indices[QUEUE_FAMILY_COUNT] = {indices.graphics_family,
                                                         indices.present_family};
    if(indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode =
            VK_SHARING_MODE_EXCLUSIVE;          // best performance as ownership is explicit
        create_info.queueFamilyIndexCount = 0;  // Optionnal
        create_info.pQueueFamilyIndices = NULL; // Optionnal
    }

    // can specify specific transformations like 90 degrees rotation; see supportedTransforms in
    // capabilities
    create_info.preTransform = swapchain_support.capabilities.currentTransform;

    // if the alpha channel should be used to blend with other windows in the window system
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // opaque to ignore it

    create_info.presentMode = present_mode;
    // if the pixels are obscured (e.g. another window in front) ignore them. gives best performance
    create_info.clipped = VK_TRUE;

    // if the swap chain is changed, if e.g. window is resized, it might become unoptimized and
    // needs to be recreated from scratch and a ref to the old one must be specified.
    create_info.oldSwapchain = VK_NULL_HANDLE;
    if(vkCreateSwapchainKHR(app->device, &create_info, NULL, &(app->swapchain)) != VK_SUCCESS) {
        printf("failed to create swapchain\n");
    }

    vkGetSwapchainImagesKHR(app->device, app->swapchain, &(app->image_count), NULL);
    app->swapchain_images = calloc(app->image_count, sizeof(VkImage));
    vkGetSwapchainImagesKHR(app->device, app->swapchain, &(app->image_count),
                            app->swapchain_images);

    app->swapchain_image_format = surface_format.format;
    app->swapchain_extent = extent;
}

void create_image_views(SimpleVkApp* app) {
    app->swapchain_images_views = calloc(app->image_count, sizeof(VkImageView));

    for(size_t i = 0; i < app->image_count; i++) {
        VkImageViewCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = app->swapchain_images[i];
        create_info.viewType =
            VK_IMAGE_VIEW_TYPE_2D; // Could be 1/2/3D texture, arrays of textures, ...
        create_info.format = app->swapchain_image_format;
        // This can allow for example to create monochrome image, by passing eg
        // VK_COMPONENT_SWIZZLE_R, or one could also map them to constant 0/1 values
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;
        if(vkCreateImageView(app->device, &create_info, NULL, &(app->swapchain_images[i])) !=
           VK_SUCCESS) {
            printf("failed to create image view %d", i);
        }
    }
}

void init_vulkan(SimpleVkApp* app) {
    create_instance(app);
    setup_debug_messenger(app);

    create_surface(app);

    pick_physical_device(app);
    make_logical_device(app);

    create_swapchain(app);
    create_image_views(app);
}

void main_loop(SimpleVkApp* app) {}

void cleanup(SimpleVkApp* app) {
    // Cleanup Vulkan

    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
    free(app->swapchain_images);
    for(size_t i = 0; i < app->image_count; i++) {
        vkDestroyImageView(app->device, app->swapchain_images_views[i], NULL);
    }

    free(app->swapchain_images_views);
    vkDestroyDevice(app->device, NULL);

    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    if(ENABLE_VALIDATION_LAYERS && app->validation_layers_available) {
        PFN_vkDestroyDebugUtilsMessengerEXT function =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                app->instance, "vkDestroyDebugUtilsMessengerEXT");
        if(function != NULL) {
            function(app->instance, app->debug_messenger, NULL);
        }
    }
    vkDestroyInstance(app->instance, NULL);

    // Cleanup glfw
    glfwDestroyWindow(app->window);
    glfwTerminate();
}

int main(int argc, char const* argv[]) {
    SimpleVkApp* app = malloc(sizeof(SimpleVkApp));

    init_window(app);
    init_vulkan(app);

    while(!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
    }

    cleanup(app);
    free(app);

    return 0;
}
