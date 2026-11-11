// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.throwableitemprojectile;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.HitResult;
public class ThrownExperienceBottle extends ThrowableItemProjectile {
    public ThrownExperienceBottle(EntityType<? extends ThrownExperienceBottle> type, Level level) {
        super(type, level);
    }
    public ThrownExperienceBottle(Level level, LivingEntity mob, ItemStack itemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public ThrownExperienceBottle(Level level, double x, double y, double z, ItemStack itemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected Item getDefaultItem() {
        return net.minecraft.world.item.Items.EXPERIENCE_BOTTLE;
    }
    protected double getDefaultGravity() {
        return super.getDefaultGravity();
    }
    protected void onHit(HitResult hitResult) {
        }
}
