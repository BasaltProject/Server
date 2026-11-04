// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;
public interface ItemOwner {
    default Level level() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner#level");
    }
    default Vec3 position() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner#position");
    }
    default float getVisualRotationYInDegrees() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner#getVisualRotationYInDegrees");
    }
    default LivingEntity asLivingEntity() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner#asLivingEntity");
    }
    static ItemOwner offsetFromOwner(ItemOwner owner, Vec3 offset) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner#offsetFromOwner");
    }
    public static record OffsetFromOwner(ItemOwner owner, Vec3 offset) implements ItemOwner {
        public Level level() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner.OffsetFromOwner#level");
        }
        public Vec3 position() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner.OffsetFromOwner#position");
        }
        public float getVisualRotationYInDegrees() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner.OffsetFromOwner#getVisualRotationYInDegrees");
        }
        public LivingEntity asLivingEntity() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.ItemOwner.OffsetFromOwner#asLivingEntity");
        }
    }
}
