// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Comparator;
import java.util.List;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.player.Player;

public class PlayerSensor extends Sensor<LivingEntity> {
    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.NEAREST_PLAYERS, MemoryModuleType.NEAREST_VISIBLE_PLAYER,
            MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYER, MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYERS);
    }

    protected void doTick(ServerLevel level, LivingEntity body) {
        double follow = getFollowDistance(body);
        List<Player> players = new java.util.ArrayList<>();
        for (Object candidate : level.players()) {
            Player player = (Player) candidate;
            if (!player.isSpectator() && player.isAlive() && body.closerThan(player, follow)) {
                players.add(player);
            }
        }
        players.sort(Comparator.comparingDouble(body::distanceToSqr));
        body.getBrain().setMemory(MemoryModuleType.NEAREST_PLAYERS, players);
        List<Player> visible = players.stream().filter(player -> isEntityTargetable(level, body, player)).toList();
        body.getBrain().setMemory(MemoryModuleType.NEAREST_VISIBLE_PLAYER, visible.isEmpty() ? null : visible.get(0));
        List<Player> attackable = visible.stream().filter(player -> isEntityAttackable(level, body, player)).toList();
        body.getBrain().setMemory(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYERS, attackable);
        body.getBrain().setMemory(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYER, attackable.isEmpty() ? null : attackable.get(0));
    }

    protected double getFollowDistance(LivingEntity body) {
        return body.getAttributeValue(Attributes.FOLLOW_RANGE);
    }
}
