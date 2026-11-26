#pragma once

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "MapPreview.h"
#include "PlayerPhysics.h"

class MapDoors {
    struct ReleaseActor {
        void operator()(physx::PxRigidDynamic* actor) const { actor->release(); }
    };

    struct Leaf {
        std::string node;
        physx::PxVec3 hinge{0.0f};
        physx::PxVec3 half{0.0f};
        physx::PxTransform closed{physx::PxIdentity};
        float openAngle = 0.0f;
        std::unique_ptr<physx::PxRigidDynamic, ReleaseActor> actor;

        physx::PxTransform pose(float angle) const
        {
            const physx::PxQuat turn(angle, physx::PxVec3(0, 1, 0));
            return physx::PxTransform(
                hinge + turn.rotate(closed.p - hinge), turn * closed.q);
        }

        float reachDistanceSquared(const physx::PxVec3& point) const
        {
            // Measure to the closed box so an open leaf cannot pull the target away
            // from its doorway. The box also gives tall and wide doors a fair reach.
            const auto local = closed.transformInv(point);
            const float dx = physx::PxMax(0.0f, std::abs(local.x) - half.x);
            const float dy = physx::PxMax(0.0f, std::abs(local.y) - half.y);
            const float dz = physx::PxMax(0.0f, std::abs(local.z) - half.z);
            return dx * dx + dy * dy + dz * dz;
        }
    };

    struct Doorway {
        std::vector<Leaf> leaves;
        bool open = false;
    };

    // Read the door fields exported by Blender. Generated node names and group
    // IDs are ASCII. Other manifest values are skipped without building a JSON tree.
    class ManifestReader {
        std::istream& input;

        bool take(char token)
        {
            if ((input >> std::ws).peek() != token) return false;
            input.get();
            return true;
        }

        bool readString(std::string& value)
        {
            if ((input >> std::ws).peek() != '"') return false;
            return bool(input >> std::quoted(value));
        }

        template<class ReadItem>
        bool readList(char begin, char end, ReadItem readItem)
        {
            if (!take(begin)) return false;
            if (take(end)) return true;
            do {
                if (!readItem()) return false;
                if (take(end)) return true;
            } while (take(','));
            return false;
        }

        bool skipValue(int depth = 0)
        {
            if (depth >= 64) return false;
            std::string text;
            switch ((input >> std::ws).peek()) {
            case '"':
                return readString(text);
            case '{':
                return readList('{', '}', [&] {
                    return readString(text) && take(':') && skipValue(depth + 1);
                });
            case '[':
                return readList('[', ']', [&] { return skipValue(depth + 1); });
            case 't':
            case 'f':
            case 'n': {
                while (input.peek() >= 'a' && input.peek() <= 'z') {
                    text += static_cast<char>(input.get());
                }
                return text == "true" || text == "false" || text == "null";
            }
            default:
                double number;
                return bool(input >> number);
            }
        }

        template<std::size_t N>
        bool readNumbers(float (&values)[N])
        {
            if (!take('[')) return false;
            for (std::size_t i = 0; i < N; ++i) {
                if (i != 0 && !take(',')) return false;
                if (!(input >> values[i]) || !std::isfinite(values[i])) return false;
            }
            return take(']');
        }

        bool readVector(physx::PxVec3& value)
        {
            float xyz[3];
            if (!readNumbers(xyz)) return false;
            value = physx::PxVec3(xyz[0], xyz[1], xyz[2]);
            return true;
        }

        bool readRotation(physx::PxQuat& value)
        {
            float xyzw[4];
            if (!readNumbers(xyzw)) return false;
            value = physx::PxQuat(xyzw[0], xyzw[1], xyzw[2], xyzw[3]);
            return value.isUnit();
        }

        bool readLeaf(Leaf& leaf, std::string& group)
        {
            // A sentinel makes a missing hinge or centre fail validation.
            leaf.hinge = physx::PxVec3(std::numeric_limits<float>::quiet_NaN());
            leaf.closed.p = leaf.hinge;
            leaf.closed.q = physx::PxQuat(0, 0, 0, 0);

            const bool read = readList('{', '}', [&] {
                std::string key;
                if (!readString(key) || !take(':')) return false;
                if (key == "node") return readString(leaf.node);
                if (key == "group") return readString(group);
                if (key == "hinge") return readVector(leaf.hinge);
                if (key == "center") return readVector(leaf.closed.p);
                if (key == "half") return readVector(leaf.half);
                if (key == "rotation") return readRotation(leaf.closed.q);
                if (key == "open_angle") return bool(input >> leaf.openAngle);
                return skipValue();
            });
            return read && !leaf.node.empty() && !group.empty() &&
                leaf.hinge.isFinite() && leaf.closed.isValid() &&
                physx::PxBoxGeometry(leaf.half).isValid() &&
                std::isfinite(leaf.openAngle) && leaf.openAngle != 0.0f;
        }

    public:
        explicit ManifestReader(std::istream& stream) : input(stream) {}

        bool read(std::vector<Doorway>& result)
        {
            std::unordered_map<std::string, std::size_t> groupIndices;
            std::unordered_set<std::string> nodeNames;
            bool foundDoors = false;
            const bool parsed = readList('{', '}', [&] {
                std::string key;
                if (!readString(key) || !take(':')) return false;
                if (key != "doors") return skipValue();
                if (foundDoors) return false;
                foundDoors = true;
                return readList('[', ']', [&] {
                    Leaf leaf;
                    std::string group;
                    if (!readLeaf(leaf, group) || !nodeNames.insert(leaf.node).second) {
                        return false;
                    }
                    auto entry = groupIndices.emplace(group, result.size());
                    if (entry.second) result.emplace_back();
                    result[entry.first->second].leaves.push_back(std::move(leaf));
                    return true;
                });
            });
            return parsed && foundDoors &&
                (input >> std::ws).peek() == std::char_traits<char>::eof();
        }
    };

    std::vector<Doorway> doorways;

public:
    void load(const char* path, const MapPreview& map, PlayerPhysics& player)
    {
        std::ifstream file(path);
        file.imbue(std::locale::classic());
        std::vector<Doorway> exported;
        if (!file || !ManifestReader(file).read(exported)) {
            std::fprintf(stderr, "Could not read door metadata from %s\n", path);
            return;
        }

        std::size_t leafCount = 0;
        for (auto& doorway : exported) {
            bool ready = true;
            for (auto& leaf : doorway.leaves) {
                if (map.hasDoor(leaf.node)) {
                    leaf.actor.reset(player.createDoorCollider(leaf.closed, leaf.half));
                }
                if (!leaf.actor) {
                    std::fprintf(stderr, "Could not set up doorway for %s\n", leaf.node.c_str());
                    ready = false;
                    break;
                }
            }
            if (ready) {
                leafCount += doorway.leaves.size();
                doorways.push_back(std::move(doorway));
            }
        }
        std::printf("Loaded %zu interactive doorways (%zu leaves)\n",
            doorways.size(), leafCount);
    }

    bool toggleNearest(MapPreview& map, PlayerPhysics& player)
    {
        const auto feet = player.feet();
        const physx::PxVec3 point(static_cast<float>(feet.x),
            static_cast<float>(feet.y) + 1.65f, static_cast<float>(feet.z));
        float nearestDistance = 2.7f * 2.7f;
        Doorway* nearest = nullptr;
        for (auto& doorway : doorways) {
            for (const auto& leaf : doorway.leaves) {
                const float distance = leaf.reachDistanceSquared(point);
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    nearest = &doorway;
                }
            }
        }
        if (!nearest) return false;

        // Check every leaf first so a double door either moves together or stays put.
        for (const auto& leaf : nearest->leaves) {
            const float angle = nearest->open ? 0.0f : leaf.openAngle;
            if (!player.doorPoseClear(leaf.pose(angle), leaf.half)) return false;
        }
        for (const auto& leaf : nearest->leaves) {
            const float angle = nearest->open ? 0.0f : leaf.openAngle;
            player.moveDoorCollider(*leaf.actor, leaf.pose(angle));
            map.setDoorAngle(leaf.node, aiVector3D(leaf.hinge.x, leaf.hinge.y, leaf.hinge.z), angle);
        }
        nearest->open = !nearest->open;
        return true;
    }
};
