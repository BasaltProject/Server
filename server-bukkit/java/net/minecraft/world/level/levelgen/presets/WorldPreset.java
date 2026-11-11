// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.presets;

import com.mojang.serialization.Codec;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.dimension.LevelStem;
import net.minecraft.world.level.levelgen.WorldDimensions;

public class WorldPreset {
    public static final Codec<WorldPreset> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(WorldPreset.class, "DIRECT_CODEC");
    public static final Codec<Holder<WorldPreset>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(WorldPreset.class, "CODEC");
    private final Map<ResourceKey<LevelStem>, LevelStem> dimensions;

    public WorldPreset(Map<ResourceKey<LevelStem>, LevelStem> dimensions) {
        this.dimensions = dimensions;
    }

    public WorldDimensions createWorldDimensions() {
        Map<ResourceKey<LevelStem>, LevelStem> ordered = new LinkedHashMap<>();
        WorldDimensions.keysInOrder(dimensions.keySet()).forEach(key -> {
            LevelStem stem = dimensions.get(key);
            if (stem != null) {
                ordered.put(key, stem);
            }
        });
        return new WorldDimensions(Map.copyOf(ordered));
    }

    public Optional<LevelStem> overworld() {
        return Optional.ofNullable(dimensions.get(LevelStem.OVERWORLD));
    }
}
