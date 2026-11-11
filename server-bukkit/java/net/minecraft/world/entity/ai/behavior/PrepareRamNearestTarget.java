// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Map;
import java.util.function.Function;
import java.util.function.ToIntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
public class PrepareRamNearestTarget<E extends PathfinderMob> extends Behavior<E> {
    public static final int TIME_OUT_DURATION = 160;
    public PrepareRamNearestTarget(ToIntFunction<E> getCooldownOnFail, int minRamDistance, int maxRamDistance, float walkSpeed, TargetingConditions ramTargeting, int ramPrepareTime, Function<E, SoundEvent> getPrepareRamSound) {
        super((java.util.Map) null);
    }
    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        
    }
    protected void stop(ServerLevel level, E body, long timestamp) {
        
    }
    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return false;
    }
    protected void tick(ServerLevel level, E body, long timestamp) {
        
    }
    public static class RamCandidate {
        private final BlockPos startPosition;
        private final BlockPos targetPosition;
        private final LivingEntity target;
        public RamCandidate(BlockPos startPosition, BlockPos targetPosition, LivingEntity target) {
            this.startPosition = startPosition;
            this.targetPosition = targetPosition;
            this.target = target;
            
        }
        public BlockPos getStartPosition() {
            return startPosition;
        }
        public BlockPos getTargetPosition() {
            return targetPosition;
        }
        public LivingEntity getTarget() {
            return target;

        }
    }
}
