// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.fish;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public class Cod extends AbstractSchoolingFish {
    public Cod(EntityType<? extends Cod> type, Level level) {
        super(type, level);
    }
    public ItemStack getBucketItemStack() {
        return new ItemStack(net.minecraft.world.item.Items.COD_BUCKET);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.cod.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.cod.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.cod.hurt");
    }
    protected SoundEvent getFlopSound() {
        return null;
    }
}
