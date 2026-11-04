// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Comparator;
import java.util.Optional;
import java.util.UUID;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.NearestVisibleLivingEntities;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.phys.Vec3;

public class BehaviorUtils {
    private BehaviorUtils() {
    }

    public static void lockGazeAndWalkToEachOther(LivingEntity entity1, LivingEntity entity2, float speedModifier, int closeEnoughDistance) {
        lookAtEntity(entity1, entity2);
        lookAtEntity(entity2, entity1);
        setWalkAndLookTargetMemories(entity1, (Entity) entity2, speedModifier, closeEnoughDistance);
        setWalkAndLookTargetMemories(entity2, (Entity) entity1, speedModifier, closeEnoughDistance);
    }

    public static boolean entityIsVisible(Brain<?> brain, LivingEntity targetEntity) {
        Optional<NearestVisibleLivingEntities> visible = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES);
        return visible.isPresent() && visible.get().contains(targetEntity);
    }

    public static boolean targetIsValid(Brain<?> brain, MemoryModuleType<? extends LivingEntity> memory, EntityType<?> targetType) {
        Predicate<LivingEntity> sameType = entity -> entity.getType() == targetType;
        return brain.getMemory(memory).filter(sameType).filter(LivingEntity::isAlive).filter(entity -> entityIsVisible(brain, entity)).isPresent();
    }

    public static void lookAtEntity(LivingEntity looker, LivingEntity targetEntity) {
        looker.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(targetEntity, true));
    }

    public static void setWalkAndLookTargetMemories(LivingEntity walker, Entity targetEntity, float speedModifier, int closeEnoughDistance) {
        setWalkAndLookTargetMemories(walker, new EntityTracker(targetEntity, true), speedModifier, closeEnoughDistance);
    }

    public static void setWalkAndLookTargetMemories(LivingEntity walker, BlockPos targetPos, float speedModifier, int closeEnoughDistance) {
        setWalkAndLookTargetMemories(walker, new BlockPosTracker(targetPos), speedModifier, closeEnoughDistance);
    }

    public static void setWalkAndLookTargetMemories(LivingEntity walker, PositionTracker target, float speedModifier, int closeEnoughDistance) {
        walker.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, target);
        walker.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(target, speedModifier, closeEnoughDistance));
    }

    public static boolean isWithinAttackRange(Mob body, LivingEntity target, int projectileAttackRangeMargin) {
        return body.isWithinMeleeAttackRange(target);
    }

    public static boolean isOtherTargetMuchFurtherAwayThanCurrentAttackTarget(LivingEntity body, LivingEntity otherTarget, double howMuchFurtherAway) {
        Optional<LivingEntity> current = body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET);
        if (current.isEmpty()) {
            return false;
        }
        double toCurrent = body.distanceToSqr(current.get().position());
        double toOther = body.distanceToSqr(otherTarget.position());
        return toOther > toCurrent + howMuchFurtherAway * howMuchFurtherAway;
    }

    public static boolean canSee(LivingEntity body, LivingEntity target) {
        Brain<?> brain = body.getBrain();
        return brain.hasMemoryValue(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES)
            && brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES).get().contains(target);
    }

    public static LivingEntity getNearestTarget(LivingEntity body, Optional<LivingEntity> target1, LivingEntity target2) {
        return target1.isEmpty() ? target2 : getTargetNearestMe(body, target1.get(), target2);
    }

    public static LivingEntity getTargetNearestMe(LivingEntity body, LivingEntity target1, LivingEntity target2) {
        return body.distanceToSqr(target1.position()) < body.distanceToSqr(target2.position()) ? target1 : target2;
    }

    public static Optional<LivingEntity> getLivingEntityFromUUIDMemory(LivingEntity body, MemoryModuleType<UUID> memoryType) {
        return body.getBrain().getMemory(memoryType).map(uuid -> body.level().getEntity(uuid))
            .map(entity -> entity instanceof LivingEntity living ? living : null);
    }

    public static boolean isBreeding(LivingEntity body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.BREED_TARGET);
    }

    public static void throwItem(LivingEntity thrower, net.minecraft.world.item.ItemStack item, Vec3 targetPos) {
        throwItem(thrower, item, targetPos, new Vec3(0.3, 0.3, 0.3), 0.3F);
    }

    public static void throwItem(LivingEntity thrower, net.minecraft.world.item.ItemStack item, Vec3 targetPos, Vec3 throwVelocity, float handYDistanceFromEye) {
        net.minecraft.world.entity.item.ItemEntity itemEntity = new net.minecraft.world.entity.item.ItemEntity(thrower.level(), thrower.getX(),
            thrower.getEyeY() - handYDistanceFromEye, thrower.getZ(), item);
        itemEntity.setThrower(thrower);
        Vec3 vector = targetPos.subtract(thrower.position()).normalize().multiply(throwVelocity.x, throwVelocity.y, throwVelocity.z);
        itemEntity.setDeltaMovement(vector);
        itemEntity.setDefaultPickUpDelay();
        thrower.level().addFreshEntity(itemEntity);
    }

    public static net.minecraft.core.SectionPos findSectionClosestToVillage(net.minecraft.server.level.ServerLevel level, net.minecraft.core.SectionPos center, int radius) {
        int centerDistance = level.sectionsToVillage(center);
        return net.minecraft.core.SectionPos.cube(center, radius).filter(s -> level.sectionsToVillage(s) < centerDistance)
            .min(java.util.Comparator.comparingInt(level::sectionsToVillage)).orElse(center);
    }
}
