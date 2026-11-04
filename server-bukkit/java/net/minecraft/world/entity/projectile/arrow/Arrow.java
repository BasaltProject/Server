// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.arrow;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.alchemy.PotionContents;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public class Arrow extends AbstractArrow {
    public Arrow(EntityType<? extends Arrow> type, Level level) {
        super(type, level);
    }
    public Arrow(Level level, double x, double y, double z, ItemStack pickupItemStack, ItemStack firedFromWeapon) {
        super(net.minecraft.world.entity.EntityTypes.ARROW, x, y, z, level, pickupItemStack, firedFromWeapon);
    }
    public Arrow(Level level, LivingEntity owner, ItemStack pickupItemStack, ItemStack firedFromWeapon) {
        super(net.minecraft.world.entity.EntityTypes.ARROW, owner, level, pickupItemStack, firedFromWeapon);
    }
    protected void setPickupItemStack(ItemStack itemStack) {
        super.setPickupItemStack(itemStack);
        PotionContents contents = itemStack.get(net.minecraft.core.component.DataComponents.POTION_CONTENTS);
        potion = contents == null ? PotionContents.EMPTY : contents;
    }
    public void addEffect(MobEffectInstance effect) {
        potion = potion.withEffectAdded(effect);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void tick() {
    }
    public int getColor() {
        if (live() instanceof org.bukkit.entity.Arrow api) {
            org.bukkit.Color color = api.getColor();
            return color == null ? -1 : color.asRGB();
        }
        return potion.equals(PotionContents.EMPTY) ? -1 : potion.getColor();
    }
    protected void doPostHurtEffects(LivingEntity mob) {
        super.doPostHurtEffects(mob);
        Entity source = getEffectSource();
        potion.forEachEffect(effect -> mob.addEffect(effect, source), 1.0F);
    }
    protected ItemStack getDefaultPickupItem() {
        return new ItemStack(net.minecraft.world.item.Items.ARROW);
    }
    public void handleEntityEvent(byte id) {
    }
    private PotionContents potion = PotionContents.EMPTY;

}
