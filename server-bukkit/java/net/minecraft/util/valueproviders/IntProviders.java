// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.core.Registry;

public class IntProviders {
    private static final Map<String, java.util.function.Supplier<MapCodec<? extends IntProvider>>> kTypes = new LinkedHashMap<>();
    public static final Codec<IntProvider> CODEC;
    public static final Codec<IntProvider> NON_NEGATIVE_CODEC;
    public static final Codec<IntProvider> POSITIVE_CODEC;

    static {
        kTypes.put("constant", () -> ConstantInt.MAP_CODEC);
        kTypes.put("uniform", () -> UniformInt.MAP_CODEC);
        kTypes.put("biased_to_bottom", () -> BiasedToBottomInt.MAP_CODEC);
        kTypes.put("very_biased_to_bottom", () -> VeryBiasedToBottomInt.MAP_CODEC);
        kTypes.put("clamped", () -> ClampedInt.MAP_CODEC);
        kTypes.put("weighted_list", () -> WeightedListInt.MAP_CODEC);
        kTypes.put("clamped_normal", () -> ClampedNormalInt.MAP_CODEC);
        kTypes.put("trapezoid", () -> TrapezoidInt.MAP_CODEC);
        CODEC = ProviderTypes.codec(Codec.INT, ConstantInt::of, ConstantInt.class, ConstantInt::value, kTypes, IntProvider::codec);
        NON_NEGATIVE_CODEC = codec(0, Integer.MAX_VALUE);
        POSITIVE_CODEC = codec(1, Integer.MAX_VALUE);
    }

    public static Codec<IntProvider> codec(int minValue, int maxValue) {
        return validateCodec(minValue, maxValue, CODEC);
    }

    public static <T extends IntProvider> Codec<T> validateCodec(int minValue, int maxValue, Codec<T> codec) {
        return codec.validate(value -> validate(minValue, maxValue, value));
    }

    private static <T extends IntProvider> DataResult<T> validate(int minValue, int maxValue, T value) {
        if (value.minInclusive() < minValue) {
            return DataResult.error(() -> "Value provider too low: " + minValue + " [" + value.minInclusive() + "-" + value.maxInclusive() + "]");
        }
        if (value.maxInclusive() > maxValue) {
            return DataResult.error(() -> "Value provider too high: " + maxValue + " [" + value.minInclusive() + "-" + value.maxInclusive() + "]");
        }
        return DataResult.success(value);
    }

    public static MapCodec<? extends IntProvider> bootstrap(Registry<MapCodec<? extends IntProvider>> registry) {
        kTypes.forEach((name, codec) -> Registry.register(registry, name, codec.get()));
        return TrapezoidInt.MAP_CODEC;
    }
}
