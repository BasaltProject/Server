// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.phys.Vec3;

public class VillagerMakeLove extends Behavior<Villager> {
    private long birthTimestamp;

    public VillagerMakeLove() {
        super(Map.of(MemoryModuleType.BREED_TARGET, MemoryStatus.VALUE_PRESENT, MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES, MemoryStatus.VALUE_PRESENT), 350, 350);
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        return isBreedingPossible(body);
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return timestamp <= birthTimestamp && isBreedingPossible(body);
    }

    protected void start(ServerLevel level, Villager body, long timestamp) {
        var breedTarget = body.getBrain().getMemory(MemoryModuleType.BREED_TARGET).get();
        BehaviorUtils.lockGazeAndWalkToEachOther(body, breedTarget, 0.5F, 2);
        level.broadcastEntityEvent(breedTarget, (byte) 18);
        level.broadcastEntityEvent(body, (byte) 18);
        birthTimestamp = timestamp + 275 + body.getRandom().nextInt(50);
    }

    protected void tick(ServerLevel level, Villager body, long timestamp) {
        Villager target = (Villager) body.getBrain().getMemory(MemoryModuleType.BREED_TARGET).get();
        if (body.distanceToSqr(target) > 5.0) {
            return;
        }
        BehaviorUtils.lockGazeAndWalkToEachOther(body, target, 0.5F, 2);
        if (timestamp >= birthTimestamp) {
            body.eatAndDigestFood();
            target.eatAndDigestFood();
            tryToGiveBirth(level, body, target);
        } else if (body.getRandom().nextInt(35) == 0) {
            level.broadcastEntityEvent(target, (byte) 12);
            level.broadcastEntityEvent(body, (byte) 12);
        }
    }

    private void tryToGiveBirth(ServerLevel level, Villager body, Villager target) {
        Optional<BlockPos> bed = level.getPoiManager().take(p -> p.is(net.minecraft.world.entity.ai.village.poi.PoiTypes.HOME),
            (poiType, poiPos) -> canReach(body, poiPos, poiType), body.blockPosition(), 48);
        if (bed.isEmpty()) {
            level.broadcastEntityEvent(target, (byte) 13);
            level.broadcastEntityEvent(body, (byte) 13);
            return;
        }
        Villager child = body.getBreedOffspring(level, target);
        if (child == null) {
            level.getPoiManager().release(bed.get());
            return;
        }
        body.setAge(6000);
        target.setAge(6000);
        child.setAge(-24000);
        child.snapTo(body.getX(), body.getY(), body.getZ(), 0.0F, 0.0F);
        level.addFreshEntityWithPassengers(child);
        level.broadcastEntityEvent(child, (byte) 12);
        child.getBrain().setMemory(MemoryModuleType.HOME, GlobalPos.of(level.dimension(), bed.get()));
    }

    protected void stop(ServerLevel level, Villager body, long timestamp) {
        body.getBrain().eraseMemory(MemoryModuleType.BREED_TARGET);
    }

    private boolean isBreedingPossible(Villager me) {
        Brain<Villager> brain = me.getBrain();
        var target = brain.getMemory(MemoryModuleType.BREED_TARGET).filter(e -> e.is(net.minecraft.world.entity.EntityTypes.VILLAGER));
        return target.isPresent() && BehaviorUtils.targetIsValid(brain, MemoryModuleType.BREED_TARGET, net.minecraft.world.entity.EntityTypes.VILLAGER)
            && me.canBreed() && target.get().canBreed();
    }

    private boolean canReach(Villager body, BlockPos poiPos, net.minecraft.core.Holder<net.minecraft.world.entity.ai.village.poi.PoiType> poiType) {
        var path = body.getNavigation().createPath(poiPos, poiType.value().validRange());
        return path != null && path.canReach();
    }
}
