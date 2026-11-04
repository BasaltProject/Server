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

public class WorkAtPoi extends Behavior<Villager> {
    private static final int CHECK_COOLDOWN = 300;
    private static final double DISTANCE = 1.73;
    private long lastCheck;

    public WorkAtPoi() {
        super(java.util.Map.of(MemoryModuleType.JOB_SITE, net.minecraft.world.entity.ai.memory.MemoryStatus.VALUE_PRESENT, MemoryModuleType.LOOK_TARGET,
            net.minecraft.world.entity.ai.memory.MemoryStatus.REGISTERED));
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        if (level.getGameTime() - lastCheck < CHECK_COOLDOWN || level.getRandom().nextInt(2) != 0) {
            return false;
        }
        lastCheck = level.getGameTime();
        GlobalPos target = body.getBrain().getMemory(MemoryModuleType.JOB_SITE).get();
        return target.dimension() == level.dimension() && target.pos().closerToCenterThan(body.position(), DISTANCE);
    }

    protected void start(ServerLevel level, Villager body, long timestamp) {
        Brain<Villager> brain = body.getBrain();
        brain.setMemory(MemoryModuleType.LAST_WORKED_AT_POI, timestamp);
        brain.getMemory(MemoryModuleType.JOB_SITE).ifPresent(globalPos -> brain.setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(globalPos.pos())));
        body.playWorkSound();
        useWorkstation(level, body);
        if (body.shouldRestock(level)) {
            body.restock();
        }
    }

    protected void useWorkstation(ServerLevel level, Villager body) {
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        Optional<GlobalPos> jobSite = body.getBrain().getMemory(MemoryModuleType.JOB_SITE);
        return jobSite.isPresent() && jobSite.get().dimension() == level.dimension() && jobSite.get().pos().closerToCenterThan(body.position(), DISTANCE);
    }
}
