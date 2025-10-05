
# Simple Demo App
set(EXECUTABLE_NAME simple_demo)
add_executable(${EXECUTABLE_NAME} main.c)

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/inc)
target_link_libraries(${EXECUTABLE_NAME} cglm glfw vulkan)

# First triangle app
set(EXECUTABLE_NAME triangle_demo)
add_executable(${EXECUTABLE_NAME} simple_vulkan_app.c)

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/inc)
target_link_libraries(${EXECUTABLE_NAME} cglm glfw vulkan m)

target_compile_definitions(${EXECUTABLE_NAME} PUBLIC SHADERS_FOLDER_PATH="${CMAKE_SOURCE_DIR}/shaders/")
