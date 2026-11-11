// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
public interface SpawnPlacementType {
    boolean isSpawnPositionOk(LevelReader level, BlockPos blockPos, EntityType<?> type);
    default BlockPos adjustSpawnPosition(LevelReader level, BlockPos candidate) {
        return candidate;
    }
}
