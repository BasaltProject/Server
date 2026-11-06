// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.material.rule.MaterialRule;
import org.basaltmc.bukkit.facade.Terrain;

public record NoiseGeneratorSettings(NoiseSettings noiseSettings, BlockState defaultBlock, BlockState defaultFluid, NoiseRouter noiseRouter, Holder<MaterialRule> materialRule, List<SpawnTargetPoint> spawnTarget, int seaLevel, boolean disableMobGeneration, Optional<Aquifer.Config> aquifers, boolean useLegacyRandomSource, DebugFunctions debugFunctions) {
    public static final Codec<NoiseGeneratorSettings> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.NoiseGeneratorSettings.class, "DIRECT_CODEC");
    public static final Codec<Holder<NoiseGeneratorSettings>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.NoiseGeneratorSettings.class, "CODEC");
    public static final ResourceKey<NoiseGeneratorSettings> OVERWORLD = key("overworld");
    public static final ResourceKey<NoiseGeneratorSettings> LARGE_BIOMES = key("large_biomes");
    public static final ResourceKey<NoiseGeneratorSettings> AMPLIFIED = key("amplified");
    public static final ResourceKey<NoiseGeneratorSettings> NETHER = key("nether");
    public static final ResourceKey<NoiseGeneratorSettings> END = key("end");
    public static final ResourceKey<NoiseGeneratorSettings> CAVES = key("caves");
    public static final ResourceKey<NoiseGeneratorSettings> FLOATING_ISLANDS = key("floating_islands");

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static ResourceKey<NoiseGeneratorSettings> key(String name) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.NOISE_SETTINGS, Identifier.withDefaultNamespace(name));
    }

    public boolean disableMobGeneration() {
        return disableMobGeneration;
    }

    public WorldgenRandom.Algorithm getRandomSource() {
        return useLegacyRandomSource ? WorldgenRandom.Algorithm.LEGACY : WorldgenRandom.Algorithm.XOROSHIRO;
    }

    public static void bootstrap(BootstrapContext<NoiseGeneratorSettings> context) {
        for (ResourceKey<NoiseGeneratorSettings> key : List.of(OVERWORLD, LARGE_BIOMES, AMPLIFIED, NETHER, END, CAVES, FLOATING_ISLANDS)) {
            NoiseGeneratorSettings built = Terrain.settings(key.identifier());
            if (built != null) {
                context.register(key, built);
            }
        }
    }

    public static record DebugFunctions(List<DebugFunctionEntry> functions) {
        public static final DebugFunctions EMPTY = new DebugFunctions(List.of());
        public static final Codec<DebugFunctions> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.NoiseGeneratorSettings.DebugFunctions.class, "CODEC");
    }

    public static record DebugFunctionEntry(String label, DensityFunction function) {
        public static final Codec<DebugFunctionEntry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.NoiseGeneratorSettings.DebugFunctionEntry.class, "CODEC");
    }
}
