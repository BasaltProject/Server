// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.LevelBasedValue;
public record ScaleExponentially(LevelBasedValue base, LevelBasedValue exponent) implements EnchantmentValueEffect {
    public static final MapCodec<ScaleExponentially> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.ScaleExponentially.class, "CODEC");
    public float process(int enchantmentLevel, RandomSource random, float inputValue) {
        return inputValue * (float) Math.pow(base.calculate(enchantmentLevel), exponent.calculate(enchantmentLevel));
    }
    public MapCodec<ScaleExponentially> codec() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.effects.ScaleExponentially#codec");
    }
}
