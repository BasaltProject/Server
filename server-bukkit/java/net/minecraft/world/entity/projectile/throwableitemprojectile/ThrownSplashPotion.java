// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.throwableitemprojectile;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.projectile.ProjectileUtil;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.alchemy.PotionContents;
import net.minecraft.world.phys.AABB;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.HitResult;
public class ThrownSplashPotion extends AbstractThrownPotion {
    public ThrownSplashPotion(EntityType<? extends ThrownSplashPotion> type, Level level) {
        super(type, level);
    }
    public ThrownSplashPotion(Level level, LivingEntity owner, ItemStack itemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public ThrownSplashPotion(Level level, double x, double y, double z, ItemStack itemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected Item getDefaultItem() {
        return Items.SPLASH_POTION;
    }
    public void onHitAsPotion(ServerLevel level, ItemStack potionItem, HitResult hitResult) {
        PotionContents contents = potionItem.getOrDefault(DataComponents.POTION_CONTENTS, PotionContents.EMPTY);
        float durationScale = potionItem.getOrDefault(DataComponents.POTION_DURATION_SCALE, 1.0F);
        AABB core = getBoundingBox().move(hitResult.getLocation().subtract(position()));
        float margin = ProjectileUtil.computeMargin(this);
        for (LivingEntity victim : level().getEntitiesOfClass(LivingEntity.class, core.inflate(4.0, 2.0, 4.0))) {
            double distSqr = core.distanceToSqr(victim.getBoundingBox().inflate(margin));
            if (!victim.isAffectedByPotions() || distSqr >= 16.0) {
                continue;
            }
            double scale = 1.0 - Math.sqrt(distSqr) / 4.0;
            for (MobEffectInstance instance : contents.getAllEffects()) {
                Holder<MobEffect> effect = instance.getEffect();
                if (effect.value().isInstantaneous()) {
                    effect.value().applyInstantaneousEffect(level, this, getOwner(), victim, instance.getAmplifier(), scale);
                } else {
                    int ticks = instance.mapDuration(base -> (int) (scale * base * durationScale + 0.5));
                    if (ticks > 20) {
                        victim.addEffect(new MobEffectInstance(effect, ticks, instance.getAmplifier(), instance.isAmbient(), instance.isVisible()), getOwner());
                    }
                }
            }
        }
    }
}
