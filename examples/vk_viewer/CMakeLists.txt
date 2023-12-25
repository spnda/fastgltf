add_executable(fastgltf_vk_viewer EXCLUDE_FROM_ALL)
target_compile_features(fastgltf_vk_viewer PUBLIC cxx_std_17)
target_link_libraries(fastgltf_vk_viewer PRIVATE fastgltf glfw::glfw glm::glm stb)
target_link_libraries(fastgltf_vk_viewer PRIVATE Vulkan::Headers volk::volk_headers vk-bootstrap::vk-bootstrap Vulkan::MemoryAllocator enkiTS::enkiTS)

fastgltf_add_source_directory(TARGET fastgltf_vk_viewer FOLDER ".")
