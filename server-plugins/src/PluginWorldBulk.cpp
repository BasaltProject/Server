#include "PluginBulkSupport.hpp"

namespace basalt::server::plugins {
using namespace bulk;

void addBulkWorldMethods(PluginObject& object, const std::shared_ptr<PluginWorldRef>& world,
                         const std::shared_ptr<WorldHooks>& hooks) {
    object.reads("block_version", [world] { return PluginValue{static_cast<std::int64_t>(world->blockVersion())}; })
        .reads("folder", [world] { return PluginValue{world->folder()}; })
        .method("section",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::vector<std::int32_t> out(kSectionBlocks);
                    if (!world->readSection(intAt(args, base), intAt(args, base + 1), intAt(args, base + 2),
                                            args.isNil(base + 3) || args.booleanOr(base + 3, true), out)) {
                        args.pushNil();
                        return 1;
                    }
                    pushInts(args, out);
                    return 1;
                })
        .method("set_section",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> states = statesFrom(args, base + 3, kSectionBlocks);
                    args.pushBoolean(world->writeSection(intAt(args, base), intAt(args, base + 1),
                                                         intAt(args, base + 2), states,
                                                         args.isNil(base + 4) || args.booleanOr(base + 4, true)));
                    return 1;
                })
        .method("apply_chunk",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const Sections sections = sectionsFrom(args, base + 2);
                    const bool resend = flag(args, base + 3, "resend", true);
                    const bool wantPrevious = flag(args, base + 3, "previous", false);
                    std::vector<std::int32_t> previous(wantPrevious ? sections.states.size() : 0);
                    const std::int32_t changed = world->applySections(intAt(args, base), intAt(args, base + 1),
                                                                      sections.ys, sections.states, resend, previous);
                    if (changed < 0) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushInteger(changed);
                    if (!wantPrevious) {
                        return 1;
                    }
                    pushPrevious(args, sections.ys, previous);
                    return 2;
                })
        .method("apply_chunks",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (!args.isTable(base)) {
                        args.raise("apply_chunks takes a list of { x, z, sections }");
                    }
                    std::vector<std::int32_t> chunks;
                    Sections all;
                    const std::int64_t length = args.length(base);
                    for (std::int64_t position = 1; position <= length; position++) {
                        args.pushIndex(base, position);
                        const std::int32_t entry = args.top();
                        args.pushField(entry, "x");
                        const std::int32_t chunkX = intAt(args, -1);
                        args.pop(1);
                        args.pushField(entry, "z");
                        const std::int32_t chunkZ = intAt(args, -1);
                        args.pop(1);
                        args.pushField(entry, "sections");
                        const Sections sections = sectionsFrom(args, args.top());
                        args.pop(2);
                        chunks.insert(chunks.end(), {chunkX, chunkZ, static_cast<std::int32_t>(sections.ys.size())});
                        all.ys.insert(all.ys.end(), sections.ys.begin(), sections.ys.end());
                        all.states.insert(all.states.end(), sections.states.begin(), sections.states.end());
                    }
                    const bool resend = flag(args, base + 1, "resend", true);
                    const bool wantPrevious = flag(args, base + 1, "previous", false);
                    std::vector<std::int32_t> previous(wantPrevious ? all.states.size() : 0);
                    const std::int32_t changed = world->applyChunks(chunks, all.ys, all.states, resend, previous);
                    if (changed < 0) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushInteger(changed);
                    if (!wantPrevious) {
                        return 1;
                    }
                    args.pushTable();
                    std::size_t section = 0;
                    for (std::size_t chunk = 0; chunk < chunks.size() / 3; chunk++) {
                        const auto count = static_cast<std::size_t>(chunks[(chunk * 3) + 2]);
                        pushPrevious(args, std::span<const std::int32_t>{all.ys}.subspan(section, count),
                                     std::span<const std::int32_t>{previous}.subspan(section * kSectionBlocks,
                                                                                     count * kSectionBlocks));
                        args.setIndex(static_cast<std::int64_t>(chunk) + 1);
                        section += count;
                    }
                    return 2;
                })
        .method("chunk_sections",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::size_t count = static_cast<std::size_t>(std::max(world->height(), 0)) >> 4;
                    std::vector<std::int32_t> all(count * kSectionBlocks);
                    if (!world->readSections(intAt(args, base), intAt(args, base + 1),
                                             args.isNil(base + 2) || args.booleanOr(base + 2, true), all)) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushTable();
                    const std::int32_t first = world->minHeight() >> 4;
                    for (std::size_t section = 0; section < count; section++) {
                        pushInts(args,
                                 std::span<const std::int32_t>{all}.subspan(section * kSectionBlocks, kSectionBlocks));
                        args.setIndex(first + static_cast<std::int64_t>(section));
                    }
                    return 1;
                })
        .method("chunk_status",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushString(
                        std::string{statusName(world->chunkStatus(intAt(args, base), intAt(args, base + 1)))});
                    return 1;
                })
        .method("hold_chunks",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(world->holdChunks(args.checkString(base), chunkPairs(args, base + 1)));
                    return 1;
                })
        .method("release_chunks",
                [world](LuaArgs& args) {
                    world->releaseChunks(args.checkString(firstArgument(args)));
                    return 0;
                })
        .method("update_neighbors",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    const std::vector<std::int32_t> previous =
                        args.isTable(base + 2) ? stateList(args, base + 2, at.size() / 3) : std::vector<std::int32_t>{};
                    args.pushInteger(world->settleBlocks(at, previous, effectsFrom(args, base + 1)));
                    return 1;
                })
        .method("use_item_on",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    const std::int32_t face = faceFrom(args, spot.next);
                    args.pushInteger(world->useItemOn(
                        spot.x, spot.y, spot.z, face, args.checkString(spot.next + 1),
                        static_cast<float>(args.isNumber(spot.next + 2) ? args.toNumber(spot.next + 2) : 0.0),
                        static_cast<float>(args.isNumber(spot.next + 3) ? args.toNumber(spot.next + 3) : 0.0)));
                    return 1;
                })
        .method("valid_state",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushInteger(world->validState(spot.x, spot.y, spot.z,
                                                       static_cast<std::int32_t>(args.toInteger(spot.next))));
                    return 1;
                })
        .method("has_tick",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->hasScheduledTick(spot.x, spot.y, spot.z));
                    return 1;
                })
        .method("schedule_tick",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    const bool fluid = args.isString(spot.next + 1) && args.toText(spot.next + 1) == "fluid";
                    args.pushBoolean(world->scheduleTick(spot.x, spot.y, spot.z,
                                                         static_cast<std::int32_t>(args.toInteger(spot.next)), fluid));
                    return 1;
                })
        .method("neighbor_changed",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    const std::vector<std::int32_t> sources =
                        args.isTable(base + 1) ? stateList(args, base + 1, at.size() / 3) : std::vector<std::int32_t>{};
                    args.pushInteger(world->neighborChanged(at, sources));
                    return 1;
                })
        .method("update_shapes",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    const bool indirect = args.isTable(base + 1);
                    const std::vector<std::int32_t> states =
                        indirect ? stateList(args, base + 1, at.size() / 3) : std::vector<std::int32_t>{};
                    args.pushInteger(world->updateShapes(at, states, indirect));
                    return 1;
                })
        .method("apply_changes",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    std::vector<std::int32_t> states(at.size() / 3);
                    if (!args.isTable(base + 1) || static_cast<std::size_t>(args.length(base + 1)) != states.size()) {
                        args.raise("apply_changes(positions, states, effects) needs one state per position");
                    }
                    for (std::size_t index = 0; index < states.size(); index++) {
                        args.pushIndex(base + 1, static_cast<std::int64_t>(index) + 1);
                        states[index] = static_cast<std::int32_t>(args.toInteger(-1));
                        args.pop(1);
                    }
                    args.pushInteger(world->applyChanges(at, states, effectsFrom(args, base + 2)));
                    return 1;
                })
        .method("read_states",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    std::vector<std::int32_t> out(at.size() / 3, -1);
                    world->readStates(at, out, args.isNil(base + 1) || args.booleanOr(base + 1, true));
                    pushInts(args, out);
                    return 1;
                })
        .method("write_states",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    std::vector<std::int32_t> states(at.size() / 3);
                    if (!args.isTable(base + 1) || static_cast<std::size_t>(args.length(base + 1)) != states.size()) {
                        args.raise("write_states(positions, states, notify) needs one state per position");
                    }
                    for (std::size_t index = 0; index < states.size(); index++) {
                        args.pushIndex(base + 1, static_cast<std::int64_t>(index) + 1);
                        states[index] = static_cast<std::int32_t>(args.toInteger(-1));
                        args.pop(1);
                    }
                    args.pushInteger(
                        world->writeStates(at, states, args.isNil(base + 2) || args.booleanOr(base + 2, true)));
                    return 1;
                })
        .method("resend_chunks",
                [world](LuaArgs& args) {
                    world->resendChunks(chunkPairs(args, firstArgument(args)));
                    return 0;
                })
        .method("light_at",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    const std::int32_t level = world->lightAt(spot.x, spot.y, spot.z, layerFrom(args, spot.next));
                    if (level < 0) {
                        args.pushNil();
                    } else {
                        args.pushInteger(level);
                    }
                    return 1;
                })
        .method("light_section",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::int32_t layer = layerFrom(args, base + 3);
                    if (args.isNil(base + 5) || args.booleanOr(base + 5, true)) {
                        static_cast<void>(world->lightAt(intAt(args, base) * 16, intAt(args, base + 1) * 16,
                                                         intAt(args, base + 2) * 16, layer));
                    }
                    std::vector<std::uint8_t> out(kLightSectionBytes);
                    if (!world->readLight(intAt(args, base), intAt(args, base + 1), intAt(args, base + 2), layer,
                                          args.isNil(base + 4) || args.booleanOr(base + 4, true), out)) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushString(std::string_view{reinterpret_cast<const char*>(out.data()), out.size()});
                    return 1;
                })
        .method(
            "set_light_section",
            [world](LuaArgs& args) {
                const std::int32_t base = firstArgument(args);
                const std::string bytes = args.checkString(base + 4);
                args.pushBoolean(world->writeLight(
                    intAt(args, base), intAt(args, base + 1), intAt(args, base + 2), layerFrom(args, base + 3),
                    std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}));
                return 1;
            })
        .method("check_light",
                [world](LuaArgs& args) {
                    world->checkLight(blockTriples(args, firstArgument(args)));
                    return 0;
                })
        .method("relight_chunks",
                [world](LuaArgs& args) {
                    world->relightChunks(chunkPairs(args, firstArgument(args)));
                    return 0;
                })
        .method("loaded_chunks",
                [world](LuaArgs& args) {
                    const std::vector<std::int32_t> flat = world->loadedChunks();
                    args.pushTable();
                    for (std::size_t pair = 0; pair + 1 < flat.size(); pair += 2) {
                        args.pushTable();
                        args.pushInteger(flat[pair]);
                        args.setIndex(1);
                        args.pushInteger(flat[pair + 1]);
                        args.setIndex(2);
                        args.setIndex(static_cast<std::int64_t>(pair / 2) + 1);
                    }
                    return 1;
                })
        .method("stored_chunk_nbt",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::optional<std::vector<std::uint8_t>> bytes =
                        world->storedChunkNbt(static_cast<std::int32_t>(args.checkInteger(base)),
                                              static_cast<std::int32_t>(args.checkInteger(base + 1)));
                    if (!bytes.has_value()) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushString(std::string_view{reinterpret_cast<const char*>(bytes->data()), bytes->size()});
                    return 1;
                })
        .method("has_stored_chunk",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(world->hasStoredChunk(static_cast<std::int32_t>(args.checkInteger(base)),
                                                           static_cast<std::int32_t>(args.checkInteger(base + 1))));
                    return 1;
                })
        .method("store_chunk_nbt",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::string raw = args.checkString(base + 2);
                    const std::span<const std::uint8_t> nbt{reinterpret_cast<const std::uint8_t*>(raw.data()),
                                                            raw.size()};
                    args.pushBoolean(world->storeChunkNbt(static_cast<std::int32_t>(args.checkInteger(base)),
                                                          static_cast<std::int32_t>(args.checkInteger(base + 1)), nbt));
                    return 1;
                })
        .method("stored_chunks", [world](LuaArgs& args) {
            const std::int32_t base = firstArgument(args);
            const std::vector<std::int32_t> flat =
                world->storedChunksIn(static_cast<std::int32_t>(args.checkInteger(base)),
                                      static_cast<std::int32_t>(args.checkInteger(base + 1)));
            args.pushTable();
            for (std::size_t pair = 0; pair + 1 < flat.size(); pair += 2) {
                args.pushTable();
                args.pushInteger(flat[pair]);
                args.setIndex(1);
                args.pushInteger(flat[pair + 1]);
                args.setIndex(2);
                args.setIndex(static_cast<std::int64_t>(pair / 2) + 1);
            }
            return 1;
        });
    addBulkBlockEntityMethods(object, world, hooks);
}
} // namespace basalt::server::plugins
