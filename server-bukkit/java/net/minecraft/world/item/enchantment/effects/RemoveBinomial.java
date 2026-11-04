// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.LevelBasedValue;
public record RemoveBinomial(LevelBasedValue chance) implements EnchantmentValueEffect {
    public static final MapCodec<RemoveBinomial> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.RemoveBinomial.class, "CODEC");
    public float process(int enchantmentLevel, RandomSource random, float inputValue) {
        float probability = chance.calculate(enchantmentLevel);
        int removed = 0;
        int trials = (int) inputValue;
        for (int i = 0; i < trials; i++) {
            if (random.nextFloat() < probability) {
                removed++;
            }
        }
        return inputValue - removed;
    }
    public MapCodec<RemoveBinomial> codec() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.effects.RemoveBinomial#codec");
    }
}
