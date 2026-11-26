#define GLEW_STATIC

#include <cstdio>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <GL/glew.h>

#include <fstream>
#include <sstream>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vector>

#include <cmath>

#include <algorithm>

#include <PxPhysicsAPI.h>

#include "MapPreview.h"
#include "PlayerPhysics.h"
#include "MapDoors.h"

#pragma comment(lib, "opengl32.lib")



//read a GLSL file and let the driver compile it. A zero handle signals failure.
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

//load the sword preview image from a separate BMP and upload it to the GPU.
GLuint loadTexture(const char* path)
{
    SDL_Surface* bmp = SDL_LoadBMP(path);
    if (!bmp) {
        std::fprintf(stderr, "Texture load failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_Surface* rgba = SDL_ConvertSurface(bmp, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(bmp);
    if (!rgba) {
        std::fprintf(stderr, "Texture conversion failed: %s\n", SDL_GetError());
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    //SDL gives the byte stride between rows. OpenGL wants the stride in RGBA pixels.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rgba->pitch / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgba->w, rgba->h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    //smaller copies reduce shimmering when the sword is far from the camera.
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    SDL_DestroySurface(rgba);
    return texture;
}

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    //SDL uses these attributes when it creates the context and its depth buffer.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

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

    //load OpenGL entry points after SDL has made the context current.
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

    //link the compiled stages into a single program that can be bound for drawing.
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

    //look up uniform locations once. Each frame supplies the current values.
    GLint yawLocation = glGetUniformLocation(shaderProgram, "uYaw");
    GLint aspectLocation = glGetUniformLocation(shaderProgram, "uAspect");

    GLint cameraZLocation = glGetUniformLocation(shaderProgram, "uCameraZ");
    float cameraZ = 3.0f;
    Uint64 previousTicks = SDL_GetTicks();

    //upload the original box geometry before attempting to load the sword.
    const float vertices[] = {
        //rectangle at Z = +0.35
        -0.6f, -0.5f,  0.35f,  // 0: bottom left
         0.6f, -0.5f,  0.35f,  // 1: bottom right
         0.6f,  0.5f,  0.35f,  // 2: top right
        -0.6f,  0.5f,  0.35f,  // 3: top left

        //matching corners at Z = -0.35
        -0.6f, -0.5f, -0.35f,  // 4: bottom left
         0.6f, -0.5f, -0.35f,  // 5: bottom right
         0.6f,  0.5f, -0.35f,  // 6: top right
        -0.6f,  0.5f, -0.35f   // 7: top left
    };

    const unsigned int indices[] = {
        0, 1, 2,   0, 2, 3,  // +Z face, the original rectangle
        5, 4, 7,   5, 7, 6,  // -Z face
        1, 5, 6,   1, 6, 2,  // Right
        4, 0, 3,   4, 3, 7,  // Left
        3, 2, 6,   3, 6, 7,  // Top
        4, 5, 1,   4, 1, 0   // Bottom
    };

    GLuint vao = 0;
    GLuint vbo = 0;

    //the VAO records the vertex layout and the index-buffer binding.
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

    glEnable(GL_DEPTH_TEST);

    GLsizei indexCount = 36;  //six box faces, two triangles per face.
    GLuint swordTexture = 0;


    //load the sword once. Assimp owns the scene data during this import.
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        "../../../sword_test_model.glb",
        aiProcess_Triangulate
    );

    //describe the sword vertex layout: three position floats followed by two UV floats.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    if (scene == nullptr) {
        std::fprintf(stderr, "Assimp import failed: %s\n",
            importer.GetErrorString());
    }
    else {
        std::printf("Loaded %u meshes\n", scene->mNumMeshes);

        for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
            const aiMesh* mesh = scene->mMeshes[i];
            std::printf("Mesh %u: %u vertices, %u faces\n",
                i, mesh->mNumVertices, mesh->mNumFaces);
        }

        if (scene->mNumMeshes > 0) {
            const aiMesh* mesh = scene->mMeshes[0];

            std::vector<float> swordVertices;
            for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
                const aiVector3D& position = mesh->mVertices[i];
                swordVertices.push_back(position.x);
                swordVertices.push_back(position.y);
                swordVertices.push_back(position.z);

                if (mesh->HasTextureCoords(0)) {
                    const aiVector3D& uv = mesh->mTextureCoords[0][i];
                    swordVertices.push_back(uv.x);
                    //match the top-first pixel rows loaded by SDL from the BMP.
                    swordVertices.push_back(1.0f - uv.y);
                }
                else {
                    swordVertices.push_back(0.0f);
                    swordVertices.push_back(0.0f);
                }
            }

            std::vector<unsigned int> swordIndices;
            for (unsigned i = 0; i < mesh->mNumFaces; ++i) {
                const aiFace& face = mesh->mFaces[i];
                if (face.mNumIndices != 3) {
                    continue;
                }

                swordIndices.push_back(face.mIndices[0]);
                swordIndices.push_back(face.mIndices[1]);
                swordIndices.push_back(face.mIndices[2]);
            }

            //replace the box data in the existing buffers after a successful import.
            if (!swordVertices.empty() && !swordIndices.empty()) {
                glBindVertexArray(vao);

                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER,
                    swordVertices.size() * sizeof(float),
                    swordVertices.data(), GL_STATIC_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    swordIndices.size() * sizeof(unsigned int),
                    swordIndices.data(), GL_STATIC_DRAW);

                indexCount = static_cast<GLsizei>(swordIndices.size());
                std::printf("Uploaded %d sword indices\n", indexCount);

                if (mesh->HasTextureCoords(0)) {
                    swordTexture = loadTexture("../../../Sword_BaseColor.bmp");
                }
            }
        }
    }

    GLint baseColorLocation = glGetUniformLocation(shaderProgram, "uBaseColor");
    GLint useTextureLocation = glGetUniformLocation(shaderProgram, "uUseTexture");
    GLint mapTextureModeLocation = glGetUniformLocation(shaderProgram, "uMapTextureMode");

    const GLuint staminaFrame = MapTextures{}.loadPng(L"assets/ui/stamina.png");
    if (staminaFrame != 0) {
        std::puts("Stamina frame loaded");
    }

    const GLint staminaHudLocation = glGetUniformLocation(shaderProgram, "uStaminaHud");
    const GLint staminaRectLocation = glGetUniformLocation(shaderProgram, "uStaminaRect");
    const GLint staminaFractionLocation = glGetUniformLocation(shaderProgram, "uStaminaFraction");

    //the map owns separate mesh buffers and a list of where to draw each mesh.
    MapPreview map;
    bool showMap = map.load("../../../Plymouth_Editor/export/world.glb");

    GLint mapPreviewLocation = glGetUniformLocation(shaderProgram, "uMapPreview");
    GLint modelLocation = glGetUniformLocation(shaderProgram, "uModel");
    GLint mapTintLocation = glGetUniformLocation(shaderProgram, "uMapTint");

    GLint mapEyeLocation = glGetUniformLocation(shaderProgram, "uMapEye");
    GLint mapYawLocation = glGetUniformLocation(shaderProgram, "uMapYaw");

    //start above the town. The map uses Y for height and looks towards -Z at zero yaw.
    float mapX = -80.0f;
    float mapY = 75.0f;
    float mapZ = 235.0f;
    float mapYaw = 0.0f;

    previousTicks = SDL_GetTicks();  //exclude the time it took to load the map.

    GLint mapPitchLocation = glGetUniformLocation(shaderProgram, "uMapPitch");
    float mapPitch = 0.4363f;  //start at the same 25-degree downward angle.
    const float mouseSensitivity = 0.0025f;

    if (showMap && !SDL_SetWindowRelativeMouseMode(window, true)) {
        std::fprintf(stderr, "Could not capture mouse: %s\n", SDL_GetError());
    }

    /*physx::PxDefaultAllocator allocator;
    physx::PxDefaultErrorCallback errors;

    physx::PxFoundation* foundation =
        PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errors);

    if (foundation) {
        std::puts("PhysX foundation ready");
        foundation->release();
    } */

    PlayerPhysics player;
    bool canWalk = showMap && player.start(
        "../../../Plymouth_Editor/export/terrain.bin",
        "../../../Plymouth_Editor/export/colliders.bin",
        "../../../Plymouth_Editor/export/interiors.bin"
    );

    // Declare doors after player so their colliders are released before the PhysX scene.
    MapDoors doors;
    if (canWalk) {
        doors.load("../../../Plymouth_Editor/export/world.json", map, player);
    }

    if (showMap && !canWalk) {
        std::fprintf(stderr, "Player physics failed to start. Fly mode is available.\n");
    }

    bool flying = true;

    constexpr float walkSpeed = 4.0f;
    constexpr float sprintSpeed = 10.0f;
    constexpr float maxStamina = 100.0f;
    constexpr float sprintDrainPerSecond = 20.0f;
    constexpr float staminaRecoveryPerSecond = 15.0f;
    constexpr float recoveryDelay = 0.75f;
    constexpr float staminaToResume = 25.0f;
    constexpr float jumpStaminaCost = 5.0f;

    constexpr Uint64 jumpBufferMs = 150;
    Uint64 jumpQueuedUntil = 0;

    float stamina = maxStamina;
    float recoveryDelayRemaining = 0.0f;
    bool exhausted = false;

    previousTicks = SDL_GetTicks();

    bool running = true;
    //each frame processes input, updates the camera, draws and presents the result.
    while (running) {
        SDL_Event event;

        bool jumpPressed = false;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }

            //escape releases the pointer. Click the map window to capture it again.
            if (showMap) {
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    SDL_SetWindowRelativeMouseMode(window, false);
                }

                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                    event.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetWindowRelativeMouseMode(window, true);
                }

                if (event.type == SDL_EVENT_MOUSE_MOTION &&
                    SDL_GetWindowRelativeMouseMode(window)) {
                    //relative motion is already a delta. Only held-key movement uses frame time.
                    mapYaw += event.motion.xrel * mouseSensitivity;
                    mapPitch += event.motion.yrel * mouseSensitivity;

                    //keep the camera from pitching through vertical and flipping over.
                    if (mapPitch < -1.55f) mapPitch = -1.55f;
                    if (mapPitch > 1.55f) mapPitch = 1.55f;
                }

                if (canWalk && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (event.key.scancode == SDL_SCANCODE_F2) {
                        flying = !flying;
                        std::puts(flying ? "Fly mode" : "Walk mode");
                    }
                    if (event.key.scancode == SDL_SCANCODE_SPACE && !flying) {
                        jumpPressed = true;
                    }

                    if (event.key.scancode == SDL_SCANCODE_E && !flying) {
                        doors.toggleNearest(map, player);
                    }
                }
            }
        }

        Uint64 now = SDL_GetTicks();
        float deltaTime = static_cast<float>(now - previousTicks) / 1000.0f;
        previousTicks = now;

        if (flying) {
            jumpQueuedUntil = 0;
        }
        else if (jumpPressed) {
            jumpQueuedUntil = now + jumpBufferMs;
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);

        if (showMap) {
            float turn = 0.0f;
            if (keys[SDL_SCANCODE_LEFT])  turn -= 1.0f;
            if (keys[SDL_SCANCODE_RIGHT]) turn += 1.0f;
            mapYaw += turn * 1.2f * deltaTime;

            float forward = 0.0f;
            float strafe = 0.0f;
            if (keys[SDL_SCANCODE_W]) forward += 1.0f;
            if (keys[SDL_SCANCODE_S]) forward -= 1.0f;
            if (keys[SDL_SCANCODE_D]) strafe += 1.0f;
            if (keys[SDL_SCANCODE_A]) strafe -= 1.0f;

            //yaw turns forward and strafe input into movement on the horizontal XZ plane.
            /*float step = 25.0f * deltaTime;
            mapX += (std::sin(mapYaw) * forward + std::cos(mapYaw) * strafe) * step;
            mapZ += (-std::cos(mapYaw) * forward + std::sin(mapYaw) * strafe) * step;

            if (keys[SDL_SCANCODE_SPACE]) mapY += step;
            if (keys[SDL_SCANCODE_LCTRL]) mapY -= step;*/

            float moveX = std::sin(mapYaw) * forward + std::cos(mapYaw) * strafe;
            float moveZ = -std::cos(mapYaw) * forward + std::sin(mapYaw) * strafe;

            if (flying) {
                float step = 25.0f * deltaTime;
                mapX += moveX * step;
                mapZ += moveZ * step;

                if (keys[SDL_SCANCODE_SPACE]) mapY += step;
                if (keys[SDL_SCANCODE_LCTRL]) mapY -= step;
            }
            else {
                const float inputLength = std::sqrt(moveX * moveX + moveZ * moveZ);
                if (inputLength > 1.0f) {
                    moveX /= inputLength;
                    moveZ /= inputLength;
                }

                const float movementDt = std::min(deltaTime, 0.1f);
                const bool grounded = player.isGrounded();
                const bool jumpNow = grounded && movementDt > 0.0f
                    && now < jumpQueuedUntil;
                const bool sprinting = grounded && !jumpNow && !exhausted
                    && forward > 0.0f && keys[SDL_SCANCODE_LSHIFT];

                if (jumpNow) {
                    jumpQueuedUntil = 0;
                    stamina = std::max(0.0f, stamina - jumpStaminaCost);
                    recoveryDelayRemaining = recoveryDelay;
                    if (stamina <= 0.0f) {
                        exhausted = true;
                    }
                }
                else if (sprinting) {
                    stamina = std::max(0.0f, stamina - sprintDrainPerSecond * movementDt);
                    recoveryDelayRemaining = recoveryDelay;
                    exhausted = stamina <= 0.0f;
                }
                else if (grounded) {
                    const float recoveryTime =
                        std::max(0.0f, movementDt - recoveryDelayRemaining);
                    recoveryDelayRemaining =
                        std::max(0.0f, recoveryDelayRemaining - movementDt);
                    stamina = std::min(maxStamina, stamina + staminaRecoveryPerSecond * recoveryTime);

                    if (stamina >= staminaToResume) {
                        exhausted = false;
                    }
                }

                const float speed = sprinting ? sprintSpeed : walkSpeed;
                player.update(moveX * speed, moveZ * speed, jumpNow, movementDt);

                auto feet = player.feet();
                mapX = static_cast<float>(feet.x);
                mapY = static_cast<float>(feet.y) + 1.65f;
                mapZ = static_cast<float>(feet.z);
            }
        }

        //W and S also update the sword preview distance for the fallback draw.
        const float cameraSpeed = 1.5f;

        if (keys[SDL_SCANCODE_W]) {
            cameraZ -= cameraSpeed * deltaTime;
        }

        if (keys[SDL_SCANCODE_S]) {
            cameraZ += cameraSpeed * deltaTime;
        }

        if (cameraZ < 1.2f) cameraZ = 1.2f;
        if (cameraZ > 8.0f) cameraZ = 8.0f;

        glClearColor(0.08f, 0.18f, 0.32f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

        glUseProgram(shaderProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, swordTexture);
        glUniform1i(baseColorLocation, 0);  //the sampler reads texture unit 0.
        glUniform1i(useTextureLocation, swordTexture != 0);

        glUniform1f(cameraZLocation, cameraZ);

        glBindVertexArray(vao);

        int pixelWidth = 0;
        int pixelHeight = 0;

        //use the framebuffer size so projection and viewport agree after resizing.
        if (SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight)
            && pixelWidth > 0 && pixelHeight > 0) {
            glViewport(0, 0, pixelWidth, pixelHeight);
            glUniform1f(aspectLocation,
                static_cast<float>(pixelWidth) / pixelHeight);

        }

        float seconds = static_cast<float>(SDL_GetTicks()) / 1000.0f;
        glUniform1f(yawLocation, seconds * 0.8f);

        //both previews share the shader program. This uniform selects the map path.

        glUniform1i(mapPreviewLocation, showMap);

        if (showMap) {
            glUniform1i(useTextureLocation, 0);
            glUniform3f(mapEyeLocation, mapX, mapY, mapZ);
            glUniform1f(mapYawLocation, mapYaw);
            glUniform1f(mapPitchLocation, mapPitch);
            map.draw(modelLocation, mapTintLocation, mapTextureModeLocation);
        }
        else {
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        }

        if (!flying && staminaFrame != 0 && pixelWidth > 0 && pixelHeight > 0) {
            const float width = std::min(360.0f, pixelWidth * 0.8f);
            const float height = width * 119.0f / 1000.0f;
            const float margin = 16.0f;

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glBindVertexArray(vao);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, staminaFrame);
            glUniform1i(staminaHudLocation, 1);
            glUniform1f(staminaFractionLocation, stamina / maxStamina);
            glUniform4f(staminaRectLocation,
                -1.0f + 2.0f * margin / pixelWidth,
                -1.0f + 2.0f * (margin + height) / pixelHeight,
                2.0f * width / pixelWidth,
                2.0f * height / pixelHeight);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glUniform1i(staminaHudLocation, 0);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        SDL_GL_SwapWindow(window);
    }

    //release GPU buffers while the OpenGL context is still active.
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteTextures(1, &staminaFrame);
    map.release();
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}