// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.providers;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.ItemEnchantments;
public record SingleEnchantment(Holder<Enchantment> enchantment, IntProvider level) implements EnchantmentProvider {
    public static final MapCodec<SingleEnchantment> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.providers.SingleEnchantment.class, "CODEC");
    public void enchant(ItemStack item, ItemEnchantments.Mutable itemEnchantments, RandomSource random, DifficultyInstance difficulty) {
        int bounded = Mth.clamp(level.sample(random), enchantment.value().getMinLevel(), enchantment.value().getMaxLevel());
        itemEnchantments.upgrade(enchantment, bounded);
    }
    public MapCodec<SingleEnchantment> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
