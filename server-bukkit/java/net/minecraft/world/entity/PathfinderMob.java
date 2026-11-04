// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
public abstract class PathfinderMob extends Mob {
    protected static final float DEFAULT_WALK_TARGET_VALUE = 0.0F;
    protected PathfinderMob(EntityType<? extends PathfinderMob> type, Level level) {
        super(type, level);
    }
    public float getWalkTargetValue(BlockPos pos) {
        return 0.0F;
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return 0.0F;
    }
    public boolean checkSpawnRules(LevelAccessor level, EntitySpawnReason spawnReason) {
        return true;
    }
    public boolean isPathFinding() {
        return !getNavigation().isDone();
    }
    public boolean isPanicking() {
        return brain != null && getBrain().hasMemoryValue(net.minecraft.world.entity.ai.memory.MemoryModuleType.IS_PANICKING);
    }
    protected boolean shouldStayCloseToLeashHolder() {
        return true;
    }
    public void closeRangeLeashBehaviour(Entity leashHolder) {
        return;
    }
    public void whenLeashedTo(Entity leashHolder) {
        return;
    }
    protected double followLeashSpeed() {
        return 1.0;
    }
}
