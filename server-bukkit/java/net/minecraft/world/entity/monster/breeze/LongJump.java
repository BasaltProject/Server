// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.breeze;

import it.unimi.dsi.fastutil.objects.ObjectArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.tags.FluidTags;
import net.minecraft.util.RandomSource;
import net.minecraft.util.Unit;
import net.minecraft.util.Util;
import net.minecraft.world.effect.MobEffects;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.behavior.Behavior;
import net.minecraft.world.entity.ai.behavior.LongJumpUtil;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;

public class LongJump extends Behavior<Breeze> {
    private static final int REQUIRED_AIR_BLOCKS_ABOVE = 4;
    private static final int JUMP_COOLDOWN_TICKS = 10;
    private static final int JUMP_COOLDOWN_WHEN_HURT_TICKS = 2;
    private static final int INHALING_DURATION_TICKS = Math.round(10.0F);
    private static final float MAX_JUMP_VELOCITY_MULTIPLIER = 0.058333334F;
    private static final List<Integer> ALLOWED_ANGLES = List.of(40, 55, 60, 75, 80);

    public LongJump() {
        super(Map.of(MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT, MemoryModuleType.BREEZE_JUMP_COOLDOWN, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.BREEZE_JUMP_INHALING, MemoryStatus.REGISTERED, MemoryModuleType.BREEZE_JUMP_TARGET, MemoryStatus.REGISTERED,
            MemoryModuleType.BREEZE_SHOOT, MemoryStatus.VALUE_ABSENT, MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.BREEZE_LEAVING_WATER, MemoryStatus.REGISTERED), 200);
    }

    public static boolean canRun(ServerLevel level, Breeze breeze) {
        boolean grounded = breeze.onGround() || breeze.isInWater();
        boolean submerged = breeze.isInFluidDeeperThan(breeze.getFluidJumpThreshold(), FluidTags.ENTITY_FLOATABLE) || breeze.isInLava();
        if (!grounded || submerged) {
            return false;
        }
        var memory = breeze.getBrain();
        if (memory.checkMemory(MemoryModuleType.BREEZE_JUMP_TARGET, MemoryStatus.VALUE_PRESENT)) {
            return true;
        }
        Optional<LivingEntity> prey = memory.getMemory(MemoryModuleType.ATTACK_TARGET);
        if (prey.isEmpty()) {
            return false;
        }
        LivingEntity quarry = prey.get();
        if (!quarry.closerThan(breeze, breeze.getAttributeValue(Attributes.FOLLOW_RANGE))) {
            memory.eraseMemory(MemoryModuleType.ATTACK_TARGET);
            return false;
        }
        boolean tooClose = quarry.distanceTo(breeze) - 4.0F <= 0.0F;
        if (tooClose || !canJumpFromCurrentPosition(level, breeze)) {
            return false;
        }
        BlockPos landing = snapToSurface(breeze, BreezeUtil.randomPointBehindTarget(quarry, breeze.getRandom()));
        if (landing == null || breeze.getType().isBlockDangerous(level.getBlockState(landing.below()))) {
            return false;
        }
        boolean visible = BreezeUtil.hasLineOfSight(breeze, Vec3.atCenterOf(landing))
            || BreezeUtil.hasLineOfSight(breeze, Vec3.atCenterOf(landing.above(REQUIRED_AIR_BLOCKS_ABOVE)));
        if (!visible) {
            return false;
        }
        memory.setMemory(MemoryModuleType.BREEZE_JUMP_TARGET, landing);
        return true;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Breeze breeze) {
        return canRun(level, breeze);
    }

    protected boolean canStillUse(ServerLevel level, Breeze breeze, long timestamp) {
        return breeze.getPose() != Pose.STANDING && !breeze.getBrain().hasMemoryValue(MemoryModuleType.BREEZE_JUMP_COOLDOWN);
    }

    protected void start(ServerLevel level, Breeze breeze, long timestamp) {
        if (breeze.getBrain().checkMemory(MemoryModuleType.BREEZE_JUMP_INHALING, MemoryStatus.VALUE_ABSENT)) {
            breeze.getBrain().setMemoryWithExpiry(MemoryModuleType.BREEZE_JUMP_INHALING, Unit.INSTANCE, INHALING_DURATION_TICKS);
        }
        breeze.setPose(Pose.INHALING);
        level.playSound(null, breeze, SoundEvents.BREEZE_CHARGE, SoundSource.HOSTILE, 1.0F, 1.0F);
        breeze.getBrain().getMemory(MemoryModuleType.BREEZE_JUMP_TARGET)
            .ifPresent(targetPos -> breeze.lookAt(net.minecraft.commands.arguments.EntityAnchorArgument.Anchor.EYES, Vec3.atCenterOf(targetPos)));
    }

    protected void tick(ServerLevel level, Breeze breeze, long timestamp) {
        var memory = breeze.getBrain();
        boolean swimming = breeze.isInWater();
        if (!swimming && memory.checkMemory(MemoryModuleType.BREEZE_LEAVING_WATER, MemoryStatus.VALUE_PRESENT)) {
            memory.eraseMemory(MemoryModuleType.BREEZE_LEAVING_WATER);
        }
        if (isFinishedInhaling(breeze)) {
            Optional<Vec3> launch = memory.getMemory(MemoryModuleType.BREEZE_JUMP_TARGET)
                .flatMap(spot -> calculateOptimalJumpVector(breeze, breeze.getRandom(), Vec3.atBottomCenterOf(spot)));
            if (launch.isEmpty()) {
                breeze.setPose(Pose.STANDING);
                return;
            }
            if (swimming) {
                memory.setMemory(MemoryModuleType.BREEZE_LEAVING_WATER, Unit.INSTANCE);
            }
            breeze.playSound(SoundEvents.BREEZE_JUMP, 1.0F, 1.0F);
            breeze.setPose(Pose.LONG_JUMPING);
            breeze.setYRot(breeze.yBodyRot);
            breeze.setDiscardFriction(true);
            breeze.setDeltaMovement(launch.get());
            return;
        }
        if (!isFinishedJumping(breeze)) {
            return;
        }
        breeze.playSound(SoundEvents.BREEZE_LAND, 1.0F, 1.0F);
        breeze.setPose(Pose.STANDING);
        breeze.setDiscardFriction(false);
        long cooldown = memory.hasMemoryValue(MemoryModuleType.HURT_BY) ? JUMP_COOLDOWN_WHEN_HURT_TICKS : JUMP_COOLDOWN_TICKS;
        memory.setMemoryWithExpiry(MemoryModuleType.BREEZE_JUMP_COOLDOWN, Unit.INSTANCE, cooldown);
        memory.setMemoryWithExpiry(MemoryModuleType.BREEZE_SHOOT, Unit.INSTANCE, 100L);
    }

    protected void stop(ServerLevel level, Breeze breeze, long timestamp) {
        Pose pose = breeze.getPose();
        if (pose == Pose.LONG_JUMPING || pose == Pose.INHALING) {
            breeze.setPose(Pose.STANDING);
        }
        for (MemoryModuleType<?> type : List.of(MemoryModuleType.BREEZE_JUMP_TARGET, MemoryModuleType.BREEZE_JUMP_INHALING,
            MemoryModuleType.BREEZE_LEAVING_WATER)) {
            breeze.getBrain().eraseMemory(type);
        }
    }

    private static boolean isFinishedInhaling(Breeze breeze) {
        return breeze.getBrain().getMemory(MemoryModuleType.BREEZE_JUMP_INHALING).isEmpty() && breeze.getPose() == Pose.INHALING;
    }

    private static boolean isFinishedJumping(Breeze breeze) {
        boolean landedInWater = breeze.isInWater() && breeze.getBrain().checkMemory(MemoryModuleType.BREEZE_LEAVING_WATER, MemoryStatus.VALUE_ABSENT);
        return breeze.getPose() == Pose.LONG_JUMPING && (breeze.onGround() || landedInWater);
    }

    private static BlockPos snapToSurface(LivingEntity entity, Vec3 target) {
        HitResult below = entity.level().clip(new ClipContext(target, target.relative(Direction.DOWN, 10.0), ClipContext.Block.COLLIDER, ClipContext.Fluid.NONE, entity));
        if (below.getType() == HitResult.Type.BLOCK) {
            return BlockPos.containing(below.getLocation()).above();
        }
        HitResult above = entity.level().clip(new ClipContext(target, target.relative(Direction.UP, 10.0), ClipContext.Block.COLLIDER, ClipContext.Fluid.NONE, entity));
        return above.getType() == HitResult.Type.BLOCK ? BlockPos.containing(above.getLocation()).above() : null;
    }

    private static boolean canJumpFromCurrentPosition(ServerLevel level, Breeze breeze) {
        BlockPos feet = breeze.blockPosition();
        if (level.getBlockState(feet).is(Blocks.HONEY_BLOCK)) {
            return false;
        }
        for (int height = 1; height <= REQUIRED_AIR_BLOCKS_ABOVE; height++) {
            BlockPos cell = feet.relative(Direction.UP, height);
            boolean open = level.getBlockState(cell).isAir() || level.getFluidState(cell).is(FluidTags.WATER);
            if (!open) {
                return false;
            }
        }
        return true;
    }

    private static Optional<Vec3> calculateOptimalJumpVector(Breeze body, RandomSource random, Vec3 targetPos) {
        for (int angle : Util.shuffledCopy(new ObjectArrayList<>(ALLOWED_ANGLES), random)) {
            float maxJumpVelocity = MAX_JUMP_VELOCITY_MULTIPLIER * (float) body.getAttributeValue(Attributes.FOLLOW_RANGE);
            Optional<Vec3> velocity = LongJumpUtil.calculateJumpVectorForAngle(body, targetPos, maxJumpVelocity, angle, false);
            if (velocity.isPresent()) {
                if (body.hasEffect(MobEffects.JUMP_BOOST)) {
                    double amplifier = velocity.get().normalize().y * body.getJumpBoostPower();
                    return velocity.map(v -> v.add(0.0, amplifier, 0.0));
                }
                return velocity;
            }
        }
        return Optional.empty();
    }
}
