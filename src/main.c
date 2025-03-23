#include <stdio.h>

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cglm/cglm.h>

int main(){
    #ifdef CGLM_ALL_UNALIGNED
    printf("All unaligned, fix plz\n");
    #endif

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(400, 300, "jubilant", NULL, NULL);

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    printf("%d extensions supported\n", extensionCount);
    
    mat4 matrix = GLM_MAT4_IDENTITY_INIT;
    matrix[2][3] = 1;
    vec4 vec = {0, 1, 2, 3};
    glm_mat4_mulv(matrix, vec, vec);

    for (size_t i = 0; i < 4; i++){
        printf("%f ", vec[i]);
    }
    printf("\n");
    
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    return 0;
}