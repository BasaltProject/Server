// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import java.util.function.Function;
import net.minecraft.core.registries.BuiltInRegistries;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Registry;
import net.minecraft.util.RandomSource;
public interface EnchantmentValueEffect {
    Codec<EnchantmentValueEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.effects.EnchantmentValueEffect.class, "CODEC");
    static MapCodec<? extends EnchantmentValueEffect> bootstrap(Registry<MapCodec<? extends EnchantmentValueEffect>> registry) {
        Registry.register(registry, "add", AddValue.CODEC);
        Registry.register(registry, "all_of", AllOf.ValueEffects.CODEC);
        Registry.register(registry, "multiply", MultiplyValue.CODEC);
        Registry.register(registry, "remove_binomial", RemoveBinomial.CODEC);
        Registry.register(registry, "exponential", ScaleExponentially.CODEC);
        return Registry.register(registry, "set", SetValue.CODEC);


    }
    float process(int enchantmentLevel, RandomSource random, float inputValue);
    MapCodec<? extends EnchantmentValueEffect> codec();
}
