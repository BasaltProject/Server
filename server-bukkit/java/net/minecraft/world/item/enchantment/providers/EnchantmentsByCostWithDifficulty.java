// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.providers;
import net.minecraft.util.Mth;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.item.enchantment.EnchantmentInstance;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.ItemEnchantments;
public record EnchantmentsByCostWithDifficulty(HolderSet<Enchantment> enchantments, int minCost, int maxCostSpan) implements EnchantmentProvider {
    public static final int MAX_ALLOWED_VALUE_PART = 10000;
    public static final MapCodec<EnchantmentsByCostWithDifficulty> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.providers.EnchantmentsByCostWithDifficulty.class, "CODEC");
    public void enchant(ItemStack item, ItemEnchantments.Mutable itemEnchantments, RandomSource random, DifficultyInstance difficulty) {
        int top = minCost + (int) (difficulty.getSpecialMultiplier() * maxCostSpan);
        int budget = Mth.randomBetweenInclusive(random, minCost, top);
        for (EnchantmentInstance pick : EnchantmentHelper.selectEnchantment(random, item, budget, enchantments.stream())) {
            itemEnchantments.upgrade(pick.enchantment(), pick.level());
        }
    }
    public MapCodec<EnchantmentsByCostWithDifficulty> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
