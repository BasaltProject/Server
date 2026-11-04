package org.basaltmc.bukkit.facade;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.MapLike;
import com.mojang.serialization.RecordBuilder;
import java.lang.reflect.Field;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.Type;
import java.util.stream.Stream;
import net.minecraft.network.codec.StreamCodec;

/** Fills the codec constants of the facade: derived from the declared type on first use, or a clear error when it cannot be. */
public final class LazyCodecs {
    private LazyCodecs() {}

    public static <A> Codec<A> codec(Class<?> owner, String field) {
        return new Lazy<>(owner, field);
    }

    public static <A> MapCodec<A> mapCodec(Class<?> owner, String field) {
        return new LazyMap<>(owner, field);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <B, V> StreamCodec<B, V> streamCodec(Class<?> owner, String field) {
        String name = owner.getName() + "." + field;
        java.util.function.Supplier<StreamCodec<B, V>> resolver = new java.util.function.Supplier<>() {
            private StreamCodec<B, V> resolved;

            @Override
            public synchronized StreamCodec<B, V> get() {
                if (resolved == null) {
                    resolved = (StreamCodec<B, V>) deriveStream(owner, field);
                    if (resolved == null) {
                        throw new UnsupportedOperationException(name + " is not assembled by Basalt");
                    }
                }
                return resolved;
            }
        };
        return StreamCodec.of((buf, value) -> resolver.get().encode(buf, value), buf -> resolver.get().decode(buf));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static StreamCodec<?, ?> deriveStream(Class<?> owner, String field) {
        try {
            StreamCodec<?, ?> specified = StreamSpecs.forClass(owner, field);
            if (specified != null) {
                return specified;
            }
            Type generic = owner.getDeclaredField(field).getGenericType();
            if (generic instanceof ParameterizedType parameterized && parameterized.getActualTypeArguments().length == 2
                && parameterized.getActualTypeArguments()[1] instanceof ParameterizedType value
                && value.getRawType() == net.minecraft.core.Holder.class
                && value.getActualTypeArguments()[0] instanceof Class<?> held) {
                return net.minecraft.network.codec.ByteBufCodecs.holderRegistry(
                    (net.minecraft.resources.ResourceKey) HolderRefs.registryKey(held));
            }
            if (generic instanceof ParameterizedType parameterized && parameterized.getActualTypeArguments().length == 2
                && parameterized.getActualTypeArguments()[1] instanceof Class<?> value && value.isEnum()) {
                Object[] constants = value.getEnumConstants();
                return net.minecraft.network.codec.ByteBufCodecs.idMapper(id -> constants[Math.floorMod(id, constants.length)],
                    constant -> ((Enum<?>) constant).ordinal());
            }
        } catch (ReflectiveOperationException | RuntimeException | LinkageError failure) {
            return null;
        }
        return null;
    }

    private static Codec<?> derive(Class<?> owner, String field) {
        try {
            Field declared = owner.getDeclaredField(field);
            Type generic = declared.getGenericType();
            if (generic instanceof ParameterizedType parameterized) {
                return ReflectiveCodecs.forType(parameterized.getActualTypeArguments()[0]);
            }
        } catch (ReflectiveOperationException | RuntimeException | LinkageError failure) {
            return null;
        }
        return null;
    }

    public static final class Lazy<A> implements Codec<A> {
        private final Class<?> owner;
        private final String field;
        private volatile Codec<A> resolved;

        private Lazy(Class<?> owner, String field) {
            this.owner = owner;
            this.field = field;
        }

        @SuppressWarnings("unchecked")
        private Codec<A> resolve() {
            Codec<A> codec = resolved;
            if (codec == null) {
                codec = (Codec<A>) derive(owner, field);
                resolved = codec;
            }
            return codec;
        }

        private String name() {
            return owner.getName() + "." + field;
        }

        @Override
        public <T> DataResult<T> encode(A input, DynamicOps<T> ops, T prefix) {
            Codec<A> codec = resolve();
            return codec == null ? DataResult.error(() -> name() + " is not assembled by Basalt") : codec.encode(input, ops, prefix);
        }

        @Override
        public <T> DataResult<Pair<A, T>> decode(DynamicOps<T> ops, T input) {
            Codec<A> codec = resolve();
            return codec == null ? DataResult.error(() -> name() + " is not assembled by Basalt") : codec.decode(ops, input);
        }

        @Override
        public String toString() {
            return "Lazy[" + name() + "]";
        }
    }

    private static final class LazyMap<A> extends MapCodec<A> {
        private final Lazy<A> codec;
        private volatile MapCodec<A> resolved;

        private LazyMap(Class<?> owner, String field) {
            this.codec = new Lazy<>(owner, field);
        }

        private MapCodec<A> resolve() {
            MapCodec<A> map = resolved;
            if (map == null) {
                map = MapCodec.assumeMapUnsafe(codec);
                resolved = map;
            }
            return map;
        }

        @Override
        public <T> Stream<T> keys(DynamicOps<T> ops) {
            return resolve().keys(ops);
        }

        @Override
        public <T> DataResult<A> decode(DynamicOps<T> ops, MapLike<T> input) {
            return resolve().decode(ops, input);
        }

        @Override
        public <T> RecordBuilder<T> encode(A input, DynamicOps<T> ops, RecordBuilder<T> prefix) {
            return resolve().encode(input, ops, prefix);
        }

        @Override
        public String toString() {
            return "LazyMap[" + codec + "]";
        }
    }
}
