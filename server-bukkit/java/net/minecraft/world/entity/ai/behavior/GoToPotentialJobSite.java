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

public class GoToPotentialJobSite extends Behavior<Villager> {
    private static final int TICKS_UNTIL_TIMEOUT = 1200;
    private final float speedModifier;

    public GoToPotentialJobSite(float speedModifier) {
        super(java.util.Map.of(MemoryModuleType.POTENTIAL_JOB_SITE, net.minecraft.world.entity.ai.memory.MemoryStatus.VALUE_PRESENT), TICKS_UNTIL_TIMEOUT);
        this.speedModifier = speedModifier;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        return body.getBrain().getActiveNonCoreActivity().map(activity -> activity == Activity.IDLE || activity == Activity.WORK || activity == Activity.PLAY).orElse(true);
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.POTENTIAL_JOB_SITE);
    }

    protected void tick(ServerLevel level, Villager body, long timestamp) {
        BehaviorUtils.setWalkAndLookTargetMemories(body, body.getBrain().getMemory(MemoryModuleType.POTENTIAL_JOB_SITE).get().pos(), speedModifier, 1);
    }

    protected void stop(ServerLevel level, Villager body, long timestamp) {
        body.getBrain().getMemory(MemoryModuleType.POTENTIAL_JOB_SITE).ifPresent(globalPos -> {
            BlockPos pos = globalPos.pos();
            ServerLevel serverLevel = level.getServer().getLevel(globalPos.dimension());
            if (serverLevel != null && serverLevel.getPoiManager().exists(pos, p -> true)) {
                serverLevel.getPoiManager().release(pos);
            }
        });
        body.getBrain().eraseMemory(MemoryModuleType.POTENTIAL_JOB_SITE);
    }
}
