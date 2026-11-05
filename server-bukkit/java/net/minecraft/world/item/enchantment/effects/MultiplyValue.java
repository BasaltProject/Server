// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.LevelBasedValue;
public record MultiplyValue(LevelBasedValue factor) implements EnchantmentValueEffect {
    public static final MapCodec<MultiplyValue> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.MultiplyValue.class, "CODEC");
    public float process(int enchantmentLevel, RandomSource random, float inputValue) {
        return inputValue * factor.calculate(enchantmentLevel);
    }
    public MapCodec<MultiplyValue> codec() {
        return CODEC;
    }
}
