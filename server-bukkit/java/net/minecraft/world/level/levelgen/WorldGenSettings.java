// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.world.level.saveddata.SavedData;
import com.mojang.serialization.Codec;
import net.minecraft.core.RegistryAccess;
import net.minecraft.world.level.saveddata.SavedDataType;
public class WorldGenSettings extends SavedData {
    private final WorldOptions options;
    private final WorldDimensions dimensions;
    public WorldGenSettings(WorldOptions worldOptions, WorldDimensions worldDimensions) {
        this.options = worldOptions;
        this.dimensions = worldDimensions;
    }
    public WorldDimensions dimensions() {
        return dimensions;
    }
    public WorldOptions options() {
        return options;
    }
    public static final Codec<WorldGenSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.WorldGenSettings.class, "CODEC");
    public static final SavedDataType<WorldGenSettings> TYPE = null;
    public static WorldGenSettings of(WorldOptions options, RegistryAccess registryAccess) {
        return new WorldGenSettings(options, new WorldDimensions(registryAccess.lookupOrThrow(net.minecraft.core.registries.Registries.LEVEL_STEM)));
    }
    public int hashCode() {
        return java.util.Objects.hash(options, dimensions);
    }
    public String toString() {
        return "WorldGenSettings[options=" + options + ", dimensions=" + dimensions + "]";
    }
}
