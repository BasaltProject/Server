// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;

import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.Level;

public record RegionStorageInfo(String level, ResourceKey<Level> dimension, String type) {
    public RegionStorageInfo withTypeSuffix(String suffix) {
        return new RegionStorageInfo(level, dimension, type + suffix);
    }
}
