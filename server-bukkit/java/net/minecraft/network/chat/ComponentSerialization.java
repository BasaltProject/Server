// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.MapEncoder;
import com.mojang.serialization.MapLike;
import com.mojang.serialization.RecordBuilder;
import io.netty.buffer.ByteBuf;
import java.util.Collection;
import java.util.Optional;
import java.util.function.Function;
import java.util.stream.Stream;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.ExtraCodecs;
public class ComponentSerialization {
    protected ComponentSerialization() {
    }
    public static final Codec<Component> CODEC = org.basaltmc.bukkit.facade.ChatJson.componentCodec();
    public static final StreamCodec<RegistryFriendlyByteBuf, Component> STREAM_CODEC = org.basaltmc.bukkit.facade.ChatJson.nbtStream(CODEC);
    public static final StreamCodec<RegistryFriendlyByteBuf, Optional<Component>> OPTIONAL_STREAM_CODEC =
        net.minecraft.network.codec.ByteBufCodecs.optional(STREAM_CODEC);
    public static final StreamCodec<RegistryFriendlyByteBuf, Component> TRUSTED_STREAM_CODEC = STREAM_CODEC;
    public static final StreamCodec<RegistryFriendlyByteBuf, Optional<Component>> TRUSTED_OPTIONAL_STREAM_CODEC = OPTIONAL_STREAM_CODEC;
    public static final StreamCodec<ByteBuf, Component> TRUSTED_CONTEXT_FREE_STREAM_CODEC = StreamCodec.of(
        (buf, value) -> net.minecraft.network.FriendlyByteBuf.writeNbt(buf, CODEC.encodeStart(net.minecraft.nbt.NbtOps.INSTANCE, value).getOrThrow()),
        buf -> CODEC.parse(net.minecraft.nbt.NbtOps.INSTANCE,
            net.minecraft.network.FriendlyByteBuf.readNbt(buf, net.minecraft.nbt.NbtAccounter.defaultQuota())).getOrThrow());
    public static Codec<Component> flatRestrictedCodec(int maxFlatSize) {
        return new Codec<Component>() {
            public <T> DataResult<com.mojang.datafixers.util.Pair<Component, T>> decode(DynamicOps<T> ops, T input) {
                return CODEC.decode(ops, input).flatMap(pair -> isTooLarge(ops, pair.getFirst())
                    ? DataResult.error(() -> "Component was too large: greater than max size " + maxFlatSize) : DataResult.success(pair));
            }
            public <T> DataResult<T> encode(Component input, DynamicOps<T> ops, T prefix) {
                return CODEC.encodeStart(ops, input);
            }
            private <T> boolean isTooLarge(DynamicOps<T> ops, Component input) {
                DynamicOps<com.google.gson.JsonElement> json = ops instanceof net.minecraft.resources.RegistryOps<T> registry
                    ? registry.withParent(com.mojang.serialization.JsonOps.INSTANCE) : com.mojang.serialization.JsonOps.INSTANCE;
                DataResult<com.google.gson.JsonElement> encoded = CODEC.encodeStart(json, input);
                return encoded.isSuccess() && net.minecraft.util.GsonHelper.encodesLongerThan(encoded.getOrThrow(), maxFlatSize);
            }
        };
    }
    public static <T> MapCodec<T> createLegacyComponentMatcher(ExtraCodecs.LateBoundIdMapper<String, MapCodec<? extends T>> types, Function<T, MapCodec<? extends T>> codecGetter, String typeFieldName) {
        MapCodec<T> fuzzy = new FuzzyCodec<>(types::values, codecGetter);
        MapCodec<T> typed = types.codec(Codec.STRING).dispatchMap(typeFieldName, codecGetter, c -> c);
        return ExtraCodecs.orCompressed(new StrictEither<>(typeFieldName, typed, fuzzy), typed);
    }
    static class StrictEither<T> extends MapCodec<T> {
        private final String typeFieldName;
        private final MapCodec<T> typed;
        private final MapCodec<T> fuzzy;
        public StrictEither(String typeFieldName, MapCodec<T> typed, MapCodec<T> fuzzy) {
            this.typeFieldName = typeFieldName;
            this.typed = typed;
            this.fuzzy = fuzzy;
        }
        public <O> DataResult<T> decode(DynamicOps<O> ops, MapLike<O> input) {
            return input.get(typeFieldName) != null ? typed.decode(ops, input) : fuzzy.decode(ops, input);
        }
        public <O> RecordBuilder<O> encode(T input, DynamicOps<O> ops, RecordBuilder<O> prefix) {
            return fuzzy.encode(input, ops, prefix);
        }
        public <T1> Stream<T1> keys(DynamicOps<T1> ops) {
            return Stream.concat(typed.keys(ops), fuzzy.keys(ops)).distinct();
        }
    }
    static class FuzzyCodec<T> extends MapCodec<T> {
        private final java.util.function.Supplier<Collection<MapCodec<? extends T>>> codecs;
        private final Function<T, ? extends MapEncoder<? extends T>> encoderGetter;
        public FuzzyCodec(Collection<MapCodec<? extends T>> codecs, Function<T, ? extends MapEncoder<? extends T>> encoderGetter) {
            this(() -> codecs, encoderGetter);
        }
        public FuzzyCodec(java.util.function.Supplier<Collection<MapCodec<? extends T>>> codecs, Function<T, ? extends MapEncoder<? extends T>> encoderGetter) {
            this.codecs = codecs;
            this.encoderGetter = encoderGetter;
        }
        public <S> DataResult<T> decode(DynamicOps<S> ops, MapLike<S> input) {
            for (var codec : codecs.get()) {
                DataResult<? extends T> result = codec.decode(ops, input);
                if (result.result().isPresent()) {
                    return result.map(value -> value);
                }
            }
            return DataResult.error(() -> "No matching codec found");
        }
        @SuppressWarnings("unchecked")
        public <S> RecordBuilder<S> encode(T input, DynamicOps<S> ops, RecordBuilder<S> prefix) {
            return ((MapEncoder<T>) encoderGetter.apply(input)).encode(input, ops, prefix);
        }
        public <S> Stream<S> keys(DynamicOps<S> ops) {
            return codecs.get().stream().flatMap(c -> c.keys(ops)).distinct();
        }
        public String toString() {
            return "FuzzyCodec[" + codecs.get() + "]";
        }
    }
}
