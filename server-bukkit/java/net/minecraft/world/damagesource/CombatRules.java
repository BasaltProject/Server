// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.damagesource;
import net.minecraft.util.Mth;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.entity.LivingEntity;
public class CombatRules {
    public static final float MAX_ARMOR = 20.0F;
    public static final float ARMOR_PROTECTION_DIVIDER = 25.0F;
    public static final float BASE_ARMOR_TOUGHNESS = 2.0F;
    public static final float MIN_ARMOR_RATIO = 0.2F;
    public static float getDamageAfterAbsorb(LivingEntity victim, float damage, DamageSource source, float totalArmor, float armorToughness) {
        float toughness = 2.0F + armorToughness / 4.0F;
        float effective = Mth.clamp(totalArmor - damage / toughness, totalArmor * 0.2F, 20.0F);
        float fraction = effective / 25.0F;
        ItemStack weapon = source.getWeaponItem();
        if (weapon != null && victim.level() instanceof ServerLevel serverLevel) {
            fraction = Mth.clamp(EnchantmentHelper.modifyArmorEffectiveness(serverLevel, weapon, victim, source, fraction), 0.0F, 1.0F);
        }
        return damage * (1.0F - fraction);
    }
    public static float getDamageAfterMagicAbsorb(float damage, float totalMagicArmor) {
        return damage * (1.0F - Mth.clamp(totalMagicArmor, 0.0F, 20.0F) / 25.0F);
    }
}
