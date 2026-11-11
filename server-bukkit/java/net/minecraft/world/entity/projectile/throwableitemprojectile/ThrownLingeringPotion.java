// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.throwableitemprojectile;
import net.minecraft.world.item.Items;
import net.minecraft.world.entity.AreaEffectCloud;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.HitResult;
public class ThrownLingeringPotion extends AbstractThrownPotion {
    public ThrownLingeringPotion(EntityType<? extends ThrownLingeringPotion> type, Level level) {
        super(type, level);
    }
    public ThrownLingeringPotion(Level level, LivingEntity owner, ItemStack itemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public ThrownLingeringPotion(Level level, double x, double y, double z, ItemStack itemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected Item getDefaultItem() {
        return Items.LINGERING_POTION;
    }
    public void onHitAsPotion(ServerLevel level, ItemStack potionItem, HitResult hitResult) {
        AreaEffectCloud cloud = hitResult instanceof EntityHitResult hit
            ? new AreaEffectCloud(level(), hit.getEntity().getX(), hit.getEntity().getY(), hit.getEntity().getZ())
            : new AreaEffectCloud(level(), getX(), getY(), getZ());
        if (getOwner() instanceof LivingEntity owner) {
            cloud.setOwner(owner);
        }
        cloud.setRadius(3.0F);
        cloud.setRadiusOnUse(-0.5F);
        cloud.setDuration(600);
        cloud.setWaitTime(10);
        cloud.setRadiusPerTick(-cloud.getRadius() / cloud.getDuration());
        cloud.applyComponentsFromItemStack(potionItem);
        level.addFreshEntity(cloud);
    }
}
