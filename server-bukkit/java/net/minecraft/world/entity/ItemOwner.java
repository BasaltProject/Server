// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;
public interface ItemOwner {
    default Level level() {
        LivingEntity living = asLivingEntity();
        return living == null ? null : living.level();
    }
    default Vec3 position() {
        LivingEntity living = asLivingEntity();
        return living == null ? Vec3.ZERO : living.position();
    }
    default float getVisualRotationYInDegrees() {
        LivingEntity living = asLivingEntity();
        return living == null ? 0.0F : living.getVisualRotationYInDegrees();
    }
    default LivingEntity asLivingEntity() {
        return this instanceof LivingEntity living ? living : null;
    }
    static ItemOwner offsetFromOwner(ItemOwner owner, Vec3 offset) {
        return new OffsetFromOwner(owner, offset);
    }
    public static record OffsetFromOwner(ItemOwner owner, Vec3 offset) implements ItemOwner {
        public Level level() {
            return owner.level();
        }
        public Vec3 position() {
            return owner.position().add(offset);
        }
        public float getVisualRotationYInDegrees() {
            return owner.getVisualRotationYInDegrees();
        }
        public LivingEntity asLivingEntity() {
            return owner.asLivingEntity();
        }
    }
}
