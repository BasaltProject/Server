// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.arrow;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class SpectralArrow extends AbstractArrow {
    public SpectralArrow(EntityType<? extends SpectralArrow> type, Level level) {
        super(type, level);
    }
    public SpectralArrow(Level level, LivingEntity owner, ItemStack pickupItemStack, ItemStack firedFromWeapon) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public SpectralArrow(Level level, double x, double y, double z, ItemStack pickupItemStack, ItemStack firedFromWeapon) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public void tick() {
    }
    protected void doPostHurtEffects(LivingEntity mob) {
        }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected ItemStack getDefaultPickupItem() {
        return new net.minecraft.world.item.ItemStack(net.minecraft.world.item.Items.SPECTRAL_ARROW);
    }
}
