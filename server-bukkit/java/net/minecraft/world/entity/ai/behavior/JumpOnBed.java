// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;

public class JumpOnBed extends Behavior<Mob> {
    private static final int MAX_TIME_TO_REACH_BED = 100;
    private static final int MIN_JUMPS = 3;
    private static final int COOLDOWN_BETWEEN_JUMPS = 5;
    private final float speedModifier;
    private BlockPos targetBed;
    private int remainingTimeToReachBed;
    private int remainingJumps;
    private int remainingCooldownUntilNextJump;

    public JumpOnBed(float speedModifier) {
        super(Map.of(MemoryModuleType.NEAREST_BED, MemoryStatus.VALUE_PRESENT, MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT));
        this.speedModifier = speedModifier;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Mob body) {
        return body.isBaby() && nearBed(level, body);
    }

    protected void start(ServerLevel level, Mob body, long timestamp) {
        super.start(level, body, timestamp);
        getNearestBed(body).ifPresent(bed -> {
            targetBed = bed;
            remainingTimeToReachBed = MAX_TIME_TO_REACH_BED;
            remainingJumps = MIN_JUMPS + level.getRandom().nextInt(4);
            remainingCooldownUntilNextJump = 0;
            body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(bed, speedModifier, 0));
        });
    }

    protected void stop(ServerLevel level, Mob body, long timestamp) {
        super.stop(level, body, timestamp);
        targetBed = null;
        remainingTimeToReachBed = 0;
        remainingJumps = 0;
        remainingCooldownUntilNextJump = 0;
    }

    protected boolean canStillUse(ServerLevel level, Mob body, long timestamp) {
        return body.isBaby() && targetBed != null && isJumpable(level, targetBed) && !tiredOfWalking(level, body) && !tiredOfJumping(level, body);
    }

    protected boolean timedOut(long timestamp) {
        return false;
    }

    protected void tick(ServerLevel level, Mob body, long timestamp) {
        if (!onOrOverBed(level, body)) {
            --remainingTimeToReachBed;
        } else if (remainingCooldownUntilNextJump > 0) {
            --remainingCooldownUntilNextJump;
        } else if (isJumpable(level, body.blockPosition())) {
            body.getJumpControl().jump();
            --remainingJumps;
            remainingCooldownUntilNextJump = COOLDOWN_BETWEEN_JUMPS;
        }
    }

    private boolean nearBed(ServerLevel level, Mob body) {
        return onOrOverBed(level, body) || getNearestBed(body).isPresent();
    }

    private boolean onOrOverBed(ServerLevel level, Mob body) {
        BlockPos pos = body.blockPosition();
        return isJumpable(level, pos) || isJumpable(level, pos.below());
    }

    private boolean isJumpable(ServerLevel level, BlockPos pos) {
        return level.getBlockState(pos).is(net.minecraft.tags.BlockTags.VILLAGER_BABIES_CAN_JUMP_ON_BED);
    }

    private Optional<BlockPos> getNearestBed(Mob body) {
        return body.getBrain().getMemory(MemoryModuleType.NEAREST_BED);
    }

    private boolean tiredOfWalking(ServerLevel level, Mob body) {
        return !onOrOverBed(level, body) && remainingTimeToReachBed <= 0;
    }

    private boolean tiredOfJumping(ServerLevel level, Mob body) {
        return onOrOverBed(level, body) && remainingJumps <= 0;
    }
}
