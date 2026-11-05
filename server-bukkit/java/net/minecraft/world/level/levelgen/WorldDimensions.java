// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import java.util.Map;
import com.google.common.collect.ImmutableSet;
import com.mojang.serialization.Lifecycle;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.dimension.DimensionType;
import net.minecraft.world.level.dimension.LevelStem;
import net.minecraft.world.level.storage.PrimaryLevelData;
public record WorldDimensions(Map dimensions) {
    private static final Set<ResourceKey<LevelStem>> kBuiltinOrder = java.util.Collections.unmodifiableSet(
        new java.util.LinkedHashSet<>(java.util.List.of(LevelStem.OVERWORLD, LevelStem.NETHER, LevelStem.END)));
    public WorldDimensions {
        if (dimensions.get(LevelStem.OVERWORLD) == null) {
            throw new IllegalStateException("Overworld settings missing");
        }
    }
    public WorldDimensions(Registry<LevelStem> registry) {
        this(registry.listElements().collect(java.util.stream.Collectors.toMap(
            net.minecraft.core.Holder.Reference::key, net.minecraft.core.Holder.Reference::value)));
    }
    public static final MapCodec<WorldDimensions> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.WorldDimensions.class, "CODEC");
    public static Stream<ResourceKey<LevelStem>> keysInOrder(Set<ResourceKey<LevelStem>> knownKeys) {
        return Stream.concat(kBuiltinOrder.stream().filter(knownKeys::contains),
            knownKeys.stream().filter(key -> !kBuiltinOrder.contains(key)));
    }
    public WorldDimensions replaceOverworldGenerator(HolderLookup.Provider registries, ChunkGenerator generator) {
        return new WorldDimensions(withOverworld((HolderLookup<DimensionType>) (HolderLookup) registries.lookupOrThrow(
            net.minecraft.core.registries.Registries.DIMENSION_TYPE),
            dimensions, generator));
    }
    public static Map<ResourceKey<LevelStem>, LevelStem> withOverworld(HolderLookup<DimensionType> dimensionTypes, Map<ResourceKey<LevelStem>, LevelStem> dimensions, ChunkGenerator generator) {
        LevelStem stem = dimensions.get(LevelStem.OVERWORLD);
        Holder<DimensionType> type = stem == null
            ? dimensionTypes.getOrThrow(net.minecraft.world.level.dimension.BuiltinDimensionTypes.OVERWORLD) : stem.type();
        return withOverworld(dimensions, type, generator);
    }
    public static Map<ResourceKey<LevelStem>, LevelStem> withOverworld(Map<ResourceKey<LevelStem>, LevelStem> dimensions, Holder<DimensionType> type, ChunkGenerator generator) {
        Map<ResourceKey<LevelStem>, LevelStem> merged = new java.util.LinkedHashMap<>(dimensions);
        merged.put(LevelStem.OVERWORLD, new LevelStem(type, generator));
        return java.util.Collections.unmodifiableMap(merged);
    }
    public ChunkGenerator overworld() {
        LevelStem stem = (LevelStem) dimensions.get(LevelStem.OVERWORLD);
        if (stem == null) {
            throw new IllegalStateException("Overworld settings missing");
        }
        return stem.generator();
    }
    public Optional<LevelStem> get(ResourceKey<LevelStem> key) {
        return Optional.ofNullable((LevelStem) dimensions.get(key));
    }
    public ImmutableSet<ResourceKey<Level>> levels() {
        ImmutableSet.Builder<ResourceKey<Level>> levels = ImmutableSet.builder();
        for (Object key : dimensions.keySet()) {
            levels.add(net.minecraft.core.registries.Registries.levelStemToLevel((ResourceKey<LevelStem>) key));
        }
        return levels.build();
    }
    public boolean isDebug() {
        return overworld().getClass().getSimpleName().equals("DebugLevelSource");
    }
    public Complete bake(Registry<LevelStem> baseDimensions) {
        Set<ResourceKey<LevelStem>> known = new java.util.LinkedHashSet<>(baseDimensions.registryKeySet());
        known.addAll(dimensions.keySet());
        net.minecraft.core.MappedRegistry<LevelStem> baked = new net.minecraft.core.MappedRegistry<>(
            net.minecraft.core.registries.Registries.LEVEL_STEM,
            known.containsAll(kBuiltinOrder) ? Lifecycle.stable() : Lifecycle.experimental());
        keysInOrder(known).forEach(key -> {
            LevelStem stem = baseDimensions.getOptional(key).orElseGet(() -> (LevelStem) dimensions.get(key));
            if (stem != null) {
                baked.register(key, stem);
            }
        });
        PrimaryLevelData.SpecialWorldProperty special = PrimaryLevelData.SpecialWorldProperty.NONE;
        String generator = baked.getOptional(LevelStem.OVERWORLD).map(stem -> stem.generator().getClass().getSimpleName()).orElse("");
        if (generator.equals("DebugLevelSource")) {
            special = PrimaryLevelData.SpecialWorldProperty.DEBUG;
        } else if (generator.equals("FlatLevelSource")) {
            special = PrimaryLevelData.SpecialWorldProperty.FLAT;
        }
        return new Complete(baked, special);
    }
    public static record Complete(Registry<LevelStem> dimensions, PrimaryLevelData.SpecialWorldProperty specialWorldProperty) {
        public Lifecycle lifecycle() {
        return dimensions.registryLifecycle();
        }
        public RegistryAccess.Frozen dimensionsRegistryAccess() {
        return new RegistryAccess.ImmutableRegistryAccess(java.util.List.of(dimensions)).freeze();
        }
    }
}
