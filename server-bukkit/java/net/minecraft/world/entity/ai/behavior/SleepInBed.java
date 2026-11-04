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

public class SleepInBed extends Behavior<LivingEntity> {
    public static final int COOLDOWN_AFTER_BEING_WOKEN = 100;
    private long nextOkStartTime;

    public SleepInBed() {
        super(Map.of(MemoryModuleType.HOME, MemoryStatus.VALUE_PRESENT, MemoryModuleType.LAST_WOKEN, MemoryStatus.REGISTERED, MemoryModuleType.LAST_SLEPT,
            MemoryStatus.REGISTERED, MemoryModuleType.WALK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE, MemoryStatus.REGISTERED));
    }

    protected boolean checkExtraStartConditions(ServerLevel level, LivingEntity body) {
        if (body.isPassenger()) {
            return false;
        }
        Brain<?> brain = body.getBrain();
        GlobalPos target = brain.getMemory(MemoryModuleType.HOME).get();
        if (level.dimension() != target.dimension()) {
            return false;
        }
        Optional<Long> lastWoken = brain.getMemory(MemoryModuleType.LAST_WOKEN);
        if (lastWoken.isPresent()) {
            long since = level.getGameTime() - lastWoken.get();
            if (since > 0L && since < COOLDOWN_AFTER_BEING_WOKEN) {
                return false;
            }
        }
        var state = level.getBlockState(target.pos());
        return target.pos().closerToCenterThan(body.position(), 2.0) && state.is(net.minecraft.tags.BlockTags.VILLAGERS_CAN_SLEEP_ON_BED)
            && !state.getValue(net.minecraft.world.level.block.AbstractBedBlock.OCCUPIED);
    }

    protected boolean canStillUse(ServerLevel level, LivingEntity body, long timestamp) {
        Optional<GlobalPos> memory = body.getBrain().getMemory(MemoryModuleType.HOME);
        if (memory.isEmpty()) {
            return false;
        }
        BlockPos bedPos = memory.get().pos();
        return body.getBrain().isActive(Activity.REST) && body.getY() > bedPos.getY() + 0.4 && bedPos.closerToCenterThan(body.position(), 1.14);
    }

    protected void start(ServerLevel level, LivingEntity body, long timestamp) {
        if (timestamp > nextOkStartTime) {
            Brain<?> brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.DOORS_TO_CLOSE)) {
                Set<GlobalPos> doors = brain.getMemory(MemoryModuleType.DOORS_TO_CLOSE).get();
                Optional<List<LivingEntity>> nearest = brain.hasMemoryValue(MemoryModuleType.NEAREST_LIVING_ENTITIES)
                    ? brain.getMemory(MemoryModuleType.NEAREST_LIVING_ENTITIES) : Optional.empty();
                InteractWithDoor.closeDoorsThatIHaveOpenedOrPassedThrough(level, body, null, null, doors, nearest);
            }
            if (body.startSleeping(brain.getMemory(MemoryModuleType.HOME).get().pos())) {
                brain.setMemory(MemoryModuleType.LAST_SLEPT, timestamp);
            }
            brain.eraseMemory(MemoryModuleType.WALK_TARGET);
            brain.eraseMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE);
        }
    }

    protected boolean timedOut(long timestamp) {
        return false;
    }

    protected void stop(ServerLevel level, LivingEntity body, long timestamp) {
        if (body.isSleeping()) {
            body.stopSleeping();
            nextOkStartTime = timestamp + 40L;
        }
    }
}
