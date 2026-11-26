#pragma once

#include <PxPhysicsAPI.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>
#include <cstdio>

#include <cstring>
#include <memory>

class PlayerPhysics {
    physx::PxDefaultAllocator allocator;
    physx::PxDefaultErrorCallback errors;

    physx::PxFoundation* foundation = nullptr;
    physx::PxPhysics* physics = nullptr;
    physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
    physx::PxScene* scene = nullptr;
    physx::PxMaterial* material = nullptr;
    physx::PxRigidStatic* ground = nullptr;
    physx::PxRigidStatic* triangleActor = nullptr;
    std::vector<physx::PxRigidStatic*> boxActors;
    std::vector<physx::PxRigidStatic*> interiorActors;
    physx::PxControllerManager* manager = nullptr;
    physx::PxController* controller = nullptr;

    bool extensionsReady = false;
    bool grounded = false;
    float verticalSpeed = 0.0f;

    bool loadTriangleCollider(std::ifstream& file)
    {
        using namespace physx;

        PxU32 triangleCount = 0;
        if (!file.read(reinterpret_cast<char*>(&triangleCount),
            sizeof(triangleCount)) || triangleCount > 5000000) {
            std::fprintf(stderr, "Invalid collision triangle count\n");
            return false;
        }
        if (triangleCount == 0) return true;

        std::printf("Cooking %u collision triangles...\n", triangleCount);

        // Each triangle has three XYZ positions stored consecutively.
        static_assert(sizeof(PxVec3) == 3 * sizeof(float),
            "The exported collision points are packed XYZ floats");

        const PxU32 pointCount = triangleCount * 3;
        std::vector<PxVec3> points(pointCount);
        if (!file.read(reinterpret_cast<char*>(points.data()),
            points.size() * sizeof(PxVec3))) {
            std::fprintf(stderr, "Collision triangle data is incomplete\n");
            return false;
        }

        // Points 0, 1, 2 form the first triangle, 3, 4, 5 the second, and so on.
        std::vector<PxU32> indices(pointCount);
        for (PxU32 i = 0; i < pointCount; ++i) {
            indices[i] = i;
        }

        PxTriangleMeshDesc meshDesc;
        meshDesc.points.count = pointCount;
        meshDesc.points.stride = sizeof(PxVec3);
        meshDesc.points.data = points.data();
        meshDesc.triangles.count = triangleCount;
        meshDesc.triangles.stride = 3 * sizeof(PxU32);
        meshDesc.triangles.data = indices.data();

        PxCookingParams cooking(physics->getTolerancesScale());

        // Join collision vertices that are within 1 mm of each other.
        cooking.meshWeldTolerance = 0.001f;
        cooking.meshPreprocessParams |= PxMeshPreprocessingFlag::eWELD_VERTICES;

        PxTriangleMesh* mesh = PxCreateTriangleMesh(
            cooking, meshDesc, physics->getPhysicsInsertionCallback());
        if (!mesh) {
            std::fprintf(stderr, "PhysX could not cook the collision mesh\n");
            return false;
        }

        // The vertices are already in world space, so the actor needs no offset.
        triangleActor = PxCreateStatic(
            *physics,
            PxTransform(PxIdentity),
            PxTriangleMeshGeometry(
                mesh, PxMeshScale(), PxMeshGeometryFlag::eDOUBLE_SIDED),
            *material
        );
        mesh->release();

        if (!triangleActor) {
            std::fprintf(stderr, "Could not create the static triangle actor\n");
            return false;
        }

        scene->addActor(*triangleActor);
        std::printf("Loaded %u static collision triangles\n", triangleCount);
        return true;
    }

    bool loadBoxColliders(const char* path)
    {
        using namespace physx;

        std::ifstream file(path, std::ios::binary);

        PxU32 count = 0;
        if (!file.read(reinterpret_cast<char*>(&count), sizeof(count)) ||
            count > 100000) {
            std::fprintf(stderr, "Could not read collision boxes from %s\n", path);
            return false;
        }

        boxActors.reserve(count);

        for (PxU32 i = 0; i < count; ++i) {
            // The file stores centre X, Y, Z, then three half-extents.
            float box[6]{};
            if (!file.read(reinterpret_cast<char*>(box), sizeof(box))) {
                std::fprintf(stderr, "Collision file ended at box %u\n", i);
                return false;
            }

            PxVec3 centre(box[0], box[1], box[2]);
            PxBoxGeometry shape(box[3], box[4], box[5]);

            if (!centre.isFinite() || !shape.isValid()) {
                std::fprintf(stderr, "Invalid collision box %u\n", i);
                return false;
            }

            PxRigidStatic* actor =
                PxCreateStatic(*physics, PxTransform(centre), shape, *material);

            if (!actor) {
                std::fprintf(stderr, "Could not create collision box %u\n", i);
                return false;
            }

            boxActors.push_back(actor);
            scene->addActor(*actor);
        }

        std::printf("Loaded %u static collision boxes\n", count);
        return loadTriangleCollider(file);
    }


    bool loadInteriorColliders(const char* path)
    {
        using namespace physx;

        std::ifstream file(path, std::ios::binary);
        auto read = [&file](auto& value) {
            return bool(file.read(reinterpret_cast<char*>(&value), sizeof(value)));
            };

        char magic[4]{};
        PxU32 version = 0;
        PxU32 meshCount = 0;

        if (!read(magic) || std::memcmp(magic, "PLYI", 4) != 0 ||
            !read(version) || version != 1 ||
            !read(meshCount) || meshCount > 1000) {
            std::fprintf(stderr, "Invalid building collision file: %s\n", path);
            return false;
        }

        static_assert(sizeof(PxVec3) == 3 * sizeof(float),
            "Building vertices are packed XYZ floats");

        PxCookingParams cooking(physics->getTolerancesScale());
        cooking.meshWeldTolerance = 0.001f;
        cooking.meshPreprocessParams |= PxMeshPreprocessingFlag::eWELD_VERTICES;

        for (PxU32 i = 0; i < meshCount; ++i) {
            PxU32 vertexCount = 0;
            PxU32 triangleCount = 0;

            if (!read(vertexCount) || !read(triangleCount) ||
                vertexCount == 0 || vertexCount > 2000000 ||
                triangleCount == 0 || triangleCount > 2000000) {
                return false;
            }

            std::vector<PxVec3> vertices(vertexCount);
            std::vector<PxU32> indices(triangleCount * 3);

            if (!file.read(reinterpret_cast<char*>(vertices.data()),
                vertices.size() * sizeof(PxVec3)) ||
                !file.read(reinterpret_cast<char*>(indices.data()),
                    indices.size() * sizeof(PxU32))) {
                return false;
            }

            PxTriangleMeshDesc desc;
            desc.points.count = vertexCount;
            desc.points.stride = sizeof(PxVec3);
            desc.points.data = vertices.data();
            desc.triangles.count = triangleCount;
            desc.triangles.stride = 3 * sizeof(PxU32);
            desc.triangles.data = indices.data();

            // Release our reference after creating the actors for this mesh.
            // Their collision shapes keep the mesh alive.
            auto releaseMesh = [](PxTriangleMesh* mesh) { mesh->release(); };
            std::unique_ptr<PxTriangleMesh, decltype(releaseMesh)> mesh(
                PxCreateTriangleMesh(cooking, desc,
                    physics->getPhysicsInsertionCallback()),
                releaseMesh);
            if (!mesh) {
                std::fprintf(stderr, "Could not cook building mesh %u\n", i);
                return false;
            }

            PxU32 instanceCount = 0;
            if (!read(instanceCount) || instanceCount > 10000) return false;

            for (PxU32 j = 0; j < instanceCount; ++j) {
                // Position XYZ, quaternion XYZW, then scale XYZ.
                float t[10]{};
                if (!read(t)) return false;

                PxVec3 position(t[0], t[1], t[2]);
                PxQuat rotation(t[3], t[4], t[5], t[6]);
                PxVec3 scale(t[7], t[8], t[9]);

                if (!position.isFinite() || !rotation.isUnit() ||
                    !scale.isFinite() || scale.x <= 0 ||
                    scale.y <= 0 || scale.z <= 0) {
                    return false;
                }

                PxTriangleMeshGeometry shape(
                    mesh.get(), PxMeshScale(scale),
                    PxMeshGeometryFlag::eDOUBLE_SIDED);

                PxRigidStatic* actor = PxCreateStatic(
                    *physics, PxTransform(position, rotation), shape, *material);
                if (!actor) return false;

                interiorActors.push_back(actor);
                scene->addActor(*actor);
            }
        }

        std::printf("Loaded %zu building colliders from %u shared meshes\n",
            interiorActors.size(), meshCount);
        return true;
    }

public:
    // The door system owns the returned actor and releases it before this scene.
    physx::PxRigidDynamic* createDoorCollider(
        const physx::PxTransform& closedPose, const physx::PxVec3& half)
    {
        using namespace physx;
        if (!scene || !closedPose.isValid() || !PxBoxGeometry(half).isValid()) {
            return nullptr;
        }

        auto* actor = physics->createRigidDynamic(closedPose);
        if (!actor) return nullptr;

        actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
        if (!PxRigidActorExt::createExclusiveShape(*actor, PxBoxGeometry(half), *material)) {
            actor->release();
            return nullptr;
        }
        scene->addActor(*actor);
        return actor;
    }

    bool doorPoseClear(const physx::PxTransform& pose, const physx::PxVec3& half) const
    {
        using namespace physx;
        const auto* capsule = static_cast<const PxCapsuleController*>(controller);
        const auto centre = capsule->getPosition();

        // A standalone PhysX capsule points along X. Rotate it upright.
        const PxTransform playerPose(
            PxVec3(static_cast<float>(centre.x), static_cast<float>(centre.y),
                static_cast<float>(centre.z)),
            PxQuat(PxHalfPi, PxVec3(0, 0, 1))
        );
        return !PxGeometryQuery::overlap(PxBoxGeometry(half), pose,
            PxCapsuleGeometry(capsule->getRadius(), capsule->getHeight() * 0.5f),
            playerPose);
    }

    void moveDoorCollider(physx::PxRigidDynamic& actor, const physx::PxTransform& pose)
    {
        actor.setGlobalPose(pose);
        controller->invalidateCache();
    }

    PlayerPhysics() = default;
    PlayerPhysics(const PlayerPhysics&) = delete;
    PlayerPhysics& operator=(const PlayerPhysics&) = delete;

    bool start(const char* terrainPath, const char* collidersPath,
        const char* interiorsPath)
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

        if (!loadBoxColliders(collidersPath)) return false;

        manager = PxCreateControllerManager(*scene);
        if (!manager) return false;

        if (!loadInteriorColliders(interiorsPath)) return false;

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

        if (triangleActor) triangleActor->release();

        if (material) material->release();
        for (auto* actor : boxActors) {
            actor->release();
        }

        for (auto* actor : interiorActors) {
            actor->release();
        }

        if (scene) scene->release();
        if (dispatcher) dispatcher->release();
        if (extensionsReady) PxCloseExtensions();
        if (physics) physics->release();
        if (foundation) foundation->release();
    }
};