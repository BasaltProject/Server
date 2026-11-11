// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.phys.Vec3;
public record DamageEntity(LevelBasedValue minDamage, LevelBasedValue maxDamage, Holder<DamageType> damageType) implements EnchantmentEntityEffect {
    public static final MapCodec<DamageEntity> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.DamageEntity.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        float amount = net.minecraft.util.Mth.randomBetween(entity.getRandom(), minDamage.calculate(enchantmentLevel), maxDamage.calculate(enchantmentLevel));
        entity.hurtServer(serverLevel, new net.minecraft.world.damagesource.DamageSource(damageType, item.owner()), amount);
    }
    public MapCodec<DamageEntity> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
