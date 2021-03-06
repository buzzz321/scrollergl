// clang-format off
#include "khrplatform.h"
#include "glad.h" // must be before glfw.h
#include <GLFW/glfw3.h>
// clang-format on
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <optional>
#include <random>
#include <vector>

constexpr int32_t SCREEN_WIDTH = 1600;
constexpr int32_t SCREEN_HEIGHT = 1100;
constexpr int32_t FONT_WIDTH = 54;
constexpr int32_t FONT_HEIGHT = 71;
constexpr int32_t MAX_FONTS_PER_LINE = 48 - 14;

constexpr float fov = glm::radians(90.0f);
const float zFar = (SCREEN_WIDTH / 2.0) / tanf64(fov / 2.0f);
constexpr auto vertexShaderSource = R"(
#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in mat4 aOffset;


out vec4 mycolour;
out vec2 mytexCoord;
flat out int fontNo; 

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec2 u_resolution;
uniform int u_font[256];

void main()
{
    gl_Position = projection * view * model * aOffset * vec4(aPos, 1.0);

    vec3 ndc = gl_Position.xyz / gl_Position.w;
    mycolour = vec4(1.0,1.0,1.0,0.0);
    mytexCoord = vec2(aTexCoord.x, aTexCoord.y);
    fontNo = u_font[gl_InstanceID%256];
}
)";

constexpr auto fragmentShaderSource = R"(
#version 430 core

out vec4 FragColor;
in vec4 mycolour;
in vec2 mytexCoord;
flat in int fontNo; 

uniform sampler2DArray ourTexture;

void main()
{
    vec4 myoutput = texture(ourTexture, vec3(mytexCoord.st,fontNo));
    if (myoutput.a < 0.2)//dont paint anything if we are almost transparent
       discard;
    FragColor = myoutput;
} )";

const std::string scroll_text =
  R"(just some random babbling to show on screen since we dont have dots or commas typing and reading will be a challenge 
)";

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id,
                            GLenum severity, [[maybe_unused]] GLsizei length,
                            const char *message,
                            [[maybe_unused]] const void *userParam);
struct Font {
  int width{0};
  int height{0};
  unsigned int texture{0};
};

Font loadFont(std::string filename) {
  // Font retVal;
  stbi_set_flip_vertically_on_load(true);
  // we need to flip image since opengl has 0,0 in lower left bottom as
  // all proper coordinate systems
  int width{0}, height{0}, nrChannels{0}, layerCount{27};
  int tiles{27}; // 27 chars in font
  auto font =
    stbi_load(filename.c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);
  unsigned int texture;

  if (font) {
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //-----------------------------------------------------------------------
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, FONT_WIDTH, FONT_HEIGHT,
                   layerCount);

    // tell opengl size of one row in whole image
    glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
    int params[] = {0, 0, 0, 0, 0, 0, 0};
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_IMMUTABLE_FORMAT,
                        params);

    std::cout << "param = " << std::hex << params[0] << std::endl;

    int mipLevel{0}, xSkipPixelsPerRow{0}, yOffset{0};
    int fontNumber{0};
    for (fontNumber = 0; fontNumber < layerCount; fontNumber++) {
      xSkipPixelsPerRow = fontNumber % layerCount * FONT_WIDTH;
      yOffset = fontNumber / 4 * FONT_HEIGHT; //[[maybe_unused]]
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, xSkipPixelsPerRow);
      // glPixelStorei(GL_UNPACK_SKIP_ROWS, yOffset);

      glTexSubImage3D(GL_TEXTURE_2D_ARRAY, mipLevel, 0, 0, fontNumber,
                      FONT_WIDTH, FONT_HEIGHT, 1 /*layerCount*/, GL_RGBA,
                      GL_UNSIGNED_BYTE, font);
    }
    stbi_image_free(font);
  }

  return {width, height, texture};
}

unsigned int loadShaders(const char *shaderSource, GLenum shaderType) {
  unsigned int shader{0};
  int success{0};
  char infoLog[1024];

  shader = glCreateShader(shaderType); // GL_VERTEX_SHADER

  glShaderSource(shader, 1, &shaderSource, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetShaderInfoLog(shader, 1024, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
              << infoLog << std::endl;
  }
  return shader;
}

unsigned int makeShaderProgram(uint32_t vertexShader, uint32_t fragmentShader) {
  unsigned int shaderProgram;
  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return shaderProgram;
}

void error_callback(int error, const char *description) {
  std::cerr << "Error: " << description << " error number " << error
            << std::endl;
}

void key_callback(GLFWwindow *window, int key, int /*scancode*/, int action,
                  int /*mods*/) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void framebuffer_size_callback([[maybe_unused]] GLFWwindow *window, int width,
                               int height) {
  glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}

void camera(uint32_t shaderId, [[maybe_unused]] float dist) {
  glm::mat4 view = glm::mat4(1.0f);

  // float zFar = (SCREEN_WIDTH / 2.0f) / tanf64(fov / 2.0f); // was 90.0f
  glm::vec3 cameraPos =
    glm::vec3(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, zFar);
  glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
  view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

  int modelView = glGetUniformLocation(shaderId, "view");
  glUniformMatrix4fv(modelView, 1, GL_FALSE, glm::value_ptr(view));
}

std::vector<glm::vec3> generateFontOffsets(uint32_t amount) {
  std::vector<glm::vec3> retVal;
  //   {0, 300, 230}, {1600, 300, 230}
  for (float index = 0; index < (float)amount; ++index) {
    glm::vec3 vertex((float)1600.0f + FONT_WIDTH, (float)300.0f, (float)230.0f);

    retVal.push_back(vertex);
  }
  return retVal;
}

std::optional<uint32_t> getFontNumber(const std::string &text, size_t pos) {
  if (text[pos] != '\n') {
    return scroll_text[pos] == ' ' ? 27 : scroll_text[pos] - 'a';
  }
  return std::nullopt;
}
int main() {
  std::random_device r;
  std::default_random_engine e1(r());
  std::vector<uint32_t> fontIndex;
  // clang-format off
  std::vector<float> quad = {// first position (3) then texture coords (2)
      -0.50f, -0.50f, 0.0f, 0.0f, 0.0f, // bottom left
       0.50f, -0.50f, 0.0f, 1.0f, 0.0f, // bottom right
       0.50f,  0.50f, 0.0f, 1.0f, 1.0f, // top right
      -0.50f, -0.50f, 0.0f, 0.0f, 0.0f, // bottom left
       0.50f,  0.50f, 0.0f, 1.0f, 1.0f, // top right
      -0.50f,  0.50f, 0.0f, 0.0f, 1.0f, //top left
  };
  // clang-format on

  for (size_t index = 0; index < scroll_text.size(); index++) {
    // 27 is pos of space
    std::cout << scroll_text[index];

    auto fontNo = getFontNumber(scroll_text, index);
    if (fontNo.has_value())
      fontIndex.push_back(fontNo.value());
  }
  std::cout << std::endl;
  std::vector<glm::vec3> fontOffsets = generateFontOffsets(MAX_FONTS_PER_LINE);
  //{{0, 300, 230}, {1600, 300, 230}}; // generateStarOffsets(100);
  std::vector<glm::mat4> offsetMatrices(fontOffsets.size());

  [[maybe_unused]] float deltaTime =
    0.0f;                 // Time between current frame and last frame
  float lastFrame = 0.0f; // Time of last frame

  if (!glfwInit()) {
    // Initialization failed
    std::cerr << "Error could not init glfw!" << std::endl;
    exit(1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
  glfwSetErrorCallback(error_callback);

  GLFWwindow *window =
    glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "My Title", nullptr, nullptr);
  if (!window) {
    std::cerr << "Error could not create window" << std::endl;
    exit(1);
    // Window or OpenGL context creation failed
  }

  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, key_callback);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << " Error could not load glad " << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(1);
  }
  glfwSwapInterval(1);

  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  //  Enable depth test
  glEnable(GL_DEPTH_TEST);
  // Accept fragment if it closer to the camera than the former one
  glDepthFunc(GL_LESS);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

  int flags;
  glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
  if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
    std::cout << "debug mode enabled!" << std::endl;
    glDebugMessageCallback(glDebugOutput, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr,
                          GL_TRUE);
  }

  unsigned int VAO;
  glGenVertexArrays(1, &VAO);

  unsigned int VBO;
  glGenBuffers(1, &VBO);

  // verticies
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, quad.size() * sizeof(float), &quad[0],
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  // end verticies

  // instance data

  unsigned int instanceVBO;
  glGenBuffers(1, &instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, offsetMatrices.size() * sizeof(glm::mat4),
               nullptr /*offsetMatrices.data()*/, GL_DYNAMIC_DRAW);
  // here we have to do this 4 times since vec 4 is max per attrib pointer
  //  and our matrix is 4x4

  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 1 * sizeof(glm::mat4),
                        (void *)0);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 1 * sizeof(glm::mat4),
                        (void *)(1 * sizeof(glm::vec4)));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 1 * sizeof(glm::mat4),
                        (void *)(2 * sizeof(glm::vec4)));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 1 * sizeof(glm::mat4),
                        (void *)(3 * sizeof(glm::vec4)));
  glEnableVertexAttribArray(5);

  glVertexAttribDivisor(2, 1);
  glVertexAttribDivisor(3, 1);
  glVertexAttribDivisor(4, 1);
  glVertexAttribDivisor(5, 1);

  auto font = loadFont("../font.png");

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
          GL_STENCIL_BUFFER_BIT); // also clear the depth buffer now!  |
                                  // GL_DEPTH_BUFFER_BIT

  // end instance data

  auto vertexShader = loadShaders(vertexShaderSource, GL_VERTEX_SHADER);
  auto fragmentShader = loadShaders(fragmentShaderSource, GL_FRAGMENT_SHADER);
  auto shaderProgram = makeShaderProgram(vertexShader, fragmentShader);

  // float zFar = (SCREEN_WIDTH / 2.0) / tanf64(fov / 2.0f) + 10.0f; // 100.0f
  glm::mat4 projection = glm::perspective(
    fov, (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, zFar + 10.0f);

  std::uniform_int_distribution<int> zrand(-zFar, 100.0);

  float dist = 0;
  std::cout << "zFar=" << zFar + 10.0f << std::endl;

  size_t startup_counter{0};
  size_t fonts_in_flight{0};
  size_t next_free_font_index{0};

  while (!glfwWindowShouldClose(window)) {
    int win_width, win_height;

    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT); // also clear the depth buffer now!  |
                                    // GL_DEPTH_BUFFER_BIT
    // 2. use our shader program when we want to render an object
    glUseProgram(shaderProgram);

    // bind texture and put in all font indexes ( eg scroll text ) into shader
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, font.texture);
    glUniform1i(glGetUniformLocation(shaderProgram, "ourTexture"), 0);

    int fontNo = glGetUniformLocation(shaderProgram, "u_font");
    glUniform1iv(fontNo, fontIndex.size(), (const GLint *)fontIndex.data());

    // push current window size into shader.
    glfwGetWindowSize(window, &win_width, &win_height);
    glm::vec2 u_res(win_width, win_height);
    int resolution = glGetUniformLocation(shaderProgram, "u_resolution");
    glUniformMatrix4fv(resolution, 1, GL_FALSE, glm::value_ptr(u_res));

    int modelprj = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(modelprj, 1, GL_FALSE, glm::value_ptr(projection));

    camera(shaderProgram, (float)dist);

    glBindVertexArray(VAO);

    // we dont move this model we just move the insances..
    glm::mat4 fontModel = glm::mat4(1.0f);
    fontModel = glm::translate(fontModel, glm::vec3(0, 0, dist));
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(fontModel));

    if (startup_counter < win_width) {
      if (0 == (startup_counter % (FONT_WIDTH - 20))) {
        if (fonts_in_flight < MAX_FONTS_PER_LINE) {
          fonts_in_flight++;
          next_free_font_index++;
          /*        std::cout << "startup_counter " << std::dec <<
          startup_counter
                    << std::endl;
          std::cout << "fonts in flight " << fonts_in_flight << std::endl;
          */
        }
      }
      startup_counter++;
    }

    size_t instance_index{0};
    for (auto &vec : fontOffsets) {
      if (instance_index < fonts_in_flight) {
        vec.x -= 1.5f;
      } else {
        vec.x = vec.x;
      }

      if (vec.x < -FONT_WIDTH) {
        vec.x = 1600.0 + FONT_WIDTH;
        auto new_char = getFontNumber(scroll_text, next_free_font_index);
        if (new_char.has_value())
          fontIndex.at(instance_index) = new_char.value();
        next_free_font_index++;
        if (next_free_font_index >= scroll_text.size()) {
          next_free_font_index = 0;
        }
      }

      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, glm::vec3(vec.x, vec.y, vec.z));
      model = glm::scale(model, glm::vec3(50.0, 50.0, 1.0));
      offsetMatrices[instance_index] = model;
      // std::cout << "index " << index << std::endl;
      instance_index++;

      // std::cout << glm::to_string(model) << std::endl;
    }
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, offsetMatrices.size() * sizeof(glm::mat4),
                 nullptr,
                 GL_DYNAMIC_DRAW); // realloc in place same buffer
                                   // with orphaning.. opengl magic.

    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    offsetMatrices.size() * sizeof(glm::mat4),
                    offsetMatrices.data());

    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, fontOffsets.size(),
                                      0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glfwSwapBuffers(window);
    //  Keep running
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id,
                            GLenum severity, [[maybe_unused]] GLsizei length,
                            const char *message,
                            [[maybe_unused]] const void *userParam) {
  // ignore non-significant error/warning codes
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
    return;

  std::cout << "---------------" << std::endl;
  std::cout << "Debug message (" << id << "): " << message << std::endl;

  switch (source) {
  case GL_DEBUG_SOURCE_API:
    std::cout << "Source: API";
    break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
    std::cout << "Source: Window System";
    break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER:
    std::cout << "Source: Shader Compiler";
    break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:
    std::cout << "Source: Third Party";
    break;
  case GL_DEBUG_SOURCE_APPLICATION:
    std::cout << "Source: Application";
    break;
  case GL_DEBUG_SOURCE_OTHER:
    std::cout << "Source: Other";
    break;
  }
  std::cout << std::endl;

  switch (type) {
  case GL_DEBUG_TYPE_ERROR:
    std::cout << "Type: Error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    std::cout << "Type: Deprecated Behaviour";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    std::cout << "Type: Undefined Behaviour";
    break;
  case GL_DEBUG_TYPE_PORTABILITY:
    std::cout << "Type: Portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE:
    std::cout << "Type: Performance";
    break;
  case GL_DEBUG_TYPE_MARKER:
    std::cout << "Type: Marker";
    break;
  case GL_DEBUG_TYPE_PUSH_GROUP:
    std::cout << "Type: Push Group";
    break;
  case GL_DEBUG_TYPE_POP_GROUP:
    std::cout << "Type: Pop Group";
    break;
  case GL_DEBUG_TYPE_OTHER:
    std::cout << "Type: Other";
    break;
  }
  std::cout << std::endl;

  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH:
    std::cout << "Severity: high";
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    std::cout << "Severity: medium";
    break;
  case GL_DEBUG_SEVERITY_LOW:
    std::cout << "Severity: low";
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    std::cout << "Severity: notification";
    break;
  }
  std::cout << std::endl;
  std::cout << std::endl;
}
