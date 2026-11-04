// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.DamageTypeTags;
import net.minecraft.tags.FluidTags;
import net.minecraft.tags.TagKey;
import net.minecraft.util.Mth;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.ai.util.AirAndWaterRandomPos;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.phys.Vec3;

public class AnimalPanic<E extends PathfinderMob> extends Behavior<E> {
    private static final int PANIC_HORIZONTAL = 5;
    private static final int PANIC_VERTICAL = 4;
    private final float speedMultiplier;
    private final Function<PathfinderMob, TagKey<DamageType>> panicCausingDamageTypes;
    private final Function<E, Vec3> positionGetter;

    public AnimalPanic(float speedMultiplier) {
        this(speedMultiplier, mob -> DamageTypeTags.PANIC_CAUSES, mob -> LandRandomPos.getPos(mob, PANIC_HORIZONTAL, PANIC_VERTICAL));
    }

    public AnimalPanic(float speedMultiplier, int flyHeight) {
        this(speedMultiplier, mob -> DamageTypeTags.PANIC_CAUSES, mob -> {
            Vec3 view = mob.getViewVector(0.0F);
            return AirAndWaterRandomPos.getPos(mob, PANIC_HORIZONTAL, PANIC_VERTICAL, flyHeight, view.x, view.z, (float) Math.PI / 2F);
        });
    }

    public AnimalPanic(float speedMultiplier, Function<PathfinderMob, TagKey<DamageType>> panicCausingDamageTypes) {
        this(speedMultiplier, panicCausingDamageTypes, mob -> LandRandomPos.getPos(mob, PANIC_HORIZONTAL, PANIC_VERTICAL));
    }

    public AnimalPanic(float speedMultiplier, Function<PathfinderMob, TagKey<DamageType>> panicCausingDamageTypes, Function<E, Vec3> positionGetter) {
        super(Map.of(MemoryModuleType.IS_PANICKING, MemoryStatus.REGISTERED, MemoryModuleType.HURT_BY, MemoryStatus.REGISTERED), 100, 120);
        this.speedMultiplier = speedMultiplier;
        this.panicCausingDamageTypes = panicCausingDamageTypes;
        this.positionGetter = positionGetter;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, E body) {
        return body.getBrain().getMemory(MemoryModuleType.HURT_BY).map(source -> source.is(panicCausingDamageTypes.apply(body))).orElse(false)
            || body.getBrain().hasMemoryValue(MemoryModuleType.IS_PANICKING);
    }

    protected boolean canStillUse(ServerLevel level, E body, long timestamp) {
        return true;
    }

    protected void start(ServerLevel level, E body, long timestamp) {
        body.getBrain().setMemory(MemoryModuleType.IS_PANICKING, true);
        body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
        body.getNavigation().stop();
    }

    protected void stop(ServerLevel level, E body, long timestamp) {
        body.getBrain().eraseMemory(MemoryModuleType.IS_PANICKING);
    }

    protected void tick(ServerLevel level, E body, long timestamp) {
        if (body.getNavigation().isDone()) {
            Vec3 target = panicPos(body, level);
            if (target != null) {
                body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(target, speedMultiplier, 0));
            }
        }
    }

    private Vec3 panicPos(E body, ServerLevel level) {
        if (body.isOnFire()) {
            Optional<Vec3> water = lookForWater(level, body).map(Vec3::atBottomCenterOf);
            if (water.isPresent()) {
                return water.get();
            }
        }
        return positionGetter.apply(body);
    }

    private Optional<BlockPos> lookForWater(ServerLevel level, E mob) {
        BlockPos origin = mob.blockPosition();
        if (!level.getBlockState(origin).getCollisionShape(level, origin).isEmpty()) {
            return Optional.empty();
        }
        boolean wide = Mth.ceil(mob.getBbWidth()) == 2;
        for (BlockPos pos : BlockPos.betweenClosed(origin.offset(-PANIC_HORIZONTAL, -1, -PANIC_HORIZONTAL), origin.offset(PANIC_HORIZONTAL, 1, PANIC_HORIZONTAL))) {
            if (Math.abs(pos.getX() - origin.getX()) + Math.abs(pos.getY() - origin.getY()) + Math.abs(pos.getZ() - origin.getZ()) > PANIC_HORIZONTAL + 1) {
                continue;
            }
            if (isWater(level, pos) && (!wide || isWater(level, pos.south()) && isWater(level, pos.east()) && isWater(level, pos.south().east()))) {
                return Optional.of(pos.immutable());
            }
        }
        return Optional.empty();
    }

    private static boolean isWater(ServerLevel level, BlockPos pos) {
        return level.getBlockState(pos).getFluidState().is(FluidTags.WATER);
    }
}
