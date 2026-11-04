package org.basaltmc.bukkit.facade;

import com.mojang.datafixers.DSL;
import com.mojang.datafixers.DataFixer;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import org.basaltmc.bukkit.Registries;
import org.basaltmc.bukkit.WorldNatives;

public final class ServerAccess {
    private static final Map<Identifier, Registry<?>> kRegistries = new ConcurrentHashMap<>();
    private static final RegistryAccess.Frozen kAccess = new Access();
    private static final DataFixer kFixer = new NativeFixer(new IdentityFixer());
    private static final Map<String, String> kRenamedItems = Map.of("minecraft:grass", "minecraft:short_grass",
        "minecraft:grass_path", "minecraft:dirt_path", "minecraft:sign", "minecraft:oak_sign",
        "minecraft:rose_red", "minecraft:red_dye", "minecraft:cactus_green", "minecraft:green_dye",
        "minecraft:dandelion_yellow", "minecraft:yellow_dye");
    private static final Map<String, String> kRenamedBlocks = Map.of("minecraft:grass", "minecraft:short_grass",
        "minecraft:grass_path", "minecraft:dirt_path", "minecraft:sign", "minecraft:oak_sign",
        "minecraft:wall_sign", "minecraft:oak_wall_sign");

    private ServerAccess() {}

    public static RegistryAccess.Frozen registries() {
        return kAccess;
    }

    public static DataFixer dataFixer() {
        return kFixer;
    }

    @SuppressWarnings("unchecked")
    private static <E> Registry<E> registry(ResourceKey<? extends Registry<? extends E>> key) {
        return (Registry<E>) org.basaltmc.bukkit.facade.Memo.get(kRegistries, key.identifier(), id -> build(key));
    }

    private static Registry<?> build(ResourceKey<? extends Registry<?>> key) {
        switch (key.identifier().getPath()) {
            case "block":
                return BuiltInRegistries.BLOCK;
            case "item":
                return BuiltInRegistries.ITEM;
            case "fluid":
                return BuiltInRegistries.FLUID;
            case "entity_type":
                return BuiltInRegistries.ENTITY_TYPE;
            case "block_entity_type":
                return BuiltInRegistries.BLOCK_ENTITY_TYPE;
            case "sound_event":
                return BuiltInRegistries.SOUND_EVENT;
            case "data_component_type":
                return BuiltInRegistries.DATA_COMPONENT_TYPE;
            case "mob_effect":
                return BuiltInRegistries.MOB_EFFECT;
            case "stat_type":
                return BuiltInRegistries.STAT_TYPE;
            case "enchantment":
                return enchantments(key);
            case "biome":
                return biomes();
            case "dimension_type":
                return dimensionTypes();
            case "worldgen/configured_feature":
                return features(key, false);
            case "worldgen/placed_feature":
                return features(key, true);
            default:
                return new MappedRegistry<>(castKey(key));
        }
    }

    @SuppressWarnings("unchecked")
    private static ResourceKey<? extends Registry<Object>> castKey(ResourceKey<? extends Registry<?>> key) {
        return (ResourceKey<? extends Registry<Object>>) key;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static MappedRegistry<?> features(ResourceKey<? extends Registry<?>> key, boolean placed) {
        MappedRegistry registry = new MappedRegistry<>(castKey(key));
        for (Identifier id : NativeFeatures.ids()) {
            registry.register(id, placed ? NativeFeatures.placed(id) : new NativeFeature(id));
        }
        return registry;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static MappedRegistry<?> enchantments(ResourceKey<? extends Registry<?>> key) {
        MappedRegistry registry = new MappedRegistry<>(castKey(key));
        for (String id : org.basaltmc.bukkit.InventoryNatives.enchantmentIds()) {
            Identifier parsed = Identifier.parse(id);
            registry.register(parsed, EnchantmentFactory.build(parsed));
        }
        return registry;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static MappedRegistry<?> dimensionTypes() {
        MappedRegistry registry = new MappedRegistry<>(net.minecraft.core.registries.Registries.DIMENSION_TYPE);
        registry.register(Identifier.withDefaultNamespace("overworld"), dimensionType(false, true, false, false, 1.0, -64, 384, 384, 0.0F));
        registry.register(Identifier.withDefaultNamespace("overworld_caves"), dimensionType(false, true, true, false, 1.0, -64, 384, 384, 0.0F));
        registry.register(Identifier.withDefaultNamespace("the_nether"), dimensionType(true, false, true, false, 8.0, 0, 256, 128, 0.1F));
        registry.register(Identifier.withDefaultNamespace("the_end"), dimensionType(true, false, false, true, 1.0, 0, 256, 256, 0.0F));
        return registry;
    }

    private static net.minecraft.world.level.dimension.DimensionType dimensionType(boolean fixedTime, boolean skyLight,
            boolean ceiling, boolean dragon, double scale, int minY, int height, int logicalHeight, float ambient) {
        return new net.minecraft.world.level.dimension.DimensionType(fixedTime, skyLight, ceiling, dragon, scale, minY, height,
            logicalHeight, null, ambient, null, null, null, null, null, Optional.empty());
    }

    private static MappedRegistry<Biome> biomes() {
        MappedRegistry<Biome> registry = new MappedRegistry<>(net.minecraft.core.registries.Registries.BIOME);
        for (String id : Registries.biomeIds()) {
            Identifier key = Identifier.parse(id);
            registry.register(key, new Biome(key));
        }
        return registry;
    }

    private static final class Access implements RegistryAccess.Frozen {
        @Override
        public <E> Optional<Registry<E>> lookup(ResourceKey<? extends Registry<? extends E>> key) {
            return Optional.of(registry(key));
        }
    }

    /** Runs the server's own data fixes, and keeps the renames the legacy block and item mapping needs after them. */
    private static final class NativeFixer implements DataFixer {
        private final DataFixer fallback;

        NativeFixer(DataFixer fallback) {
            this.fallback = fallback;
        }

        @Override
        public <T> Dynamic<T> update(DSL.TypeReference type, Dynamic<T> input, int version, int newVersion) {
            if (version >= WorldNatives.dataVersion()) {
                return input;
            }
            Dynamic<T> fixed = nativeUpdate(type.typeName(), input, version);
            return fallback.update(type, fixed == null ? input : fixed, version, newVersion);
        }

        private static <T> Dynamic<T> nativeUpdate(String kind, Dynamic<T> input, int version) {
            switch (kind) {
                case "block_state": {
                    String text = stateText(input);
                    String fixed = WorldNatives.fixName("block_state", text, version);
                    return fixed == null ? null : stateDynamic(input, fixed);
                }
                case "item_name":
                case "biome": {
                    String fixed = WorldNatives.fixName(kind.equals("biome") ? "biome" : "item", input.asString(""), version);
                    return fixed == null ? null : input.createString(fixed);
                }
                case "chunk":
                case "block_entity":
                case "entity":
                case "item_stack": {
                    Tag tag = input.convert(NbtOps.INSTANCE).getValue();
                    if (!(tag instanceof CompoundTag compound)) {
                        return null;
                    }
                    byte[] fixed = WorldNatives.fixNbt(kind.equals("item_stack") ? "item" : kind, NbtCodec.toBytes(compound), version);
                    CompoundTag result = NbtCodec.fromBytes(fixed);
                    return result == null ? null : new Dynamic<>(NbtOps.INSTANCE, result).convert(input.getOps());
                }
                default:
                    return null;
            }
        }

        private static <T> String stateText(Dynamic<T> state) {
            boolean old = state.get("id").result().isEmpty();
            StringBuilder text = new StringBuilder(state.get(old ? "Name" : "id").asString(""));
            Map<Dynamic<T>, Dynamic<T>> properties = state.get(old ? "Properties" : "properties").asMap(d -> d, d -> d);
            if (!properties.isEmpty()) {
                text.append('[');
                boolean first = true;
                for (Map.Entry<Dynamic<T>, Dynamic<T>> entry : properties.entrySet()) {
                    text.append(first ? "" : ",").append(entry.getKey().asString("")).append('=')
                        .append(entry.getValue().asString("").replace("\"", ""));
                    first = false;
                }
                text.append(']');
            }
            return text.toString();
        }

        private static <T> Dynamic<T> stateDynamic(Dynamic<T> like, String text) {
            int open = text.indexOf('[');
            Dynamic<T> state = like.emptyMap().set("id", like.createString(open < 0 ? text : text.substring(0, open)));
            if (open >= 0) {
                Dynamic<T> properties = like.emptyMap();
                for (String pair : text.substring(open + 1, text.length() - 1).split(",")) {
                    int equals = pair.indexOf('=');
                    if (equals > 0) {
                        properties = properties.set(pair.substring(0, equals), like.createString(pair.substring(equals + 1)));
                    }
                }
                state = state.set("properties", properties);
            }
            return state;
        }

        @Override
        public Schema getSchema(int key) {
            throw new UnsupportedOperationException("The Basalt data fixer has no schemas");
        }
    }

    private static final class IdentityFixer implements DataFixer {
        @Override
        public <T> Dynamic<T> update(DSL.TypeReference type, Dynamic<T> input, int version, int newVersion) {
            if (type.typeName().equals("item_name")) {
                String renamed = kRenamedItems.get(input.asString(""));
                return renamed == null ? input : input.createString(renamed);
            }
            if (!type.typeName().equals("block_state")) {
                return input;
            }
            Dynamic<T> state = input;
            if (version < 5006 && newVersion >= 5006) {
                state = state.renameField("Name", "id").renameField("Properties", "properties");
            }
            String id = state.get("id").asString("");
            String renamed = kRenamedBlocks.get(id);
            if (renamed != null && version < 5006) {
                state = state.set("id", state.createString(renamed));
            }
            if (id.endsWith("_wall") && version < 2500) {
                for (String side : new String[] {"north", "south", "east", "west"}) {
                    state = state.update("properties", properties -> properties.update(side, value -> {
                        String text = value.asString("");
                        return text.equals("true") ? value.createString("low")
                            : text.equals("false") ? value.createString("none") : value;
                    }));
                }
            }
            return state;
        }

        @Override
        public Schema getSchema(int key) {
            throw new UnsupportedOperationException("The Basalt data fixer has no schemas");
        }
    }
}
