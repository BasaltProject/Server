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

public class CelebrateVillagersSurvivedRaid extends Behavior<Villager> {
    private net.minecraft.world.entity.raid.Raid currentRaid;

    public CelebrateVillagersSurvivedRaid(int minDuration, int maxDuration) {
        super(Map.of(), minDuration, maxDuration);
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        BlockPos testPos = body.blockPosition();
        currentRaid = level.getRaidAt(testPos);
        return currentRaid != null && currentRaid.isVictory() && MoveToSkySeeingSpot.hasNoBlocksAbove(level, body, testPos);
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return currentRaid != null && !currentRaid.isStopped();
    }

    protected void stop(ServerLevel level, Villager body, long timestamp) {
        currentRaid = null;
        body.getBrain().updateActivityFromSchedule(level.environmentAttributes(), level.getGameTime(), body.position());
    }

    protected void tick(ServerLevel level, Villager body, long timestamp) {
        net.minecraft.util.RandomSource random = body.getRandom();
        if (random.nextInt(100) == 0) {
            body.playCelebrateSound();
        }
        if (random.nextInt(200) == 0 && MoveToSkySeeingSpot.hasNoBlocksAbove(level, body, body.blockPosition())) {
            org.bukkit.DyeColor[] colors = org.bukkit.DyeColor.values();
            org.bukkit.DyeColor color = colors[random.nextInt(colors.length)];
            if (body.live() instanceof org.bukkit.entity.Villager api) {
                org.bukkit.entity.Firework firework = api.getWorld().spawn(api.getEyeLocation(), org.bukkit.entity.Firework.class);
                org.bukkit.inventory.meta.FireworkMeta meta = firework.getFireworkMeta();
                meta.setPower(random.nextInt(3));
                meta.addEffect(org.bukkit.FireworkEffect.builder().with(org.bukkit.FireworkEffect.Type.BURST).withColor(color.getFireworkColor()).build());
                firework.setFireworkMeta(meta);
            }
        }
    }
}
