// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.skeleton;
import net.minecraft.world.entity.projectile.arrow.Arrow;
import net.minecraft.world.effect.MobEffects;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.projectile.arrow.AbstractArrow;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public class Parched extends AbstractSkeleton {
    public Parched(EntityType<? extends AbstractSkeleton> type, Level level) {
        super(type, level);
    }
    protected AbstractArrow getArrow(ItemStack projectile, float power, ItemStack firingWeapon) {
        AbstractArrow arrow = super.getArrow(projectile, power, firingWeapon);
        if (arrow instanceof Arrow tipped) {
            tipped.addEffect(new MobEffectInstance(MobEffects.WEAKNESS, 600));
        }
        return arrow;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    protected SoundEvent getStepSound() {
        return null;
    }
    protected int getHardAttackInterval() {
        return 0;
    }
    protected int getAttackInterval() {
        return 0;
    }
    public boolean canBeAffected(MobEffectInstance newEffect) {
        return super.canBeAffected(newEffect);
    }
}
