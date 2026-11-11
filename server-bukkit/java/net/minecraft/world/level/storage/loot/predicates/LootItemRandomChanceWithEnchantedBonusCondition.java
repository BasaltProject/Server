// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.item.enchantment.Enchantments;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.level.storage.loot.LootContext;
public record LootItemRandomChanceWithEnchantedBonusCondition(float unenchantedChance, LevelBasedValue enchantedChance, Holder<Enchantment> enchantment) implements LootItemCondition {
    public static final MapCodec<LootItemRandomChanceWithEnchantedBonusCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.LootItemRandomChanceWithEnchantedBonusCondition.class, "MAP_CODEC");
    public MapCodec<LootItemRandomChanceWithEnchantedBonusCondition> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.ATTACKING_ENTITY);
    }
    public boolean test(LootContext context) {
        int level = context.getOptional(LootContextParams.ATTACKING_ENTITY) instanceof LivingEntity killer
            ? EnchantmentHelper.getEnchantmentLevel(enchantment, killer) : 0;
        float chance = level > 0 ? enchantedChance.calculate(level) : unenchantedChance;
        return context.getRandom().nextFloat() < chance;
    }
    public static LootItemCondition.Builder randomChanceAndLootingBoost(HolderGetter<Enchantment> enchantments, float chance, float perEnchantmentLevel) {
        return () -> new LootItemRandomChanceWithEnchantedBonusCondition(chance, new LevelBasedValue.Linear(chance + perEnchantmentLevel, perEnchantmentLevel),
            enchantments.getOrThrow(Enchantments.LOOTING));
    }
}
