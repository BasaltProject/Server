// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.world.level.EntityGetter;
import java.util.List;
import net.minecraft.tags.TagKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.AABB;
public interface ServerEntityGetter extends EntityGetter {
    ServerLevel getLevel();
    default Player getNearestPlayer(TargetingConditions targetConditions, LivingEntity source) {
        return getNearestPlayer(targetConditions, source, source.getX(), source.getY(), source.getZ());
    }
    default Player getNearestPlayer(TargetingConditions targetConditions, LivingEntity source, double x, double y, double z) {
        return getNearestEntity(players(), targetConditions, source, x, y, z);
    }
    default Player getNearestPlayer(TargetingConditions targetConditions, double x, double y, double z) {
        return getNearestPlayer(targetConditions, null, x, y, z);
    }
    default <T extends LivingEntity> T getNearestEntity(Class<? extends T> type, TargetingConditions targetConditions, LivingEntity source, double x, double y, double z, AABB bb) {
        return getNearestEntity(getEntitiesOfClass(type, bb, candidate -> true), targetConditions, source, x, y, z);
    }
    default LivingEntity getNearestEntity(TagKey<EntityType<?>> tag, TargetingConditions targetConditions, LivingEntity source, double x, double y, double z, AABB bb) {
        return getNearestEntity(getEntitiesOfClass(LivingEntity.class, bb, candidate -> org.basaltmc.bukkit.item.TagSets.members("entity_type", tag.location().toString())
            .contains(net.minecraft.world.entity.EntityType.getKey(candidate.getType()).toString())), targetConditions, source, x, y, z);
    }
    default <T extends Entity> T getNearestEntity(List<? extends T> entities, double x, double y, double z) {
        T closest = null;
        double bestDistance = Double.MAX_VALUE;
        for (T candidate : entities) {
            double distance = candidate.distanceToSqr(x, y, z);
            if (distance < bestDistance) {
                bestDistance = distance;
                closest = candidate;
            }
        }
        return closest;
    }
    default <T extends LivingEntity> T getNearestEntity(List<? extends T> entities, TargetingConditions targetConditions, LivingEntity source, double x, double y, double z) {
        T closest = null;
        double bestDistance = Double.MAX_VALUE;
        for (T candidate : entities) {
            if (!targetConditions.test(getLevel(), source, candidate)) {
                continue;
            }
            double distance = candidate.distanceToSqr(x, y, z);
            if (distance < bestDistance) {
                bestDistance = distance;
                closest = candidate;
            }
        }
        return closest;
    }
    default List<Player> getNearbyPlayers(TargetingConditions targetConditions, LivingEntity source, AABB bb) {
        List<Player> found = new java.util.ArrayList<>();
        for (Player player : players()) {
            if (bb.contains(player.getX(), player.getY(), player.getZ()) && targetConditions.test(getLevel(), source, player)) {
                found.add(player);
            }
        }
        return found;
    }
    default <T extends LivingEntity> List<T> getNearbyEntities(Class<T> type, TargetingConditions targetConditions, LivingEntity source, AABB bb) {
        return getEntitiesOfClass(type, bb, candidate -> targetConditions.test(getLevel(), source, candidate));
    }
}
