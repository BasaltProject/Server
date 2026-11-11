#include "PluginBulkSupport.hpp"

namespace basalt::server::plugins {
using namespace bulk;

void addBulkBlockEntityMethods(PluginObject& object, const std::shared_ptr<PluginWorldRef>& world,
                               const std::shared_ptr<WorldHooks>& hooks) {
    object
        .method("block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    pushText(args, snbtOf(world->blockEntityNbt(spot.x, spot.y, spot.z)));
                    return 1;
                })
        .method("set_block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->setBlockEntityNbt(spot.x, spot.y, spot.z, bytesOf(args, spot.next)));
                    return 1;
                })
        .method("create_block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->createBlockEntity(spot.x, spot.y, spot.z,
                                                              args.isNil(spot.next) ? std::vector<std::uint8_t>{}
                                                                                    : bytesOf(args, spot.next)));
                    return 1;
                })
        .method("remove_block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->removeBlockEntity(spot.x, spot.y, spot.z));
                    return 1;
                })
        .method("remove_entity",
                [world](LuaArgs& args) {
                    args.pushBoolean(world->removeEntityByUuid(args.toText(firstArgument(args))));
                    return 1;
                })
        .method("block_entities",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> flat =
                        world->blockEntityPositions(intAt(args, base), intAt(args, base + 1));
                    args.pushTable();
                    for (std::size_t triple = 0; triple + 2 < flat.size(); triple += 3) {
                        args.pushTable();
                        for (std::size_t part = 0; part < 3; part++) {
                            args.pushInteger(flat[triple + part]);
                            args.setIndex(static_cast<std::int64_t>(part) + 1);
                        }
                        args.setIndex(static_cast<std::int64_t>(triple / 3) + 1);
                    }
                    return 1;
                })
        .method("entity_nbt",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::int32_t id = 0;
                    if (args.isTable(base)) {
                        args.pushField(base, "id");
                        id = static_cast<std::int32_t>(args.toInteger(-1));
                        args.pop(1);
                    } else {
                        id = intAt(args, base);
                    }
                    pushText(args, snbtOf(world->entityNbt(id)));
                    return 1;
                })
        .method("plugin_data",
                [world](LuaArgs& args) {
                    pushText(args, snbtOf(world->pluginData()));
                    return 1;
                })
        .method("set_plugin_data",
                [world](LuaArgs& args) {
                    args.pushBoolean(world->setPluginData(bytesOf(args, firstArgument(args))));
                    return 1;
                })
        .method("chunk_data",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    pushText(args, snbtOf(world->chunkPluginData(intAt(args, base), intAt(args, base + 1))));
                    return 1;
                })
        .method("set_chunk_data",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(
                        world->setChunkPluginData(intAt(args, base), intAt(args, base + 1), bytesOf(args, base + 2)));
                    return 1;
                })
        .method("block_entity_data",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    pushText(args, snbtOf(world->blockEntityPluginData(spot.x, spot.y, spot.z)));
                    return 1;
                })
        .method("set_block_entity_data",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->setBlockEntityPluginData(spot.x, spot.y, spot.z, bytesOf(args, spot.next)));
                    return 1;
                })
        .method("entity_data",
                [world](LuaArgs& args) {
                    pushText(args, snbtOf(world->entityPluginData(entityIdAt(args, firstArgument(args)))));
                    return 1;
                })
        .method("set_entity_data",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(world->setEntityPluginData(entityIdAt(args, base), bytesOf(args, base + 1)));
                    return 1;
                })
        .method("spawn_nbt",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::int32_t id = world->spawnEntityNbt(
                        bytesOf(args, base), args.isString(base + 1) ? args.toText(base + 1) : std::string{"plugin"});
                    const std::shared_ptr<PluginEntityRef> made = id < 0 ? nullptr : world->entityById(id);
                    if (made == nullptr) {
                        args.pushNil();
                    } else {
                        pushEntity(args, made);
                    }
                    return 1;
                })
        .method("entity",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::shared_ptr<PluginEntityRef> found = args.isString(base) && !args.isNumber(base)
                                                                       ? world->entityByUuid(args.toText(base))
                                                                       : world->entityById(intAt(args, base));
                    if (found == nullptr) {
                        args.pushNil();
                    } else {
                        pushEntity(args, found);
                    }
                    return 1;
                })
        .method("entities_in",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const std::shared_ptr<PluginEntityRef>& entity :
                         world->entitiesIn(intAt(args, base), intAt(args, base + 1))) {
                        pushEntity(args, entity);
                        args.setIndex(position++);
                    }
                    return 1;
                })
        .method("biomes",
                [world, hooks](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::vector<std::int32_t> ids(biomeCells(*world));
                    if (!world->readBiomes(intAt(args, base), intAt(args, base + 1), ids)) {
                        args.pushNil();
                        return 1;
                    }
                    const std::vector<std::string> names =
                        hooks->biomeIds ? hooks->biomeIds() : std::vector<std::string>{};
                    args.pushTable();
                    for (std::size_t cell = 0; cell < ids.size(); cell++) {
                        const auto id = static_cast<std::size_t>(ids[cell]);
                        args.pushString(id < names.size() ? names[id] : "");
                        args.setIndex(static_cast<std::int64_t>(cell) + 1);
                    }
                    return 1;
                })
        .method("set_biomes", [world, hooks](LuaArgs& args) {
            const std::int32_t base = firstArgument(args);
            if (!args.isTable(base + 2)) {
                args.raise("set_biomes(chunk_x, chunk_z, biomes) takes one biome name per cell");
            }
            std::unordered_map<std::string, std::int32_t> byName;
            const std::vector<std::string> names = hooks->biomeIds ? hooks->biomeIds() : std::vector<std::string>{};
            for (std::size_t id = 0; id < names.size(); id++) {
                byName.emplace(names[id], static_cast<std::int32_t>(id));
            }
            std::vector<std::int32_t> ids(biomeCells(*world), -1);
            for (std::size_t cell = 0; cell < ids.size(); cell++) {
                args.pushIndex(base + 2, static_cast<std::int64_t>(cell) + 1);
                if (args.isString(-1)) {
                    const auto found = byName.find(args.toText(-1));
                    if (found == byName.end()) {
                        args.raise("'" + args.toText(-1) + "' is not a biome");
                    }
                    ids[cell] = found->second;
                }
                args.pop(1);
            }
            args.pushBoolean(world->writeBiomes(intAt(args, base), intAt(args, base + 1), ids));
            return 1;
        });
}
} // namespace basalt::server::plugins
