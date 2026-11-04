// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;

import com.mojang.serialization.Codec;
import java.util.Map;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

public interface DataComponentType<T> {
    Codec<DataComponentType<?>> CODEC = net.minecraft.resources.Identifier.CODEC.flatXmap(id -> {
        DataComponentType<?> found = org.basaltmc.bukkit.facade.ComponentTypes.byId(id);
        return found == null ? com.mojang.serialization.DataResult.error(() -> "Unknown component " + id)
            : com.mojang.serialization.DataResult.success(found);
    }, type -> com.mojang.serialization.DataResult.success(org.basaltmc.bukkit.facade.ComponentTypes.idOf(type)));
    StreamCodec<RegistryFriendlyByteBuf, DataComponentType<?>> STREAM_CODEC = StreamCodec.of(
        (buf, type) -> buf.writeVarInt(DataComponents.registry().getId(type)),
        buf -> DataComponents.registry().byId(buf.readVarInt()));
    Codec<DataComponentType<?>> PERSISTENT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.core.component.DataComponentType.class, "PERSISTENT_CODEC");
    Codec<Map<DataComponentType<?>, Object>> VALUE_MAP_CODEC = org.basaltmc.bukkit.facade.ComponentValueCodecs.valueMap(CODEC);

    static <T> Builder<T> builder() {
        return new Builder<>();
    }

    Codec<T> codec();

    default Codec<T> codecOrThrow() {
        Codec<T> codec = codec();
        if (codec == null) {
            throw new IllegalStateException(this + " is not a persistent component");
        }
        return codec;
    }

    default boolean isTransient() {
        return codec() == null;
    }

    boolean ignoreSwapAnimation();

    StreamCodec<? super RegistryFriendlyByteBuf, T> streamCodec();

    public static class Builder<T> {
        private Codec<T> codec;
        private StreamCodec<? super RegistryFriendlyByteBuf, T> streamCodec;
        private boolean ignoreSwapAnimation;

        public Builder<T> persistent(Codec<T> codec) {
            this.codec = codec;
            return this;
        }

        public Builder<T> networkSynchronized(StreamCodec<? super RegistryFriendlyByteBuf, T> streamCodec) {
            this.streamCodec = streamCodec;
            return this;
        }

        public Builder<T> cacheEncoding() {
            return this;
        }

        public DataComponentType<T> build() {
            return new SimpleType<>(null, codec, streamCodec, ignoreSwapAnimation);
        }

        public DataComponentType<T> build(String name) {
            return new SimpleType<>(name, codec, streamCodec, ignoreSwapAnimation);
        }

        public DataComponentType<T> buildReflective(String name, java.lang.reflect.Type valueType) {
            SimpleType<T> type = new SimpleType<>(name, codec, streamCodec, ignoreSwapAnimation);
            type.valueType = valueType;
            return type;
        }

        public static void attach(DataComponentType<?> type, java.lang.reflect.Type valueType) {
            if (type instanceof SimpleType<?> simple && simple.valueType == null) {
                simple.valueType = valueType;
            }
        }

        public Builder<T> ignoreSwapAnimation() {
            this.ignoreSwapAnimation = true;
            return this;
        }

        static class SimpleType<T> implements DataComponentType<T> {
            private final String name;
            private Codec<T> codec;
            private java.lang.reflect.Type valueType;
            private boolean resolved;
            private final StreamCodec<? super RegistryFriendlyByteBuf, T> streamCodec;
            private final boolean ignoreSwapAnimation;

            private SimpleType(String name, Codec<T> codec, StreamCodec<? super RegistryFriendlyByteBuf, T> streamCodec,
                    boolean ignoreSwapAnimation) {
                this.name = name;
                this.codec = codec;
                this.streamCodec = streamCodec;
                this.ignoreSwapAnimation = ignoreSwapAnimation;
            }

            @Override
            public boolean ignoreSwapAnimation() {
                return ignoreSwapAnimation;
            }

            @Override
            @SuppressWarnings("unchecked")
            public synchronized Codec<T> codec() {
                if (codec == null && !resolved && valueType != null) {
                    resolved = true;
                    codec = (Codec<T>) org.basaltmc.bukkit.facade.ReflectiveCodecs.forType(valueType);
                }
                return codec;
            }

            @Override
            public StreamCodec<? super RegistryFriendlyByteBuf, T> streamCodec() {
                return streamCodec;
            }

            @Override
            public String toString() {
                return name == null ? "component@" + Integer.toHexString(System.identityHashCode(this)) : name;
            }
        }
    }
}
