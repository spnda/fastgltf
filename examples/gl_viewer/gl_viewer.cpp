#include <chrono>
#include <fstream>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fastgltf_parser.hpp>
#include <fastgltf_types.hpp>

constexpr std::string_view vertexShaderSource = R"(
    #version 460 core

    layout(location = 0) in vec3 position;

    uniform mat4 modelMatrix;
    uniform mat4 viewProjectionMatrix;

    out vec3 colour;

    void main() {
        gl_Position = viewProjectionMatrix * modelMatrix * vec4(position, 1.0);
        colour = vec3(0.5, 0.5, 0.5);
    }
)";

constexpr std::string_view fragmentShaderSource = R"(
    #version 460 core

    in vec3 colour;
    out vec4 finalColor;
    void main() {
        finalColor = vec4(colour, 1.0);
    }
)";

void glMessageCallback(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam) {
    std::cout << message << std::endl;
}

bool checkGlCompileErrors(GLuint shader) {
    GLint success;
    constexpr int length = 1024;
    char log[length];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, length, nullptr, log);
        std::cout << "Shader compilation error: " << "\n"
                  << log << "\n -- --------------------------------------------------- -- " << std::endl;
        return false;
    }
    return true;
}

bool checkGlLinkErrors(GLuint target) {
    GLint success;
    constexpr int length = 1024;
    char log[length];
    glGetProgramiv(target, GL_LINK_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(target, length, nullptr, log);
        std::cout << "Shader program linking error: " << "\n"
                  << log << "\n -- --------------------------------------------------- -- " << std::endl;
        return false;
    }
    return true;
}

struct IndirectDrawCommand {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t baseVertex;
    uint32_t baseInstance;
};

struct Primitive {
    IndirectDrawCommand draw;
    GLenum primitiveType;
    GLenum indexType;
    GLuint vertexArray;

    // GL names we already constructed previously.
    GLuint vertexBuffer;
    GLuint indexBuffer;
};

struct Mesh {
    GLuint drawsBuffer;
    std::vector<Primitive> primitives;
};

struct Viewer {
    std::unique_ptr<fastgltf::Asset> asset;

    std::vector<GLuint> buffers;
    std::vector<Mesh> meshes;

    glm::mat4 viewMatrix = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::mat4(1.0f);
    GLint viewProjectionMatrixUniform = GL_NONE;
    GLint modelMatrixUniform = GL_NONE;

    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    glm::vec3 accelerationVector = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 position = glm::vec3(3.0f, 3.0f, 3.0f);

    glm::dvec2 lastCursorPosition = glm::dvec2(0.0f);
    glm::vec3 direction = glm::vec3(0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool firstMouse = true;
};

void updateCameraMatrix(Viewer* viewer) {
    glm::mat4 viewProjection = viewer->projectionMatrix * viewer->viewMatrix;
    glUniformMatrix4fv(viewer->viewProjectionMatrixUniform, 1, GL_FALSE, &viewProjection[0][0]);
}

void windowSizeCallback(GLFWwindow* window, int width, int height) {
    void* ptr = glfwGetWindowUserPointer(window);
    auto* viewer = static_cast<Viewer*>(ptr);

    viewer->projectionMatrix = glm::perspective(glm::radians(75.0f),
                                                static_cast<float>(width) / static_cast<float>(height),
                                                0.1f, 1000.0f);

    glViewport(0, 0, width, height);
}

void cursorCallback(GLFWwindow* window, double xpos, double ypos) {
    void* ptr = glfwGetWindowUserPointer(window);
    auto* viewer = static_cast<Viewer*>(ptr);

    if (viewer->firstMouse) {
        viewer->lastCursorPosition = { xpos, ypos };
        viewer->firstMouse = false;
    }

    auto offset = glm::vec2(xpos - viewer->lastCursorPosition.x, viewer->lastCursorPosition.y - ypos);
    viewer->lastCursorPosition = { xpos, ypos };
    offset *= 0.1f;

    viewer->yaw   += offset.x;
    viewer->pitch += offset.y;
    viewer->pitch = glm::clamp(viewer->pitch, -89.0f, 89.0f);

    auto& direction = viewer->direction;
    direction.x = cos(glm::radians(viewer->yaw)) * cos(glm::radians(viewer->pitch));
    direction.y = sin(glm::radians(viewer->pitch));
    direction.z = sin(glm::radians(viewer->yaw)) * cos(glm::radians(viewer->pitch));
    direction = glm::normalize(direction);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    void* ptr = glfwGetWindowUserPointer(window);
    auto* viewer = static_cast<Viewer*>(ptr);
    constexpr glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    auto& acceleration = viewer->accelerationVector;
    if (key == GLFW_KEY_W) {
        acceleration = viewer->direction;
    }

    if (key == GLFW_KEY_S) {
        acceleration -= viewer->direction;
    }

    if (key == GLFW_KEY_D) {
        acceleration += glm::normalize(glm::cross(viewer->direction, cameraUp));
    }

    if (key == GLFW_KEY_A) {
        acceleration -= glm::normalize(glm::cross(viewer->direction, cameraUp));
    }
}

glm::mat4 getTransformMatrix(const fastgltf::Node& node, glm::mat4x4& base) {
    /** Both a matrix and TRS values are not allowed
     * to exist at the same time according to the spec */
    if (node.hasMatrix) {
        return base * glm::mat4x4(glm::make_mat4x4(node.matrix.data()));
    } else {
        // TODO: Support rotation
        return base
            * glm::translate(glm::mat4(1.0f), glm::make_vec3(node.translation.data()))
            * glm::scale(glm::mat4(1.0f), glm::make_vec3(node.scale.data()));
    }
}

bool loadGltf(Viewer* viewer, std::string_view cPath) {
    std::cout << "Loading " << cPath << std::endl;

    // Parse the glTF file and get the constructed asset
    {
        fastgltf::Parser parser;

        auto path = std::filesystem::path{cPath};
        std::unique_ptr<fastgltf::glTF> gltf;

        if (path.extension() == ".gltf") {
            auto data = fastgltf::JsonData(path);
            gltf = parser.loadGLTF(&data, path.parent_path(), fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble);
        } else if (path.extension() == ".glb") {
            gltf = parser.loadBinaryGLTF(path, fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers);
        }

        if (parser.getError() != fastgltf::Error::None) {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(parser.getError()) << std::endl;
            return false;
        }

        gltf->parseScenes();
        gltf->parseNodes();
        gltf->parseMeshes();
        gltf->parseAccessors();
        gltf->parseBufferViews();
        auto error = gltf->parseBuffers();
        if (error != fastgltf::Error::None) {
            std::cerr << "Failed to parse glTF: " << fastgltf::to_underlying(error) << std::endl;
        }

        viewer->asset = gltf->getParsedAsset();
    }

    auto& buffers = viewer->asset->buffers;
    auto bufferCount = buffers.size();
    viewer->buffers.resize(bufferCount);
    glCreateBuffers(static_cast<GLsizei>(bufferCount), viewer->buffers.data());

    for (auto it = buffers.begin(); it != buffers.end(); ++it) {
        auto index = std::distance(buffers.begin(), it);
        constexpr GLuint bufferUsage = GL_STATIC_DRAW;

        switch (it->location) {
            case fastgltf::DataLocation::FilePathWithByteRange: {
                std::ifstream file(it->data.path, std::ios::ate | std::ios::binary);
                auto length = static_cast<int64_t>(file.tellg());
                auto minLength = (static_cast<int64_t>(it->byteLength) < length)
                    ? static_cast<int64_t>(it->byteLength)
                    : length;

                char* data = new char[minLength];
                file.seekg(0, std::istream::beg);
                file.read(data, minLength);
                glNamedBufferData(viewer->buffers[index], minLength, data, bufferUsage);
                delete[] data;
                break;
            }
            case fastgltf::DataLocation::VectorWithMime: {
                glNamedBufferData(viewer->buffers[index], static_cast<int64_t>(it->byteLength),
                                  it->data.bytes.data(), bufferUsage);
                break;
            }
            // TODO
            case fastgltf::DataLocation::None:
            case fastgltf::DataLocation::BufferViewWithMime:
                break;
        }
    }

    return true;
}

bool loadMesh(Viewer* viewer, fastgltf::Mesh& mesh) {
    auto& asset = viewer->asset;
    Mesh outMesh = {};
    outMesh.primitives.resize(mesh.primitives.size());
    for (auto it = mesh.primitives.begin(); it != mesh.primitives.end(); ++it) {
        if (it->attributes.find("POSITION") == it->attributes.end())
            continue;

        // We only support indexed geometry.
        if (!it->indicesAccessor.has_value()) {
            return false;
        }

        auto& positionAccessor = asset->accessors[it->attributes["POSITION"]];
        if (!positionAccessor.bufferViewIndex.has_value())
            continue;

        auto& positionView = asset->bufferViews[positionAccessor.bufferViewIndex.value()];

        // Get the output primitive
        auto index = std::distance(mesh.primitives.begin(), it);
        auto& primitive = outMesh.primitives[index];
        primitive.primitiveType = fastgltf::to_underlying(it->type);

        // Generate the VAO
        GLuint vao = GL_NONE;
        glCreateVertexArrays(1, &vao);
        primitive.vertexArray = vao;
        primitive.vertexBuffer = viewer->buffers[positionView.bufferIndex];
        primitive.indexBuffer = viewer->buffers[positionView.bufferIndex];

        glEnableVertexArrayAttrib(vao, 0);
        glVertexArrayAttribFormat(vao, 0,
                                  static_cast<GLint>(fastgltf::getNumComponents(positionAccessor.type)),
                                  fastgltf::getGLComponentType(positionAccessor.componentType),
                                  GL_FALSE, 0);
        glVertexArrayAttribBinding(vao, 0, 0);

        if (positionView.byteStride.has_value()) {
            glVertexArrayVertexBuffer(vao, 0, primitive.vertexBuffer,
                                      static_cast<GLintptr>(positionView.byteOffset + positionAccessor.byteOffset),
                                      static_cast<GLsizei>(positionView.byteStride.value()));
        } else {
            auto offset = positionView.byteOffset + positionAccessor.byteOffset;
            glVertexArrayVertexBuffer(vao, 0, primitive.vertexBuffer,
                                      static_cast<GLintptr>(offset),
                                      static_cast<GLsizei>(fastgltf::getElementByteSize(positionAccessor.type, positionAccessor.componentType)));
        }
        glVertexArrayElementBuffer(vao, primitive.indexBuffer);

        // Generate the indirect draw command
        auto& draw = primitive.draw;
        draw.instanceCount = 1;
        draw.baseInstance = 0;
        draw.baseVertex = 0;

        auto& indices = asset->accessors[it->indicesAccessor.value()];
        if (!indices.bufferViewIndex.has_value())
            return false;
        draw.count = indices.count;

        auto& indicesView = asset->bufferViews[indices.bufferViewIndex.value()];
        draw.firstIndex = (indices.byteOffset + indicesView.byteOffset) / fastgltf::getElementByteSize(indices.type, indices.componentType);
        primitive.indexType = getGLComponentType(indices.componentType);
    }

    // Create the buffer holding all of our primitive structs.
    glCreateBuffers(1, &outMesh.drawsBuffer);
    glNamedBufferData(outMesh.drawsBuffer, static_cast<GLsizeiptr>(outMesh.primitives.size() * sizeof(Primitive)),
                      outMesh.primitives.data(), GL_STATIC_DRAW);

    viewer->meshes.emplace_back(outMesh);

    return true;
}

void drawMesh(Viewer* viewer, size_t meshIndex, glm::mat4 matrix) {
    auto& mesh = viewer->meshes[meshIndex];

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mesh.drawsBuffer);

    glUniformMatrix4fv(viewer->modelMatrixUniform, 1, GL_FALSE, &matrix[0][0]);

    for (auto i = 0U; i < mesh.primitives.size(); ++i) {
        auto& prim = mesh.primitives[i];

        glBindVertexArray(prim.vertexArray);

        glDrawElementsIndirect(prim.primitiveType, prim.indexType,
                               reinterpret_cast<const void*>(i * sizeof(Primitive)));
    }
}

void drawNode(Viewer* viewer, size_t nodeIndex, glm::mat4 matrix) {
    auto& node = viewer->asset->nodes[nodeIndex];
    matrix = getTransformMatrix(node, matrix);

    if (node.meshIndex.has_value()) {
        drawMesh(viewer, node.meshIndex.value(), matrix);
    }

    for (auto& child : node.children) {
        drawNode(viewer, child, matrix);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "No gltf file specified." << std::endl;
        return -1;
    }
    auto gltfFile = std::string_view { argv[1] };
    Viewer viewer;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize glfw." << std::endl;
        return -1;
    }

    auto* mainMonitor = glfwGetPrimaryMonitor();
    auto* vidMode = glfwGetVideoMode(mainMonitor);

    GLFWwindow* window = glfwCreateWindow(static_cast<int>(static_cast<float>(vidMode->width) * 0.9f), static_cast<int>(static_cast<float>(vidMode->height) * 0.9f), "gl_viewer", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create window" << std::endl;
        return -1;
    }
    glfwSetWindowUserPointer(window, &viewer);
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetWindowSizeCallback(window, windowSizeCallback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL context." << std::endl;
        return -1;
    }

    if (GLAD_GL_VERSION_4_6 != 1) {
        std::cerr << "Missing support for GL 4.6" << std::endl;
        return -1;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(glMessageCallback, nullptr);

    // Compile the shaders
    GLuint program = GL_NONE;
    {
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

        auto* frag = fragmentShaderSource.data();
        auto* vert = vertexShaderSource.data();
        auto fragSize = static_cast<GLint>(fragmentShaderSource.size());
        auto vertSize = static_cast<GLint>(vertexShaderSource.size());

        glShaderSource(fragmentShader, 1, &frag, &fragSize);
        glShaderSource(vertexShader, 1, &vert, &vertSize);
        glCompileShader(fragmentShader);
        glCompileShader(vertexShader);
        if (!checkGlCompileErrors(fragmentShader))
            return -1;
        if (!checkGlCompileErrors(vertexShader))
            return -1;

        program = glCreateProgram();
        glAttachShader(program, fragmentShader);
        glAttachShader(program, vertexShader);
        glLinkProgram(program);
        if (!checkGlLinkErrors(program))
            return -1;

        glDeleteShader(fragmentShader);
        glDeleteShader(vertexShader);
    }

    // Load the glTF file
    auto start = std::chrono::high_resolution_clock::now();
    if (!loadGltf(&viewer, gltfFile)) {
        std::cerr << "Failed to parse glTF" << std::endl;
        return -1;
    }

    auto& asset = viewer.asset;
    for (auto& mesh : asset->meshes) {
        loadMesh(&viewer, mesh);
    }
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
    std::cout << "Loaded glTF file in " << diff.count() << "ms." << std::endl;

    viewer.modelMatrixUniform = glGetUniformLocation(program, "modelMatrix");
    viewer.viewProjectionMatrixUniform = glGetUniformLocation(program, "viewProjectionMatrix");
    glUseProgram(program);

    {
        // We just emulate the initial sizing of the window with a manual call.
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        windowSizeCallback(window, width, height);
    }

    viewer.lastFrame = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window)) {
        auto currentFrame = static_cast<float>(glfwGetTime());
        viewer.deltaTime = currentFrame - viewer.lastFrame;
        viewer.lastFrame = currentFrame;

        // Reset the acceleration
        viewer.accelerationVector = glm::vec3(0.0f);

        // Updates the acceleration vector and direction vectors.
        glfwPollEvents();

        // Factor the deltaTime into the amount of acceleration
        viewer.velocity += (viewer.accelerationVector * 50.0f) * viewer.deltaTime;
        // Lerp the velocity to 0, adding deceleration.
        viewer.velocity = viewer.velocity + (2.0f * viewer.deltaTime) * (glm::vec3(0.0f) - viewer.velocity);
        // Add the velocity into the position
        viewer.position += viewer.velocity * viewer.deltaTime;
        viewer.viewMatrix = glm::lookAt(viewer.position, viewer.position + viewer.direction, glm::vec3(0.0f, 1.0f, 0.0f));
        updateCameraMatrix(&viewer);

        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        auto& scene = viewer.asset->scenes[0];
        for (auto& node : scene.nodeIndices) {
            drawNode(&viewer, node, glm::mat4(1.0f));
        }

        glfwSwapBuffers(window);
    }

    for (auto& mesh : viewer.meshes) {
        glDeleteBuffers(1, &mesh.drawsBuffer);

        for (auto& prim : mesh.primitives) {
            glDeleteVertexArrays(1, &prim.vertexArray);
        }
    }

    glDeleteProgram(program);
    glDeleteBuffers(static_cast<GLint>(viewer.buffers.size()), viewer.buffers.data());

    glfwDestroyWindow(window);
    glfwTerminate();
}
