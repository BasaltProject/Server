// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.phys.Vec3;

public class VillagerPanicTrigger extends Behavior<Villager> {
    public VillagerPanicTrigger() {
        super(java.util.Map.of());
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return isHurt(body) || hasHostile(body);
    }

    protected void start(ServerLevel level, Villager body, long timestamp) {
        if (!canStillUse(level, body, timestamp)) {
            return;
        }
        Brain<?> mind = body.getBrain();
        if (!mind.isActive(Activity.PANIC)) {
            for (MemoryModuleType<?> dropped : java.util.List.of(MemoryModuleType.PATH, MemoryModuleType.WALK_TARGET,
                MemoryModuleType.LOOK_TARGET, MemoryModuleType.BREED_TARGET, MemoryModuleType.INTERACTION_TARGET)) {
                mind.eraseMemory(dropped);
            }
        }
        mind.setActiveActivityIfPossible(Activity.PANIC);
    }

    protected void tick(ServerLevel level, Villager body, long timestamp) {
        if (timestamp % 100L == 0L) {
            body.spawnGolemIfNeeded(level, timestamp, 3);
        }
    }

    public static boolean hasHostile(LivingEntity myBody) {
        return myBody.getBrain().hasMemoryValue(MemoryModuleType.NEAREST_HOSTILE);
    }

    public static boolean isHurt(LivingEntity myBody) {
        return myBody.getBrain().hasMemoryValue(MemoryModuleType.HURT_BY);
    }
}
