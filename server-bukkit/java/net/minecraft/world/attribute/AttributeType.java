// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.Codec;
import java.util.Map;
import java.util.function.ToIntFunction;
import net.minecraft.util.ToFloatFunction;
import net.minecraft.world.attribute.modifier.AttributeModifier;

public record AttributeType<Value>(Codec<Value> valueCodec, Map<AttributeModifier.OperationId, AttributeModifier<Value, ?>> modifierLibrary, Codec<AttributeModifier<Value, ?>> modifierCodec, LerpFunction<Value> keyframeLerp, LerpFunction<Value> stateChangeLerp, LerpFunction<Value> spatialLerp, LerpFunction<Value> partialTickLerp, ToFloatFunction<Value> toFloat, ToIntFunction<Value> toInt) {
    private static final ToFloatFunction<Object> kNoFloat = value -> {
        throw new UnsupportedOperationException("this attribute type has no float form");
    };
    private static final ToIntFunction<Object> kNoInt = value -> {
        throw new UnsupportedOperationException("this attribute type has no int form");
    };

    public static <Value> AttributeType<Value> ofInterpolated(Codec<Value> valueCodec, Map<AttributeModifier.OperationId, AttributeModifier<Value, ?>> modifierLibrary, LerpFunction<Value> lerp) {
        return ofInterpolated(valueCodec, modifierLibrary, lerp, lerp, castFloat(), castInt());
    }

    public static <Value> AttributeType<Value> ofInterpolated(Codec<Value> valueCodec, Map<AttributeModifier.OperationId, AttributeModifier<Value, ?>> modifierLibrary, LerpFunction<Value> lerp, LerpFunction<Value> partialTickLerp, ToFloatFunction<Value> toFloat, ToIntFunction<Value> toInt) {
        return new AttributeType<>(valueCodec, modifierLibrary, modifierCodecOf(modifierLibrary), lerp, lerp, lerp, partialTickLerp, toFloat, toInt);
    }

    public static <Value> AttributeType<Value> ofNotInterpolated(Codec<Value> valueCodec, Map<AttributeModifier.OperationId, AttributeModifier<Value, ?>> modifierLibrary) {
        LerpFunction<Value> step = LerpFunction.ofStep(0.5F);
        return new AttributeType<>(valueCodec, modifierLibrary, modifierCodecOf(modifierLibrary), LerpFunction.ofConstant(), step, LerpFunction.ofConstant(), LerpFunction.ofConstant(), castFloat(), castInt());
    }

    public static <Value> AttributeType<Value> ofNotInterpolated(Codec<Value> valueCodec) {
        return ofNotInterpolated(valueCodec, Map.of(AttributeModifier.OperationId.OVERRIDE, AttributeModifier.override()));
    }

    @SuppressWarnings("unchecked")
    private static <Value> ToFloatFunction<Value> castFloat() {
        return (ToFloatFunction<Value>) kNoFloat;
    }

    @SuppressWarnings("unchecked")
    private static <Value> ToIntFunction<Value> castInt() {
        return (ToIntFunction<Value>) kNoInt;
    }

    private static <Value> Codec<AttributeModifier<Value, ?>> modifierCodecOf(Map<AttributeModifier.OperationId, AttributeModifier<Value, ?>> library) {
        return AttributeModifier.OperationId.CODEC.flatXmap(
            id -> library.containsKey(id)
                ? com.mojang.serialization.DataResult.success(library.get(id))
                : com.mojang.serialization.DataResult.error(() -> "unsupported operation " + id),
            modifier -> library.entrySet().stream().filter(entry -> entry.getValue() == modifier).findFirst()
                .map(entry -> com.mojang.serialization.DataResult.success(entry.getKey()))
                .orElseGet(() -> com.mojang.serialization.DataResult.error(() -> "modifier is not part of the library")));
    }

    public void checkAllowedModifier(AttributeModifier<Value, ?> modifier) {
        if (!modifierLibrary.containsValue(modifier)) {
            throw new IllegalArgumentException("Modifier " + modifier + " is not valid for " + this);
        }
    }

    public float toFloat(Value value) {
        return toFloat.applyAsFloat(value);
    }

    public int toInt(Value value) {
        return toInt.applyAsInt(value);
    }
}
