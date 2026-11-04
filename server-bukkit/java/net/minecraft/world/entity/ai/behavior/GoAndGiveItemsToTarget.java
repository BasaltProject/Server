// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.Predicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.phys.Vec3;

public class GoAndGiveItemsToTarget<E extends LivingEntity> extends Behavior<E> {
    private static final int CLOSE_ENOUGH_DISTANCE_TO_TARGET = 3;
    private final Function<LivingEntity, Optional<PositionTracker>> targetPositionGetter;
    private final float speedModifier;
    private final ItemThrower<E> itemThrower;
    private final MemoryModuleType<Integer> cooldownMemory;
    private final int cooldownDuration;
    private final Predicate<E> hasItemPredicate;

    public GoAndGiveItemsToTarget(Function<LivingEntity, Optional<PositionTracker>> targetPositionGetter, float speedModifier, int timeoutDuration,
            ItemThrower<E> itemThrower, MemoryModuleType<Integer> cooldownMemory, int cooldownDuration, Predicate<E> hasItemPredicate) {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.WALK_TARGET, MemoryStatus.REGISTERED, cooldownMemory, MemoryStatus.REGISTERED), timeoutDuration);
        this.targetPositionGetter = targetPositionGetter;
        this.speedModifier = speedModifier;
        this.itemThrower = itemThrower;
        this.cooldownMemory = cooldownMemory;
        this.cooldownDuration = cooldownDuration;
        this.hasItemPredicate = hasItemPredicate;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, E body) {
        return canThrowItemToTarget(body);
    }

    protected boolean canStillUse(ServerLevel level, E body, long timestamp) {
        return canThrowItemToTarget(body);
    }

    protected void start(ServerLevel level, E body, long timestamp) {
        targetPositionGetter.apply(body).ifPresent(tracker -> BehaviorUtils.setWalkAndLookTargetMemories(body, tracker, speedModifier, CLOSE_ENOUGH_DISTANCE_TO_TARGET));
    }

    protected void tick(ServerLevel level, E body, long timestamp) {
        Optional<PositionTracker> target = targetPositionGetter.apply(body);
        if (target.isPresent()) {
            Vec3 position = target.get().currentPosition();
            if (position.distanceTo(body.getEyePosition()) < CLOSE_ENOUGH_DISTANCE_TO_TARGET) {
                itemThrower.throwItem(level, body, position);
                body.getBrain().setMemory(cooldownMemory, cooldownDuration);
            }
        }
    }

    private boolean canThrowItemToTarget(E body) {
        return hasItemPredicate.test(body) && targetPositionGetter.apply(body).isPresent();
    }

    @FunctionalInterface
    public interface ItemThrower<E> {
        void throwItem(ServerLevel level, E thrower, Vec3 targetPos);
    }
}
