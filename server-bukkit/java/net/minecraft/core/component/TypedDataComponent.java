// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;

import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import java.util.Map;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

public record TypedDataComponent<T>(DataComponentType<T> type, T value) {
    public static final StreamCodec<RegistryFriendlyByteBuf, TypedDataComponent<?>> STREAM_CODEC = new StreamCodec<RegistryFriendlyByteBuf, TypedDataComponent<?>>() {
        @Override
        @SuppressWarnings({"unchecked", "rawtypes"})
        public TypedDataComponent<?> decode(RegistryFriendlyByteBuf input) {
            DataComponentType type = DataComponentType.STREAM_CODEC.decode(input);
            return new TypedDataComponent(type, type.streamCodec().decode(input));
        }

        @Override
        @SuppressWarnings({"unchecked", "rawtypes"})
        public void encode(RegistryFriendlyByteBuf output, TypedDataComponent<?> component) {
            DataComponentType.STREAM_CODEC.encode(output, component.type());
            ((StreamCodec) component.type().streamCodec()).encode(output, component.value());
        }
    };

    @SuppressWarnings({"unchecked", "rawtypes"})
    static TypedDataComponent<?> fromEntryUnchecked(Map.Entry<DataComponentType<?>, Object> entry) {
        return createUnchecked((DataComponentType) entry.getKey(), entry.getValue());
    }

    @SuppressWarnings("unchecked")
    public static <T> TypedDataComponent<T> createUnchecked(DataComponentType<T> type, Object value) {
        return new TypedDataComponent<>(type, (T) value);
    }

    public void applyTo(PatchedDataComponentMap components) {
        components.set(type, value);
    }

    public <D> DataResult<D> encodeValue(DynamicOps<D> ops) {
        com.mojang.serialization.Codec<T> codec = type.codec();
        return codec == null ? DataResult.error(() -> "Component of type " + type + " is not encodable")
            : codec.encodeStart(ops, value);
    }

    @Override
    public String toString() {
        return type + "=>" + value;
    }
}
