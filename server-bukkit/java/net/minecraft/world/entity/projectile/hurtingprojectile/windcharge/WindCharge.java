// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.hurtingprojectile.windcharge;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.ProjectileDeflection;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;
public class WindCharge extends AbstractWindCharge {
    private int noDeflectTicks = 5;
    public WindCharge(EntityType<? extends AbstractWindCharge> type, Level level) {
        super(type, level);
    }
    public WindCharge(Player player, Level level, double x, double y, double z) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public WindCharge(Level level, double x, double y, double z, Vec3 direction) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public void tick() {
    }
    public boolean deflect(ProjectileDeflection deflection, Entity deflectingEntity, EntityReference<Entity> newOwner, boolean byAttack, Vec3 power) {
        return noDeflectTicks > 0 ? false : super.deflect(deflection, deflectingEntity, newOwner, byAttack, power);


    }
    protected void explode(Vec3 position) {
        }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
}
