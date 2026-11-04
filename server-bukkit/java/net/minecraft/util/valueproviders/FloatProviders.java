// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.core.Registry;

public class FloatProviders {
    private static final Map<String, java.util.function.Supplier<MapCodec<? extends FloatProvider>>> kTypes = new LinkedHashMap<>();
    public static final Codec<FloatProvider> CODEC;

    static {
        kTypes.put("constant", () -> ConstantFloat.MAP_CODEC);
        kTypes.put("uniform", () -> UniformFloat.MAP_CODEC);
        kTypes.put("clamped_normal", () -> ClampedNormalFloat.MAP_CODEC);
        kTypes.put("trapezoid", () -> TrapezoidFloat.MAP_CODEC);
        CODEC = ProviderTypes.codec(Codec.FLOAT, ConstantFloat::of, ConstantFloat.class, ConstantFloat::value, kTypes, FloatProvider::codec);
    }

    public static Codec<FloatProvider> codec(float minValue, float maxValue) {
        return CODEC.validate(value -> {
            if (value.min() < minValue) {
                return DataResult.error(() -> "Value provider too low: " + minValue + " [" + value.min() + "-" + value.max() + "]");
            }
            return value.max() > maxValue ? DataResult.error(() -> "Value provider too high: " + maxValue + " [" + value.min() + "-" + value.max() + "]")
                : DataResult.success(value);
        });
    }

    public static Codec<FloatProvider> codec(float minValue) {
        return CODEC.validate(value -> value.min() < minValue
            ? DataResult.error(() -> "Value provider too low: " + minValue + " [" + value.min() + "-" + value.max() + "]") : DataResult.success(value));
    }

    public static MapCodec<? extends FloatProvider> bootstrap(Registry<MapCodec<? extends FloatProvider>> registry) {
        kTypes.forEach((name, codec) -> Registry.register(registry, name, codec.get()));
        return TrapezoidFloat.MAP_CODEC;
    }
}
