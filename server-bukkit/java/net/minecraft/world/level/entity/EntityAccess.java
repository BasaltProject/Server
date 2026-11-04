// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;

import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.AABB;
public interface EntityAccess extends UniquelyIdentifyable {
    default int getId() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#getId");
    }
    default BlockPos blockPosition() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#blockPosition");
    }
    default AABB getBoundingBox() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#getBoundingBox");
    }
    default void setLevelCallback(EntityInLevelCallback callback) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#setLevelCallback");
    }
    default Stream<? extends EntityAccess> getSelfAndPassengers() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#getSelfAndPassengers");
    }
    default Stream<? extends EntityAccess> getPassengersAndSelf() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#getPassengersAndSelf");
    }
    default void setRemoved(Entity.RemovalReason removalReason) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#setRemoved");
    }
    default boolean shouldBeSaved() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#shouldBeSaved");
    }
    default boolean isAlwaysTicking() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.EntityAccess#isAlwaysTicking");
    }
}
