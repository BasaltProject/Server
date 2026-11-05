// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.List;
import java.util.UUID;
import java.util.function.Predicate;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.entity.EntityTypeTest;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.shapes.VoxelShape;
public interface EntityGetter {
    List<Entity> getEntities(Entity except, AABB bb, Predicate<? super Entity> selector);
    <T extends Entity> List<T> getEntities(EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector);
    default <T extends Entity> List<T> getEntitiesOfClass(Class<T> baseClass, AABB bb, Predicate<? super T> selector) {
        return getEntities(EntityTypeTest.forClass(baseClass), bb, selector);
    }
    List<? extends Player> players();
    default List<Entity> getEntities(Entity except, AABB bb) {
        return getEntities(except, bb, entity -> !entity.isSpectator());
    }
    default boolean isUnobstructed(Entity source, VoxelShape shape) {
        return true;
    }
    default <T extends Entity> List<T> getEntitiesOfClass(Class<T> baseClass, AABB bb) {
        return getEntitiesOfClass(baseClass, bb, entity -> true);
    }
    default List<VoxelShape> getEntityCollisions(Entity source, AABB testArea) {
        return List.of();
    }
    default Player getNearestPlayer(double x, double y, double z, double range, Predicate<Entity> predicate) {
        double best = -1.0;
        Player closest = null;
        for (Player player : players()) {
            if (!predicate.test(player)) {
                continue;
            }
            double distance = player.distanceToSqr(x, y, z);
            if ((range < 0.0 || distance < range * range) && (best == -1.0 || distance < best)) {
                best = distance;
                closest = player;
            }
        }
        return closest;
    }
    default Player getNearestPlayer(Entity source, double maxDist) {
        return getNearestPlayer(source.getX(), source.getY(), source.getZ(), maxDist, false);
    }
    default Player getNearestPlayer(double x, double y, double z, double maxDist, boolean filterOutCreative) {
        return getNearestPlayer(x, y, z, maxDist, filterOutCreative ? entity -> !(entity instanceof Player p && p.isCreative())
            && !entity.isSpectator() : entity -> !entity.isSpectator());
    }
    default boolean hasNearbyAlivePlayer(double x, double y, double z, double range) {
        for (Player player : players()) {
            if (player.isAlive() && !player.isSpectator() && player.distanceToSqr(x, y, z) < range * range) {
                return true;
            }
        }
        return false;
    }
    default Player getPlayerByUUID(UUID uuid) {
        for (Player player : players()) {
            if (uuid.equals(player.getUUID())) {
                return player;
            }
        }
        return null;
    }
}
