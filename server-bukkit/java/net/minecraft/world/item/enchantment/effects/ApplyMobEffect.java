// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.phys.Vec3;
public record ApplyMobEffect(HolderSet<MobEffect> toApply, LevelBasedValue minDuration, LevelBasedValue maxDuration, LevelBasedValue minAmplifier, LevelBasedValue maxAmplifier) implements EnchantmentEntityEffect {
    public static final MapCodec<ApplyMobEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.ApplyMobEffect.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        if (!(entity instanceof net.minecraft.world.entity.LivingEntity living)) {
            return;
        }
        var random = living.getRandom();
        var chosen = toApply.getRandomElement(random);
        if (chosen.isEmpty()) {
            return;
        }
        float seconds = net.minecraft.util.Mth.randomBetween(random, minDuration.calculate(enchantmentLevel), maxDuration.calculate(enchantmentLevel));
        float level = net.minecraft.util.Mth.randomBetween(random, minAmplifier.calculate(enchantmentLevel), maxAmplifier.calculate(enchantmentLevel));
        living.addEffect(new net.minecraft.world.effect.MobEffectInstance(chosen.get(), Math.round(seconds * 20.0F), Math.max(0, Math.round(level))));
    }
    public MapCodec<ApplyMobEffect> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
