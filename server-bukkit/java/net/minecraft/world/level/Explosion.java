// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import net.minecraft.world.entity.item.PrimedTnt;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.phys.Vec3;
public interface Explosion {
    static DamageSource getDefaultDamageSource(Level level, Entity source) {
        return level.damageSources().explosion(source, getIndirectSourceEntity(source));
    }
    static LivingEntity getIndirectSourceEntity(Entity source) {
        if (source instanceof PrimedTnt tnt) {
            return tnt.getOwner();
        }
        if (source instanceof LivingEntity living) {
            return living;
        }
        return source instanceof Projectile shot && shot.getOwner() instanceof LivingEntity owner ? owner : null;
    }
    ServerLevel level();
    BlockInteraction getBlockInteraction();
    LivingEntity getIndirectSourceEntity();
    Entity getDirectSourceEntity();
    float radius();
    Vec3 center();
    boolean canTriggerBlocks();
    boolean shouldAffectBlocklikeEntities();
    public static enum BlockInteraction {
KEEP(false), DESTROY(true), DESTROY_WITH_DECAY(true), TRIGGER_BLOCK(false);
        private final boolean affectsBlocklike;
        BlockInteraction(boolean affectsBlocklike) {
            this.affectsBlocklike = affectsBlocklike;
        }
        public boolean shouldAffectBlocklikeEntities() {
        return affectsBlocklike;
        }
    }
}
