// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.frog;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.behavior.Behavior;
import net.minecraft.world.entity.ai.behavior.BehaviorUtils;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.level.pathfinder.Path;

public class ShootTongue extends Behavior<Frog> {
    public static final int TIME_OUT_DURATION = 100;
    public static final int CATCH_ANIMATION_DURATION = 6;
    public static final int TONGUE_ANIMATION_DURATION = 10;
    private static final float EATING_DISTANCE = 1.75F;
    private static final float EATING_MOVEMENT_FACTOR = 0.75F;
    public static final int UNREACHABLE_TONGUE_TARGETS_COOLDOWN_DURATION = 100;
    public static final int MAX_UNREACHBLE_TONGUE_TARGETS_IN_MEMORY = 5;
    private int eatAnimationTimer;
    private int calculatePathCounter;
    private final SoundEvent tongueSound;
    private final SoundEvent eatSound;
    private State state = State.DONE;

    public ShootTongue(SoundEvent tongueSound, SoundEvent eatSound) {
        super(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT, MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED,
            MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT, MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT), TIME_OUT_DURATION);
        this.tongueSound = tongueSound;
        this.eatSound = eatSound;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Frog body) {
        LivingEntity target = body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).get();
        boolean reachable = canPathfindToTarget(body, target);
        if (!reachable) {
            body.getBrain().eraseMemory(MemoryModuleType.ATTACK_TARGET);
            addUnreachableTargetToMemory(body, target);
        }
        return reachable && body.getPose() != Pose.CROAKING && Frog.canEat(target);
    }

    protected boolean canStillUse(ServerLevel level, Frog body, long timestamp) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.ATTACK_TARGET) && state != State.DONE && !body.getBrain().hasMemoryValue(MemoryModuleType.IS_PANICKING);
    }

    protected void start(ServerLevel level, Frog body, long timestamp) {
        LivingEntity target = body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).get();
        BehaviorUtils.lookAtEntity(body, target);
        body.setTongueTarget(target);
        body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(target.position(), 2.0F, 0));
        calculatePathCounter = 10;
        state = State.MOVE_TO_TARGET;
    }

    protected void stop(ServerLevel level, Frog body, long timestamp) {
        body.getBrain().eraseMemory(MemoryModuleType.ATTACK_TARGET);
        body.eraseTongueTarget();
        body.setPose(Pose.STANDING);
    }

    private void eatEntity(ServerLevel level, Frog body) {
        level.playSound(null, body, eatSound, SoundSource.NEUTRAL, 2.0F, 1.0F);
        Optional<Entity> tongueTarget = body.getTongueTarget();
        if (tongueTarget.isPresent()) {
            Entity target = tongueTarget.get();
            if (target.isAlive()) {
                body.doHurtTarget(level, target);
                if (!target.isAlive()) {
                    target.remove(Entity.RemovalReason.KILLED);
                }
            }
        }
    }

    protected void tick(ServerLevel level, Frog body, long timestamp) {
        LivingEntity target = body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).get();
        body.setTongueTarget(target);
        switch (state) {
            case MOVE_TO_TARGET:
                if (target.distanceTo(body) < EATING_DISTANCE) {
                    level.playSound(null, body, tongueSound, SoundSource.NEUTRAL, 2.0F, 1.0F);
                    body.setPose(Pose.USING_TONGUE);
                    target.setDeltaMovement(target.position().vectorTo(body.position()).normalize().scale(EATING_MOVEMENT_FACTOR));
                    eatAnimationTimer = 0;
                    state = State.CATCH_ANIMATION;
                } else if (calculatePathCounter <= 0) {
                    body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(target.position(), 2.0F, 0));
                    calculatePathCounter = 10;
                } else {
                    --calculatePathCounter;
                }
                break;
            case CATCH_ANIMATION:
                if (eatAnimationTimer++ >= CATCH_ANIMATION_DURATION) {
                    state = State.EAT_ANIMATION;
                    eatEntity(level, body);
                }
                break;
            case EAT_ANIMATION:
                if (eatAnimationTimer >= TONGUE_ANIMATION_DURATION) {
                    state = State.DONE;
                } else {
                    ++eatAnimationTimer;
                }
                break;
            default:
                break;
        }
    }

    private boolean canPathfindToTarget(Frog body, LivingEntity target) {
        Path path = body.getNavigation().createPath(target, 0);
        return path != null && path.getDistToTarget() < EATING_DISTANCE;
    }

    private void addUnreachableTargetToMemory(Frog body, LivingEntity entity) {
        List<UUID> unreachable = new ArrayList<>(body.getBrain().getMemory(MemoryModuleType.UNREACHABLE_TONGUE_TARGETS).orElseGet(ArrayList::new));
        boolean add = !unreachable.contains(entity.getUUID());
        if (unreachable.size() == MAX_UNREACHBLE_TONGUE_TARGETS_IN_MEMORY && add) {
            unreachable.remove(0);
        }
        if (add) {
            unreachable.add(entity.getUUID());
        }
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.UNREACHABLE_TONGUE_TARGETS, unreachable, UNREACHABLE_TONGUE_TARGETS_COOLDOWN_DURATION);
    }

    private static enum State {
        MOVE_TO_TARGET, CATCH_ANIMATION, EAT_ANIMATION, DONE;
    }
}
