// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import net.minecraft.world.level.levelgen.WorldDimensions;
import net.minecraft.world.level.levelgen.WorldGenSettings;

public record LevelDataAndDimensions(WorldDataAndGenSettings worldDataAndGenSettings, WorldDimensions.Complete dimensions) {
    public static LevelDataAndDimensions create(WorldData data, WorldGenSettings genSettings, WorldDimensions.Complete dimensions) {
        return new LevelDataAndDimensions(new WorldDataAndGenSettings(data, genSettings), dimensions);
    }

    public static record WorldDataAndGenSettings(WorldData data, WorldGenSettings genSettings) {
    }
}
