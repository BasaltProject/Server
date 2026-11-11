// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.breeze;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.tags.FluidTags;
import net.minecraft.core.Direction;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.ai.behavior.Behavior;
public class LongJump extends Behavior<Breeze> {
    private static boolean hasHeadroom(ServerLevel level, Breeze breeze) {
        BlockPos here = breeze.blockPosition();
        if (level.getBlockState(here).is(Blocks.HONEY_BLOCK)) {
            return false;
        }
        for (int up = 1; up <= 4; up++) {
            BlockPos above = here.relative(Direction.UP, up);
            if (!level.getBlockState(above).isAir() && !level.getFluidState(above).is(FluidTags.WATER)) {
                return false;
            }
        }
        return true;
    }
    private static BlockPos surfaceNear(LivingEntity entity, Vec3 target) {
        for (Direction way : new Direction[] {Direction.DOWN, Direction.UP}) {
            ClipContext probe = new ClipContext(target, target.relative(way, 10.0), ClipContext.Block.COLLIDER, ClipContext.Fluid.NONE, entity);
            HitResult hit = entity.level().clip(probe);
            if (hit.getType() == HitResult.Type.BLOCK) {
                return BlockPos.containing(hit.getLocation()).above();
            }
        }
        return null;
    }
    public LongJump() {
        super((java.util.Map) null);
    }
    public static boolean canRun(ServerLevel level, Breeze breeze) {
        if (!breeze.onGround() && !breeze.isInWater()) {
            return false;
        }
        if (breeze.isInFluidDeeperThan(breeze.getFluidJumpThreshold(), FluidTags.ENTITY_FLOATABLE) || breeze.isInLava()) {
            return false;
        }
        if (breeze.getBrain().checkMemory(MemoryModuleType.BREEZE_JUMP_TARGET, MemoryStatus.VALUE_PRESENT)) {
            return true;
        }
        LivingEntity prey = breeze.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).orElse(null);
        if (prey == null) {
            return false;
        }
        if (!prey.closerThan(breeze, breeze.getAttributeValue(Attributes.FOLLOW_RANGE))) {
            breeze.getBrain().eraseMemory(MemoryModuleType.ATTACK_TARGET);
            return false;
        }
        if (prey.distanceTo(breeze) - 4.0F <= 0.0F || !hasHeadroom(level, breeze)) {
            return false;
        }
        BlockPos landing = surfaceNear(breeze, BreezeUtil.randomPointBehindTarget(prey, breeze.getRandom()));
        if (landing == null || breeze.getType().isBlockDangerous(level.getBlockState(landing.below()))) {
            return false;
        }
        if (!BreezeUtil.hasLineOfSight(breeze, Vec3.atCenterOf(landing)) && !BreezeUtil.hasLineOfSight(breeze, Vec3.atCenterOf(landing.above(4)))) {
            return false;
        }
        breeze.getBrain().setMemory(MemoryModuleType.BREEZE_JUMP_TARGET, landing);
        return true;
    }
    protected boolean checkExtraStartConditions(ServerLevel level, Breeze breeze) {
        return false;
    }
    protected boolean canStillUse(ServerLevel level, Breeze breeze, long timestamp) {
        return false;
    }
    protected void start(ServerLevel level, Breeze breeze, long timestamp) {
        
    }
    protected void tick(ServerLevel level, Breeze breeze, long timestamp) {
        
    }
    protected void stop(ServerLevel level, Breeze breeze, long timestamp) {
        
    }
}
