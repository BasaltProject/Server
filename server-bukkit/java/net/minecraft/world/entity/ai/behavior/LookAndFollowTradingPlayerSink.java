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

public class LookAndFollowTradingPlayerSink extends Behavior<Villager> {
    private final float speedModifier;

    public LookAndFollowTradingPlayerSink(float speedModifier) {
        super(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED), Integer.MAX_VALUE);
        this.speedModifier = speedModifier;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        Player trading = body.getTradingPlayer();
        return body.isAlive() && trading != null && !body.isInWater() && !body.wasHurtRecently() && body.distanceToSqr(trading) <= 16.0;
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return checkExtraStartConditions(level, body);
    }

    protected void start(ServerLevel level, Villager body, long timestamp) {
        followPlayer(body);
    }

    protected void stop(ServerLevel level, Villager body, long timestamp) {
        body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
        body.getBrain().eraseMemory(MemoryModuleType.LOOK_TARGET);
    }

    protected void tick(ServerLevel level, Villager body, long timestamp) {
        followPlayer(body);
    }

    protected boolean timedOut(long timestamp) {
        return false;
    }

    private void followPlayer(Villager body) {
        body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(body.getTradingPlayer(), false), speedModifier, 2));
        body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(body.getTradingPlayer(), true));
    }
}
