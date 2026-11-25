#pragma once

#include <PxPhysicsAPI.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

class PlayerPhysics {
    physx::PxDefaultAllocator allocator;
    physx::PxDefaultErrorCallback errors;

    physx::PxFoundation* foundation = nullptr;
    physx::PxPhysics* physics = nullptr;
    physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
    physx::PxScene* scene = nullptr;
    physx::PxMaterial* material = nullptr;
    physx::PxRigidStatic* ground = nullptr;
    physx::PxControllerManager* manager = nullptr;
    physx::PxController* controller = nullptr;

    bool extensionsReady = false;
    bool grounded = false;
    float verticalSpeed = 0.0f;

public:
    PlayerPhysics() = default;
    PlayerPhysics(const PlayerPhysics&) = delete;
    PlayerPhysics& operator=(const PlayerPhysics&) = delete;

    bool start(const char* terrainPath)
    {
        using namespace physx;

        // File header: dimensions, first X/Z coordinate, and X/Z spacing.
        std::ifstream file(terrainPath, std::ios::binary);
        PxU32 width = 0, depth = 0;
        float minX = 0, minZ = 0, stepX = 0, stepZ = 0;
        auto read = [&file](auto& value) {
            return bool(file.read(reinterpret_cast<char*>(&value), sizeof(value)));
            };
        if (!read(width) || !read(depth) || !read(minX) || !read(minZ) ||
            !read(stepX) || !read(stepZ) || width < 2 || depth < 2 ||
            width > 4096 || depth > 4096 || stepX <= 0 || stepZ <= 0) {
            return false;
        }

        std::vector<float> heights(static_cast<size_t>(width) * depth);
        if (!file.read(reinterpret_cast<char*>(heights.data()),
            heights.size() * sizeof(float))) {
            return false;
        }

        foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errors);
        if (!foundation) return false;

        PxTolerancesScale scale;
        physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, scale);
        if (!physics) return false;

        extensionsReady = PxInitExtensions(*physics, nullptr);
        if (!extensionsReady) return false;

        dispatcher = PxDefaultCpuDispatcherCreate(2);
        if (!dispatcher) return false;

        PxSceneDesc sceneDesc(scale);
        sceneDesc.cpuDispatcher = dispatcher;
        sceneDesc.filterShader = PxDefaultSimulationFilterShader;
        scene = physics->createScene(sceneDesc);
        material = physics->createMaterial(0.6f, 0.6f, 0.0f);
        if (!scene || !material) return false;

        // The file is ordered by Z then X. PhysX expects X then Z here.
        // Store height in centimetres, then scale it back to metres.
        std::vector<PxHeightFieldSample> samples(heights.size());
        for (PxU32 x = 0; x < width; ++x) {
            for (PxU32 z = 0; z < depth; ++z) {
                auto& sample = samples[x * depth + z];
                sample.height = static_cast<PxI16>(
                    std::lround(heights[z * width + x] * 100.0f));
                sample.materialIndex0 = 0;
                sample.materialIndex1 = 0;
                sample.clearTessFlag();
            }
        }

        PxHeightFieldDesc fieldDesc;
        fieldDesc.nbRows = width;
        fieldDesc.nbColumns = depth;
        fieldDesc.samples.data = samples.data();
        fieldDesc.samples.stride = sizeof(PxHeightFieldSample);

        PxHeightField* field = PxCreateHeightField(
            fieldDesc, physics->getPhysicsInsertionCallback());
        if (!field) return false;

        ground = PxCreateStatic(*physics, PxTransform(PxVec3(minX, 0, minZ)),
            PxHeightFieldGeometry(field, PxMeshGeometryFlags(), 0.01f, stepX, stepZ),
            *material);
        field->release();
        if (!ground) return false;
        scene->addActor(*ground);

        manager = PxCreateControllerManager(*scene);
        if (!manager) return false;

        PxCapsuleControllerDesc capsule;
        capsule.radius = 0.28f;
        capsule.height = 1.13f;
        capsule.contactOffset = 0.025f;
        capsule.stepOffset = 0.3f;
        capsule.slopeLimit = 0.67f;
        capsule.material = material;
        controller = manager->createController(capsule);
        if (!controller) return false;

        // Start above the exported farm spawn and let the capsule settle.
        controller->setFootPosition(PxExtendedVec3(-560.0, 10.5, -190.0));
        return true;
    }

    void update(float vx, float vz, bool jump, float dt)
    {
        using namespace physx;

        if (dt <= 0.0f) return;
        if (jump && grounded) verticalSpeed = 5.0f;

        // Split long frames into smaller moves for steadier collision.
        for (float remaining = std::min(dt, 0.1f); remaining > 0.0f;) {
            const float step = std::min(remaining, 1.0f / 60.0f);
            remaining -= step;
            verticalSpeed = std::max(verticalSpeed - 9.81f * step, -40.0f);

            PxControllerCollisionFlags hit = controller->move(
                PxVec3(vx * step, verticalSpeed * step, vz * step),
                0.0005f, step, PxControllerFilters());

            grounded = hit.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN);
            if (grounded && verticalSpeed < 0.0f) verticalSpeed = 0.0f;
            if (hit.isSet(PxControllerCollisionFlag::eCOLLISION_UP) &&
                verticalSpeed > 0.0f) {
                verticalSpeed = 0.0f;
            }

            scene->simulate(step);
            scene->fetchResults(true);
        }
    }

    physx::PxExtendedVec3 feet() const
    {
        return controller->getFootPosition();
    }

    ~PlayerPhysics()
    {
        if (manager) manager->release();
        if (ground) ground->release();
        if (material) material->release();
        if (scene) scene->release();
        if (dispatcher) dispatcher->release();
        if (extensionsReady) PxCloseExtensions();
        if (physics) physics->release();
        if (foundation) foundation->release();
    }
};