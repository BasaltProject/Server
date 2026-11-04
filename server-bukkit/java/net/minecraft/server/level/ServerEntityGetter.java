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
    default ServerLevel getLevel() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getLevel");
    }
    default Player getNearestPlayer(TargetingConditions targetConditions, LivingEntity source) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearestPlayer");
    }
    default Player getNearestPlayer(TargetingConditions targetConditions, LivingEntity source, double x, double y, double z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearestPlayer");
    }
    default Player getNearestPlayer(TargetingConditions targetConditions, double x, double y, double z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearestPlayer");
    }
    default <T extends LivingEntity> T getNearestEntity(Class<? extends T> type, TargetingConditions targetConditions, LivingEntity source, double x, double y, double z, AABB bb) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearestEntity");
    }
    default LivingEntity getNearestEntity(TagKey<EntityType<?>> tag, TargetingConditions targetConditions, LivingEntity source, double x, double y, double z, AABB bb) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearestEntity");
    }
    default <T extends Entity> T getNearestEntity(List<? extends T> entities, double x, double y, double z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearestEntity");
    }
    default <T extends LivingEntity> T getNearestEntity(List<? extends T> entities, TargetingConditions targetConditions, LivingEntity source, double x, double y, double z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearestEntity");
    }
    default List<Player> getNearbyPlayers(TargetingConditions targetConditions, LivingEntity source, AABB bb) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearbyPlayers");
    }
    default <T extends LivingEntity> List<T> getNearbyEntities(Class<T> type, TargetingConditions targetConditions, LivingEntity source, AABB bb) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerEntityGetter#getNearbyEntities");
    }
}
