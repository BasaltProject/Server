package net.minecraft.world.level.levelgen;

import net.minecraft.world.level.saveddata.SavedData;
import org.basaltmc.bukkit.Unsupported;
public class WorldGenSettings extends SavedData {
    public WorldGenSettings(WorldOptions worldOptions, WorldDimensions worldDimensions) {
    }
    public WorldDimensions dimensions() {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.WorldGenSettings.dimensions");
    }
    public WorldOptions options() {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.WorldGenSettings.options");
    }
}
