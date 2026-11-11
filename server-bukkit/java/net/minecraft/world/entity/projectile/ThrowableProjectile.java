// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.level.Level;
public abstract class ThrowableProjectile extends Projectile {
    protected ThrowableProjectile(EntityType<? extends ThrowableProjectile> type, Level level) {
        super(type, level);
    }
    protected ThrowableProjectile(EntityType<? extends ThrowableProjectile> type, double x, double y, double z, Level level) {
        super(type, level);
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    public boolean canUsePortal(boolean ignorePassenger) {
        return true;
    }
    public void tick() {
    }
    protected float getAirDrag() {
        return live() instanceof org.bukkit.entity.ThrowableProjectile api ? (float) api.getAirDrag() : 0;
    }
    protected double getDefaultGravity() {
        return live() instanceof org.bukkit.entity.ThrowableProjectile api ? (double) api.getDefaultGravity() : 0;
    }
}
