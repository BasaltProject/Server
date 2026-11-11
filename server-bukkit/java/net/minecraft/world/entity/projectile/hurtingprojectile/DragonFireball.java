// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.hurtingprojectile;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;
public class DragonFireball extends AbstractHurtingProjectile {
    public static final float SPLASH_RANGE = 4.0F;
    public DragonFireball(EntityType<? extends DragonFireball> type, Level level) {
        super(type, level);
    }
    public DragonFireball(Level level, LivingEntity mob, Vec3 direction) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected void onHit(HitResult hitResult) {
        }
    protected ParticleOptions getTrailParticle() {
        return net.minecraft.core.particles.PowerParticleOption.create(net.minecraft.core.particles.ParticleTypes.DRAGON_BREATH, 1.0F);
    }
    protected boolean shouldBurn() {
        return false;
    }
}
