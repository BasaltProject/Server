// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.level.block.state.BlockState;
public class PathfindingContext {
    private final CollisionGetter level;
    private final BlockPos mobPosition;
    public PathfindingContext(CollisionGetter level, Mob mob) {
        this.level = level;
        this.mobPosition = mob.blockPosition();
    }
    public PathType getPathTypeFromState(int x, int y, int z) {
        return WalkNodeEvaluator.getPathTypeFromState(level, new BlockPos(x, y, z));
    }
    public BlockState getBlockState(BlockPos pos) {
        return level.getBlockState(pos);
    }
    public CollisionGetter level() {
        return level;
    }
    public BlockPos mobPosition() {
        return mobPosition;
    }
}
