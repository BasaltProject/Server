// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.LevelBasedValue;
public record AddValue(LevelBasedValue value) implements EnchantmentValueEffect {
    public static final MapCodec<AddValue> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.AddValue.class, "CODEC");
    public float process(int enchantmentLevel, RandomSource random, float inputValue) {
        return inputValue + value.calculate(enchantmentLevel);
    }
    public MapCodec<AddValue> codec() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.effects.AddValue#codec");
    }
}
