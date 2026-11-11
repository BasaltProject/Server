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
    public static final Codec<WorldGenSettings> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
        WorldOptions.CODEC.forGetter(WorldGenSettings::options), WorldDimensions.CODEC.forGetter(WorldGenSettings::dimensions)).apply(i, i.stable(WorldGenSettings::new)));
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static final SavedDataType<WorldGenSettings> TYPE = new SavedDataType<>(net.minecraft.resources.Identifier.withDefaultNamespace("world_gen_settings"),
        () -> new WorldGenSettings(WorldOptions.defaultWithRandomSeed(), new WorldDimensions(new java.util.HashMap())), CODEC,
        net.minecraft.util.datafix.DataFixTypes.SAVED_DATA_WORLD_GEN_SETTINGS);
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
