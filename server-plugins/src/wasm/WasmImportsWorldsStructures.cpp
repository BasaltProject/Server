#include "WasmWorlds.hpp"

#include <basalt/server/plugins/PluginStructures.hpp>
#include <basalt/server/plugins/PluginWorldgen.hpp>

#include <algorithm>

namespace basalt::server::plugins::wasm::worlds {
namespace {
constexpr std::int32_t kDisk = 32;
constexpr std::int32_t kEntities = 2048;

[[nodiscard]] bool has(std::int32_t flags, std::int32_t bit) { return (flags & bit) != 0; }

[[nodiscard]] PluginStructures& structures(State& state) { return state.host->services().structures(); }

[[nodiscard]] PluginWorldgen& worldgen(State& state) { return state.host->services().worldgen(); }

void structureList(void* user, abi::Call& c) {
    c.ret(putValue(c.memory(), c.i32(0), c.i32(1), textList(structures(stateOf(user)).ids())));
}

void structureGet(void* user, abi::Call& c) {
    const std::optional<std::string_view> id = c.memory().string(c.i32(0), c.i32(1));
    if (!id) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::optional<std::vector<std::uint8_t>> nbt = structures(stateOf(user)).read(*id);
    c.ret(nbt ? putBytes(c.memory(), c.i32(2), c.i32(3), *nbt) : abi::kErrAbsent);
}

void structurePut(void* user, abi::Call& c) {
    const std::optional<std::string_view> id = c.memory().string(c.i32(0), c.i32(1));
    const std::optional<std::vector<std::uint8_t>> nbt = readBytes(c.memory(), c.i32(2), c.i32(3));
    if (!id || id->empty() || !nbt) {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(structures(stateOf(user)).store(*id, *nbt, has(c.i32(4), kDisk)) ? abi::kOk : abi::kErrInvalid);
}

void structureRemove(void* user, abi::Call& c) {
    const std::optional<std::string_view> id = c.memory().string(c.i32(0), c.i32(1));
    if (!id) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginStructures& all = structures(stateOf(user));
    const std::vector<std::string> ids = all.ids();
    if (std::ranges::find(ids, *id) == ids.end()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(all.remove(*id, has(c.i32(2), kDisk)) ? abi::kOk : abi::kErrAbsent);
}

void structureFile(void* user, abi::Call& c) {
    const std::optional<std::string_view> id = c.memory().string(c.i32(0), c.i32(1));
    if (!id) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginStructures& all = structures(stateOf(user));
    const std::string path = all.file(*id);
    if (!path.empty()) {
        c.ret(c.memory().put(c.i32(2), c.i32(3), path));
        return;
    }
    const std::vector<std::string> ids = all.ids();
    c.ret(std::ranges::find(ids, *id) != ids.end() ? c.memory().put(c.i32(2), c.i32(3), *id) : abi::kErrAbsent);
}

void structureCapture(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::optional<std::vector<std::uint8_t>> nbt = structures(state).capture(
            world.name(), c.i32(1), c.i32(2), c.i32(3), c.i32(4), c.i32(5), c.i32(6), has(c.i32(7), kEntities));
        return nbt ? putBytes(c.memory(), c.i32(8), c.i32(9), *nbt) : abi::kErrAbsent;
    });
}

void structureScan(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::vector<std::uint8_t>> nbt = readBytes(c.memory(), c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = mapArgument(c.memory(), c.i32(2), c.i32(3));
    if (!nbt || !spec) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginTemplateScanRequest request;
    request.x = static_cast<std::int32_t>(intField(*spec, "x"));
    request.y = static_cast<std::int32_t>(intField(*spec, "y"));
    request.z = static_cast<std::int32_t>(intField(*spec, "z"));
    request.rotation = static_cast<std::int32_t>(intField(*spec, "rotation"));
    request.mirror = static_cast<std::int32_t>(intField(*spec, "mirror"));
    request.pivotX = static_cast<std::int32_t>(intField(*spec, "pivot_x"));
    request.pivotZ = static_cast<std::int32_t>(intField(*spec, "pivot_z"));
    request.block = textField(*spec, "block");
    request.absolute = boolField(*spec, "absolute", true);
    const std::optional<PluginTemplateScan> scan = structures(state).scan(*nbt, request);
    if (!scan) {
        c.ret(abi::kErrAbsent);
        return;
    }
    Value::List blocks;
    for (const PluginTemplateBlock& block : scan->blocks) {
        Value::Map row;
        put(row, "x", Value{block.x});
        put(row, "y", Value{block.y});
        put(row, "z", Value{block.z});
        put(row, "state", Value{block.stateId});
        put(row, "nbt", bytesValue(block.nbt));
        blocks.emplace_back(std::move(row));
    }
    Value::List jigsaws;
    for (const PluginTemplateJigsaw& jigsaw : scan->jigsaws) {
        Value::Map row;
        put(row, "x", Value{jigsaw.x});
        put(row, "y", Value{jigsaw.y});
        put(row, "z", Value{jigsaw.z});
        put(row, "front", Value{jigsaw.front});
        put(row, "top", Value{jigsaw.top});
        put(row, "rollable", Value{jigsaw.rollable});
        put(row, "name", Value{jigsaw.name});
        put(row, "pool", Value{jigsaw.pool});
        put(row, "target", Value{jigsaw.target});
        put(row, "final_state", Value{jigsaw.finalState});
        put(row, "placement_priority", Value{jigsaw.placementPriority});
        put(row, "selection_priority", Value{jigsaw.selectionPriority});
        jigsaws.emplace_back(std::move(row));
    }
    Value::Map result;
    put(result, "size", intList(std::array<std::int32_t, 3>{scan->sizeX, scan->sizeY, scan->sizeZ}));
    put(result, "blocks", Value{std::move(blocks)});
    put(result, "jigsaws", Value{std::move(jigsaws)});
    c.ret(putValue(c.memory(), c.i32(4), c.i32(5), Value{std::move(result)}));
}

void structurePlace(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::vector<std::uint8_t>> nbt = readBytes(c.memory(), c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = mapArgument(c.memory(), c.i32(2), c.i32(3));
    if (!nbt || !spec) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginStructurePlacement placement;
    placement.world = textField(*spec, "world");
    placement.x = static_cast<std::int32_t>(intField(*spec, "x"));
    placement.y = static_cast<std::int32_t>(intField(*spec, "y"));
    placement.z = static_cast<std::int32_t>(intField(*spec, "z"));
    placement.rotation = static_cast<std::int32_t>(intField(*spec, "rotation"));
    placement.mirror = static_cast<std::int32_t>(intField(*spec, "mirror"));
    placement.integrity = static_cast<float>(realField(*spec, "integrity", 1.0));
    placement.seed = intField(*spec, "seed");
    placement.entities = boolField(*spec, "entities", true);
    placement.palette = static_cast<std::int32_t>(intField(*spec, "palette", -1));
    if (placement.world.empty() || placement.rotation < 0 || placement.rotation > 3 || placement.mirror < 0 ||
        placement.mirror > 2 || state.host->services().worlds().byName(placement.world) == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(structures(state).place(*nbt, placement) ? abi::kOk : abi::kErrAbsent);
}

[[nodiscard]] std::optional<Value> number(const std::optional<double>& value) {
    return value ? std::optional<Value>{Value{*value}} : std::nullopt;
}

[[nodiscard]] std::optional<Value> evaluate(PluginWorldgen& gen, const Value& spec) {
    const std::string op = textField(spec, "op");
    const std::string settings = textField(spec, "settings", "minecraft:overworld");
    const std::int64_t seed = intField(spec, "seed");
    const auto x = static_cast<std::int32_t>(intField(spec, "x"));
    const auto y = static_cast<std::int32_t>(intField(spec, "y"));
    const auto z = static_cast<std::int32_t>(intField(spec, "z"));
    if (op == "shape") {
        const std::optional<PluginTerrainShape> shape = gen.shape(settings);
        if (!shape) {
            return std::nullopt;
        }
        Value::Map row;
        put(row, "min_y", Value{shape->minY});
        put(row, "height", Value{shape->height});
        put(row, "sea_level", Value{shape->seaLevel});
        put(row, "default_block", Value{shape->defaultBlock});
        put(row, "default_fluid", Value{shape->defaultFluid});
        put(row, "legacy_random_source", Value{shape->legacyRandomSource});
        put(row, "disable_mob_generation", Value{shape->disableMobGeneration});
        return Value{std::move(row)};
    }
    if (op == "density") {
        return number(gen.density(settings, seed, textField(spec, "function"), x, y, z));
    }
    if (op == "density_tree") {
        return number(gen.densityTree(settings, seed, textField(spec, "json"), x, y, z));
    }
    if (op == "noise") {
        return number(gen.noise(settings, seed, textField(spec, "id"), realField(spec, "x"), realField(spec, "y"),
                                realField(spec, "z")));
    }
    if (op == "noise_range") {
        const auto range = gen.noiseRange(settings, seed, textField(spec, "id"));
        if (!range) {
            return std::nullopt;
        }
        return Value{Value::List{Value{range->first}, Value{range->second}}};
    }
    if (op == "height") {
        const std::optional<std::int32_t> height = gen.baseHeight(settings, seed, x, z, boolField(spec, "ocean_floor"));
        return height ? std::optional<Value>{Value{*height}} : std::nullopt;
    }
    if (op == "column") {
        const std::vector<std::int32_t> column = gen.baseColumn(settings, seed, x, z);
        if (column.empty()) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> bytes(column.size() * 4);
        std::memcpy(bytes.data(), column.data(), bytes.size());
        return bytesValue(bytes);
    }
    return std::nullopt;
}

void noiseQuery(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::expected<Value, std::int32_t> spec = mapArgument(c.memory(), c.i32(0), c.i32(1));
    if (!spec) {
        c.ret(spec.error());
        return;
    }
    const std::optional<Value> answer = evaluate(worldgen(state), *spec);
    c.ret(answer ? putValue(c.memory(), c.i32(2), c.i32(3), *answer) : abi::kErrAbsent);
}

[[nodiscard]] PluginNoiseSeed seedOf(const Value& spec) {
    PluginNoiseSeed seed;
    if (const Value* found = spec.find("seed"); found != nullptr && found->asMap() != nullptr) {
        seed.legacyRandom = boolField(*found, "legacy");
        seed.low = intField(*found, "low");
        seed.high = intField(*found, "high");
        seed.legacyNether = boolField(*found, "nether");
    }
    return seed;
}

void noiseCreate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::expected<Value, std::int32_t> spec = mapArgument(c.memory(), c.i32(0), c.i32(1));
    if (!spec) {
        c.ret(spec.error());
        return;
    }
    std::optional<std::int64_t> made;
    if (hasField(*spec, "id")) {
        made = worldgen(state).createNoise(textField(*spec, "id"), seedOf(*spec));
    } else if (const Value* parameters = spec->find("parameters"); parameters != nullptr) {
        PluginNoiseParameters built;
        built.baseAmplitude = realField(*parameters, "base_amplitude", 1.0);
        built.baseOctave = static_cast<std::int32_t>(intField(*parameters, "base_octave"));
        built.octaveCount = static_cast<std::int32_t>(intField(*parameters, "octave_count", 1));
        built.normalize = static_cast<std::int32_t>(intField(*parameters, "normalize", 1));
        if (const Value* list = parameters->find("amplitudes"); list != nullptr && list->asList() != nullptr) {
            for (const Value& item : *list->asList()) {
                const double* real = item.asFloat();
                const std::int64_t* whole = item.asInt();
                built.amplitudeModifiers.push_back(real != nullptr ? *real : whole != nullptr ? double(*whole) : 0.0);
            }
        }
        made = worldgen(state).createNoise(built, seedOf(*spec));
    } else {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (!made) {
        c.ret(abi::kErrAbsent);
        return;
    }
    Registry& all = registry(state);
    all.worldgen = &worldgen(state);
    const std::int32_t handle = all.noises.add(*made);
    if (handle == 0) {
        worldgen(state).releaseNoise(*made);
    }
    c.ret(handle == 0 ? abi::kErrInvalid : handle);
}

void noiseSample(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int64_t* handle = nullptr;
    const std::int32_t status = registry(state).noises.find(c.i32(0), handle);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<double> value = worldgen(state).sampleNoise(*handle, c.f64(1), c.f64(2), c.f64(3));
    c.ret(!value ? abi::kErrAbsent : c.memory().write(c.i32(4), *value) ? abi::kOk : abi::kErrInvalid);
}

void noiseInfo(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int64_t* handle = nullptr;
    const std::int32_t status = registry(state).noises.find(c.i32(0), handle);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const auto range = worldgen(state).handleRange(*handle);
    c.ret(!range ? abi::kErrAbsent
                 : putValue(c.memory(), c.i32(1), c.i32(2),
                            Value{Value::List{Value{range->first}, Value{range->second}}}));
}

void noiseRelease(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int64_t* handle = nullptr;
    const std::int32_t status = registry(state).noises.find(c.i32(0), handle);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    worldgen(state).releaseNoise(*handle);
    static_cast<void>(registry(state).noises.remove(c.i32(0)));
    c.ret(abi::kOk);
}
} // namespace

Registry::~Registry() {
    if (worldgen != nullptr) {
        noises.forEach([this](std::int32_t, std::int64_t handle) { worldgen->releaseNoise(handle); });
    }
}
} // namespace basalt::server::plugins::wasm::worlds

namespace basalt::server::plugins::wasm {
void addWorldsStructuresImports(State& state, std::vector<abi::HostImport>& out) {
    using namespace worlds;
    void* user = &state;
    out.insert(out.end(), {
                              {"structure_list", structureList, user},
                              {"structure_get", structureGet, user},
                              {"structure_put", structurePut, user},
                              {"structure_remove", structureRemove, user},
                              {"structure_file", structureFile, user},
                              {"structure_capture", structureCapture, user},
                              {"structure_scan", structureScan, user},
                              {"structure_place", structurePlace, user},
                              {"noise_query", noiseQuery, user},
                              {"noise_create", noiseCreate, user},
                              {"noise_sample", noiseSample, user},
                              {"noise_info", noiseInfo, user},
                              {"noise_release", noiseRelease, user},
                          });
}
} // namespace basalt::server::plugins::wasm
