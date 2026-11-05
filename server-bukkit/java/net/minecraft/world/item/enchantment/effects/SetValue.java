// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.LevelBasedValue;
public record SetValue(LevelBasedValue value) implements EnchantmentValueEffect {
    public static final MapCodec<SetValue> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.SetValue.class, "CODEC");
    public float process(int enchantmentLevel, RandomSource random, float inputValue) {
        return value.calculate(enchantmentLevel);
    }
    public MapCodec<SetValue> codec() {
        return CODEC;
    }
}
