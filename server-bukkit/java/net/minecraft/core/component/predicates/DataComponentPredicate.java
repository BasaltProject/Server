// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapCodec;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;

public interface DataComponentPredicate {
    Codec<Map<Type<?>, DataComponentPredicate>> CODEC = new Codec<>() {
        @Override
        @SuppressWarnings({"unchecked", "rawtypes"})
        public <T> DataResult<Pair<Map<Type<?>, DataComponentPredicate>, T>> decode(DynamicOps<T> ops, T input) {
            DataResult<java.util.stream.Stream<Pair<T, T>>> entries = ops.getMapValues(input);
            if (entries.error().isPresent()) {
                return DataResult.error(() -> "Not a map of predicates");
            }
            Map<Type<?>, DataComponentPredicate> out = new LinkedHashMap<>();
            for (Pair<T, T> entry : (Iterable<Pair<T, T>>) entries.result().get()::iterator) {
                DataResult<Type<?>> type = Type.CODEC.parse(ops, entry.getFirst());
                if (type.error().isPresent()) {
                    return DataResult.error(() -> "Unknown predicate type");
                }
                DataResult<?> parsed = ((Codec) type.result().get().codec()).parse(ops, entry.getSecond());
                if (parsed.error().isPresent()) {
                    return DataResult.error(() -> "Bad predicate for " + type.result().get());
                }
                out.put(type.result().get(), (DataComponentPredicate) parsed.result().get());
            }
            return DataResult.success(Pair.of(out, input));
        }

        @Override
        @SuppressWarnings({"unchecked", "rawtypes"})
        public <T> DataResult<T> encode(Map<Type<?>, DataComponentPredicate> input, DynamicOps<T> ops, T prefix) {
            Map<T, T> out = new LinkedHashMap<>();
            for (Map.Entry<Type<?>, DataComponentPredicate> entry : input.entrySet()) {
                DataResult<T> key = Type.CODEC.encodeStart(ops, entry.getKey());
                DataResult<T> value = ((Codec) entry.getKey().codec()).encodeStart(ops, entry.getValue());
                if (key.error().isPresent() || value.error().isPresent()) {
                    return DataResult.error(() -> "Cannot encode " + entry.getKey());
                }
                out.put(key.result().get(), value.result().get());
            }
            return DataResult.success(ops.createMap(out));
        }
    };
    StreamCodec<RegistryFriendlyByteBuf, Single<?>> SINGLE_STREAM_CODEC = StreamCodec.of(
        (buf, single) -> {
            Type.STREAM_CODEC.encode(buf, single.type());
            writeNbt(buf, single);
        }, buf -> {
            throw new UnsupportedOperationException("Predicate singles are decoded through the map codec");
        });
    StreamCodec<RegistryFriendlyByteBuf, Map<Type<?>, DataComponentPredicate>> STREAM_CODEC = ByteBufCodecs.fromCodecWithRegistries(CODEC);

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static void writeNbt(RegistryFriendlyByteBuf buf, Single<?> single) {
        ByteBufCodecs.TRUSTED_TAG.encode(buf, (net.minecraft.nbt.Tag) ((Codec) single.type().codec())
            .encodeStart(net.minecraft.nbt.NbtOps.INSTANCE, single.predicate()).result().orElseThrow());
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    static MapCodec<Single<?>> singleCodec(String name) {
        java.util.function.Function<Single<?>, Type<?>> typeOf = single -> single.type();
        java.util.function.Function<Type<?>, MapCodec<? extends Single<?>>> codecOf = type -> (MapCodec) type.wrappedCodec();
        return Type.CODEC.dispatchMap(name, typeOf, codecOf);
    }

    boolean matches(DataComponentGetter components);

    public interface Type<T extends DataComponentPredicate> {
        Codec<Type<?>> CODEC = Identifier.CODEC.flatXmap(id -> {
            Type<?> found = DataComponentPredicates.byId(id);
            return found == null ? DataResult.error(() -> "Unknown predicate type " + id) : DataResult.success(found);
        }, type -> {
            Identifier id = DataComponentPredicates.idOf(type);
            return id == null ? DataResult.error(() -> "Unregistered predicate type") : DataResult.success(id);
        });
        @SuppressWarnings({"unchecked", "rawtypes"})
        StreamCodec<RegistryFriendlyByteBuf, Type<?>> STREAM_CODEC = (StreamCodec) ByteBufCodecs.STRING_UTF8.map(
            text -> DataComponentPredicates.byId(Identifier.parse(text)), type -> DataComponentPredicates.idOf((Type<?>) type).toString());

        Codec<T> codec();

        MapCodec<Single<T>> wrappedCodec();

        StreamCodec<RegistryFriendlyByteBuf, Single<T>> singleStreamCodec();
    }

    public static abstract class TypeBase<T extends DataComponentPredicate> implements Type<T> {
        private final Codec<T> codec;
        private final MapCodec<Single<T>> wrapped;

        public TypeBase(Codec<T> codec) {
            this.codec = codec;
            this.wrapped = codec.xmap(predicate -> new Single<>(this, predicate), Single::predicate).fieldOf("value");
        }

        @Override
        public Codec<T> codec() {
            return codec;
        }

        @Override
        public MapCodec<Single<T>> wrappedCodec() {
            return wrapped;
        }

        @Override
        @SuppressWarnings({"unchecked", "rawtypes"})
        public StreamCodec<RegistryFriendlyByteBuf, Single<T>> singleStreamCodec() {
            return (StreamCodec) SINGLE_STREAM_CODEC;
        }
    }

    public static final class ConcreteType<T extends DataComponentPredicate> extends TypeBase<T> {
        public ConcreteType(Codec<T> codec) {
            super(codec);
        }
    }

    public static final class AnyValueType extends TypeBase<AnyValue> {
        private final AnyValue predicate;

        public AnyValueType(AnyValue predicate) {
            super(MapCodec.unit(predicate).codec());
            this.predicate = predicate;
        }

        public AnyValue predicate() {
            return predicate;
        }

        public DataComponentType<?> componentType() {
            return predicate.type();
        }

        public static AnyValueType create(DataComponentType<?> componentType) {
            return new AnyValueType(new AnyValue(componentType));
        }
    }

    public static record Single<T extends DataComponentPredicate>(Type<T> type, T predicate) {
    }
}
