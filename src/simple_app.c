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
const bool ENABLE_VALIDATION_LAYERS = true;
const uint32_t NB_VALIDATION_LAYERS = 1;
const char* VALIDATION_LAYERS[1] = {"VK_LAYER_KHRONOS_validation"};
#else
const bool ENABLE_VALIDATION_LAYERS = false;
#endif

struct QueueFamilyIndices {
    uint32_t graphics_family;
    bool graphics_family_found;
} typedef QueueFamilyIndices;

bool is_queue_family_complete(QueueFamilyIndices q) { return q.graphics_family_found; }

QueueFamilyIndices find_queue_families(VkPhysicalDevice device) {
    QueueFamilyIndices indices = {0};

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);
    for(size_t i = 0; i < queue_family_count && !is_queue_family_complete(indices); i++) {
        if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.graphics_family_found = true;
        }
    }
    return indices;
}

struct SimpleVkApp {
    GLFWwindow* window;
    VkInstance instance;
    bool validation_layers_available;

    VkDebugUtilsMessengerEXT debug_messenger;

    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphicsQueue;

} typedef SimpleVkApp;

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

bool is_device_suitable(VkPhysicalDevice device) {
    // Properties: name, type, supported vulkan version...
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    // Texture compression, 64 bits floats, multi viewport rendering...
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
       device_features.geometryShader && is_queue_family_complete(find_queue_families(device))) {
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
        if(is_device_suitable(devices[i])) {
            app->physical_device = devices[i];
            break;
        }
    }

    if(app->physical_device == VK_NULL_HANDLE) {
        printf("no suitable GPU found\n");
    }
}

void make_logical_device(SimpleVkApp* app) {
    QueueFamilyIndices indices = find_queue_families(app->physical_device);

    VkDeviceQueueCreateInfo queue_create_info = {0};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = indices.graphics_family;
    queue_create_info.queueCount = 1;
    float queue_priority = 1.0f;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features = {0};
    VkDeviceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = &queue_create_info;
    create_info.queueCreateInfoCount = 1;
    create_info.pEnabledFeatures = &device_features;

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

    vkGetDeviceQueue(app->device, indices.graphics_family, 0, &(app->graphicsQueue));
}

void init_vulkan(SimpleVkApp* app) {
    create_instance(app);
    setup_debug_messenger(app);
    pick_physical_device(app);
    make_logical_device(app);
}

void main_loop(SimpleVkApp* app) {}

void cleanup(SimpleVkApp* app) {
    // Cleanup Vulkan

    vkDestroyDevice(app->device, NULL);

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
