#define GLEW_STATIC

#include <cstdio>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <GL/glew.h>

#include <fstream>
#include <sstream>
#include <string>

#pragma comment(lib, "opengl32.lib")



GLuint compileShader(GLenum type, const char* path) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "Could not open %s\n", path);
        return 0;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    const char* text = source.c_str();

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "%s failed to compile: \n%s\n", path, log);
        glDeleteShader(shader);
        return 0;
    }

    std::printf("Cmpiled %s\n", path);
    return shader;
}

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "SDL3 + OpenGL 4", 960, 540,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);

    glewExperimental = GL_TRUE;
    GLenum status = glewInit();

    if (status != GLEW_OK) {
        std::fprintf(stderr, "GLEW failed: %s\n",
            reinterpret_cast<const char*>(glewGetErrorString(status)));
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::puts("GLEW ready");

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, "basic.vert");
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, "basic.frag");

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint linked = GL_FALSE;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);

    if (linked == GL_TRUE) {
        std::puts("Shader program linked");
    } else {
        char log[2048]{};
        glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader link failed:\n%s\n", log);
    }

    SDL_GL_SetSwapInterval(1);
    std::printf("OpenGL version: %s\n",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));



    const float vertices[] = {
        -0.6f, -0.5f, 0.0f, //bottom left
        0.6f, -0.5f, 0.0f, //bottom right
        0.6f, 0.5f, 0.0f, //top right
        -0.6f, 0.5f, 0.0f //top left
    };

    const unsigned int indices[] = {
        0,1,2,
        0,2,3
    };

    GLuint vao = 0;
    GLuint vbo = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    GLuint ebo = 0;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        glClearColor(0.08f, 0.18f, 0.32f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        SDL_GL_SwapWindow(window);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}