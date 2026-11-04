// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.google.gson.JsonElement;
import com.mojang.authlib.GameProfile;
import com.mojang.authlib.properties.PropertyMap;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.JavaOps;
import com.mojang.serialization.JsonOps;
import com.mojang.serialization.Lifecycle;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.MapLike;
import com.mojang.serialization.RecordBuilder;
import com.mojang.serialization.codecs.BaseMapCodec;
import it.unimi.dsi.fastutil.objects.Object2BooleanMap;
import it.unimi.dsi.fastutil.objects.Object2BooleanOpenHashMap;
import java.net.URI;
import java.nio.file.Path;
import java.time.Instant;
import java.time.format.DateTimeFormatter;
import java.time.temporal.TemporalAccessor;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.BitSet;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalLong;
import java.util.Set;
import java.util.function.BiFunction;
import java.util.function.Function;
import java.util.function.IntFunction;
import java.util.function.ToIntFunction;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import net.minecraft.core.HolderSet;
import net.minecraft.nbt.Tag;
import net.minecraft.resources.Identifier;
import org.basaltmc.bukkit.facade.ChatJson;
import org.joml.AxisAngle4f;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Quaternionf;
import org.joml.Quaternionfc;
import org.joml.Vector2f;
import org.joml.Vector2fc;
import org.joml.Vector3f;
import org.joml.Vector3fc;
import org.joml.Vector3i;
import org.joml.Vector3ic;
import org.joml.Vector4f;
import org.joml.Vector4fc;

public class ExtraCodecs {
    public static final Codec<JsonElement> JSON = Codec.PASSTHROUGH.xmap(
        dynamic -> dynamic.convert(JsonOps.INSTANCE).getValue(), json -> new Dynamic<>(JsonOps.INSTANCE, json));
    public static final Codec<Object> JAVA = Codec.PASSTHROUGH.xmap(
        dynamic -> dynamic.convert(JavaOps.INSTANCE).getValue(), value -> new Dynamic<>(JavaOps.INSTANCE, value));
    public static final Codec<Tag> NBT = ChatJson.TAG_CODEC;
    public static final Codec<Vector2fc> VECTOR2F = floatList(2).xmap(l -> new Vector2f(l.get(0), l.get(1)),
        v -> List.of(v.x(), v.y()));
    public static final Codec<Vector3fc> VECTOR3F = floatList(3).xmap(l -> new Vector3f(l.get(0), l.get(1), l.get(2)),
        v -> List.of(v.x(), v.y(), v.z()));
    public static final Codec<Vector3ic> VECTOR3I = Codec.INT.listOf().comapFlatMap(
        list -> list.size() == 3 ? DataResult.success((Vector3ic) new Vector3i(list.get(0), list.get(1), list.get(2)))
            : DataResult.error(() -> "Expected 3 values, got " + list.size()),
        v -> List.of(v.x(), v.y(), v.z()));
    public static final Codec<Vector4fc> VECTOR4F = floatList(4).xmap(
        l -> new Vector4f(l.get(0), l.get(1), l.get(2), l.get(3)), v -> List.of(v.x(), v.y(), v.z(), v.w()));
    public static final Codec<Quaternionfc> QUATERNIONF_COMPONENTS = floatList(4).xmap(
        l -> new Quaternionf(l.get(0), l.get(1), l.get(2), l.get(3)).normalize(),
        q -> List.of(q.x(), q.y(), q.z(), q.w()));
    public static final Codec<AxisAngle4f> AXISANGLE4F = com.mojang.serialization.codecs.RecordCodecBuilder.create(
        instance -> instance.group(Codec.FLOAT.fieldOf("angle").forGetter(a -> a.angle),
            VECTOR3F.fieldOf("axis").forGetter(a -> new Vector3f(a.x, a.y, a.z)))
            .apply(instance, (angle, axis) -> new AxisAngle4f(angle, axis)));
    public static final Codec<Quaternionfc> QUATERNIONF = Codec.withAlternative(QUATERNIONF_COMPONENTS,
        AXISANGLE4F.xmap(a -> (Quaternionfc) new Quaternionf(a), q -> new AxisAngle4f(q)));
    public static final Codec<Matrix4fc> MATRIX4F = Codec.FLOAT.listOf().comapFlatMap(list -> {
        if (list.size() != 16) {
            return DataResult.error(() -> "Expected 16 values, got " + list.size());
        }
        Matrix4f matrix = new Matrix4f();
        for (int i = 0; i < 16; i++) {
            matrix.setRowColumn(i >> 2, i & 3, list.get(i));
        }
        return DataResult.success((Matrix4fc) matrix);
    }, matrix -> {
        List<Float> values = new ArrayList<>(16);
        for (int i = 0; i < 16; i++) {
            values.add(matrix.getRowColumn(i >> 2, i & 3));
        }
        return values;
    });
    public static final Codec<Integer> RGB_COLOR_CODEC = Codec.withAlternative(Codec.INT,
        VECTOR3F.xmap(v -> packColor(v.x(), v.y(), v.z(), 1.0F) | 0xFF000000, v -> new Vector3f(
            (v >> 16 & 255) / 255.0F, (v >> 8 & 255) / 255.0F, (v & 255) / 255.0F)));
    public static final Codec<Integer> ARGB_COLOR_CODEC = Codec.withAlternative(Codec.INT,
        VECTOR4F.xmap(v -> packColor(v.x(), v.y(), v.z(), v.w()), v -> new Vector4f(
            (v >> 16 & 255) / 255.0F, (v >> 8 & 255) / 255.0F, (v & 255) / 255.0F, (v >>> 24) / 255.0F)));
    public static final Codec<Vector3fc> RGB_COLOR_VEC3_CODEC = VECTOR3F;
    public static final Codec<Vector4fc> ARGB_COLOR_VEC4_CODEC = VECTOR4F;
    public static final Codec<Integer> STRING_RGB_COLOR = Codec.STRING.comapFlatMap(
        text -> parseHex(text, 6).map(value -> value | 0xFF000000), value -> String.format("#%06X", value & 0xFFFFFF));
    public static final Codec<Integer> STRING_ARGB_COLOR = Codec.STRING.comapFlatMap(
        text -> parseHex(text, 8), value -> String.format("#%08X", value));
    public static final Codec<Vector3fc> STRING_RGB_VEC3_COLOR = STRING_RGB_COLOR.xmap(
        value -> new Vector3f((value >> 16 & 255) / 255.0F, (value >> 8 & 255) / 255.0F, (value & 255) / 255.0F),
        v -> packColor(v.x(), v.y(), v.z(), 1.0F) | 0xFF000000);
    public static final Codec<Vector4fc> STRING_ARGB_VEC4_COLOR = STRING_ARGB_COLOR.xmap(
        value -> new Vector4f((value >> 16 & 255) / 255.0F, (value >> 8 & 255) / 255.0F, (value & 255) / 255.0F,
            (value >>> 24) / 255.0F),
        v -> packColor(v.x(), v.y(), v.z(), v.w()));
    public static final Codec<Integer> UNSIGNED_BYTE = Codec.BYTE.flatComapMap(
        value -> value & 255, value -> value > 255 ? DataResult.error(() -> "Unsigned byte was too large: " + value + " > 255")
            : DataResult.success(value.byteValue()));
    public static final Codec<Integer> NON_NEGATIVE_INT = intRangeWithMessage(0, Integer.MAX_VALUE,
        value -> "Value must be non-negative: " + value);
    public static final Codec<Integer> POSITIVE_INT = intRangeWithMessage(1, Integer.MAX_VALUE,
        value -> "Value must be positive: " + value);
    public static final Codec<Long> NON_NEGATIVE_LONG = Codec.LONG.validate(
        value -> value >= 0L ? DataResult.success(value) : DataResult.error(() -> "Value must be non-negative: " + value));
    public static final Codec<Long> POSITIVE_LONG = Codec.LONG.validate(
        value -> value > 0L ? DataResult.success(value) : DataResult.error(() -> "Value must be positive: " + value));
    public static final Codec<Float> NON_NEGATIVE_FLOAT = Codec.FLOAT.validate(
        value -> value >= 0.0F ? DataResult.success(value) : DataResult.error(() -> "Value must be non-negative: " + value));
    public static final Codec<Float> POSITIVE_FLOAT = Codec.FLOAT.validate(
        value -> value > 0.0F ? DataResult.success(value) : DataResult.error(() -> "Value must be positive: " + value));
    public static final Codec<Pattern> PATTERN = Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(Pattern.compile(text));
        } catch (java.util.regex.PatternSyntaxException e) {
            return DataResult.error(() -> "Invalid regex pattern '" + text + "': " + e.getMessage());
        }
    }, Pattern::pattern);
    public static final Codec<Instant> INSTANT_ISO8601 = temporalCodec(DateTimeFormatter.ISO_INSTANT).xmap(
        Instant::from, instant -> instant);
    public static final Codec<byte[]> BASE64_STRING = Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(Base64.getDecoder().decode(text));
        } catch (IllegalArgumentException e) {
            return DataResult.error(() -> "Malformed base64 string");
        }
    }, bytes -> Base64.getEncoder().encodeToString(bytes));
    public static final Codec<String> ESCAPED_STRING = Codec.STRING.comapFlatMap(
        text -> DataResult.success(org.apache.commons.lang3.StringEscapeUtils.unescapeJava(text)),
        org.apache.commons.lang3.StringEscapeUtils::escapeJava);
    public static final Codec<TagOrElementLocation> TAG_OR_ELEMENT_ID = Codec.STRING.comapFlatMap(text -> {
        boolean tag = text.startsWith("#");
        Identifier id = Identifier.tryParse(tag ? text.substring(1) : text);
        return id != null ? DataResult.success(new TagOrElementLocation(id, tag))
            : DataResult.error(() -> "Invalid tag id: " + text);
    }, TagOrElementLocation::toString);
    public static final Function<Optional<Long>, OptionalLong> toOptionalLong = optional ->
        optional.map(OptionalLong::of).orElseGet(OptionalLong::empty);
    public static final Function<OptionalLong, Optional<Long>> fromOptionalLong = optional ->
        optional.isPresent() ? Optional.of(optional.getAsLong()) : Optional.empty();
    public static final Codec<BitSet> BIT_SET = Codec.LONG_STREAM.xmap(
        stream -> BitSet.valueOf(stream.toArray()), set -> Arrays.stream(set.toLongArray()));
    public static final int MAX_PROPERTY_NAME_LENGTH = 64;
    public static final int MAX_PROPERTY_VALUE_LENGTH = 32767;
    public static final int MAX_PROPERTY_SIGNATURE_LENGTH = 1024;
    public static final int MAX_PROPERTIES = 16;
    public static final Codec<PropertyMap> PROPERTY_MAP = unsupportedCodec("authlib property maps");
    public static final Codec<String> PLAYER_NAME = Codec.STRING.validate(name -> name.length() > 16
        ? DataResult.error(() -> "Player name '" + name + "' is too long; maximum length is 16")
        : DataResult.success(name));
    public static final Codec<GameProfile> AUTHLIB_GAME_PROFILE = unsupportedCodec("authlib game profiles");
    public static final MapCodec<GameProfile> STORED_GAME_PROFILE = ChatJson.unsupportedMap("stored game profiles");
    public static final Codec<String> NON_EMPTY_STRING = Codec.STRING.validate(
        value -> value.isEmpty() ? DataResult.error(() -> "Expected non-empty string") : DataResult.success(value));
    public static final Codec<Integer> CODEPOINT = Codec.STRING.comapFlatMap(text -> {
        int[] points = text.codePoints().toArray();
        return points.length != 1 ? DataResult.error(() -> "Expected one codepoint, got: " + text)
            : DataResult.success(points[0]);
    }, codepoint -> new String(Character.toChars(codepoint)));
    public static final Codec<String> RESOURCE_PATH_CODEC = Codec.STRING.validate(
        path -> Identifier.isValidPath(path) ? DataResult.success(path)
            : DataResult.error(() -> "Invalid string to use as a resource path element: " + path));
    public static final Codec<URI> UNTRUSTED_URI = Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(Util.parseAndValidateUntrustedUri(text));
        } catch (java.net.URISyntaxException e) {
            return DataResult.error(e::getMessage);
        }
    }, URI::toString);
    public static final Codec<String> CHAT_STRING = Codec.STRING.validate(text -> {
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c < ' ' || c == 127 || c == '§') {
                return DataResult.error(() -> "Disallowed chat character: '" + c + "'");
            }
        }
        return DataResult.success(text);
    });

    private static <T> Codec<T> unsupportedCodec(String what) {
        return new Codec<>() {
            @Override
            public <O> DataResult<Pair<T, O>> decode(DynamicOps<O> ops, O input) {
                return DataResult.error(() -> what + " are not supported by Basalt");
            }

            @Override
            public <O> DataResult<O> encode(T input, DynamicOps<O> ops, O prefix) {
                return DataResult.error(() -> what + " are not supported by Basalt");
            }
        };
    }

    private static Codec<List<Float>> floatList(int size) {
        return Codec.FLOAT.listOf().validate(list -> list.size() == size ? DataResult.success(list)
            : DataResult.error(() -> "Expected " + size + " values, got " + list.size()));
    }

    private static int packColor(float r, float g, float b, float a) {
        return Math.round(a * 255.0F) << 24 | Math.round(r * 255.0F) << 16 | Math.round(g * 255.0F) << 8
            | Math.round(b * 255.0F);
    }

    private static DataResult<Integer> parseHex(String text, int digits) {
        if (!text.startsWith("#") || text.length() != digits + 1) {
            return DataResult.error(() -> "Invalid color value: " + text);
        }
        try {
            return DataResult.success((int) Long.parseLong(text.substring(1), 16));
        } catch (NumberFormatException e) {
            return DataResult.error(() -> "Invalid color value: " + text);
        }
    }

    private static Codec<Integer> intRangeWithMessage(int min, int max, Function<Integer, String> message) {
        return Codec.INT.validate(value -> value.compareTo(min) >= 0 && value.compareTo(max) <= 0
            ? DataResult.success(value) : DataResult.error(() -> message.apply(value)));
    }

    public static <T> Codec<T> converter(DynamicOps<T> ops) {
        return Codec.PASSTHROUGH.xmap(dynamic -> dynamic.convert(ops).getValue(), value -> new Dynamic<>(ops, value));
    }

    public static <P, I> Codec<I> intervalCodec(Codec<P> pointCodec, String lowerBoundName, String upperBoundName,
        BiFunction<P, P, DataResult<I>> makeInterval, Function<I, P> getMin, Function<I, P> getMax) {
        Codec<I> listForm = Codec.list(pointCodec).comapFlatMap(list -> Util.fixedSize(list, 2).flatMap(pair -> {
            return makeInterval.apply(pair.get(0), pair.get(1));
        }), interval -> List.of(getMin.apply(interval), getMax.apply(interval)));
        Codec<I> objectForm = com.mojang.serialization.codecs.RecordCodecBuilder.<Pair<P, P>>create(instance -> instance
            .group(pointCodec.fieldOf(lowerBoundName).forGetter(Pair::getFirst),
                pointCodec.fieldOf(upperBoundName).forGetter(Pair::getSecond))
            .apply(instance, Pair::of)).comapFlatMap(pair -> makeInterval.apply(pair.getFirst(), pair.getSecond()),
            interval -> Pair.of(getMin.apply(interval), getMax.apply(interval)));
        return Codec.withAlternative(listForm, objectForm);
    }

    public static <A> Codec.ResultFunction<A> orElsePartial(A value) {
        return new Codec.ResultFunction<>() {
            @Override
            public <T> DataResult<Pair<A, T>> apply(DynamicOps<T> ops, T input, DataResult<Pair<A, T>> a) {
                return a.isError() ? DataResult.success(Pair.of(value, input), Lifecycle.stable()) : a;
            }

            @Override
            public <T> DataResult<T> coApply(DynamicOps<T> ops, A input, DataResult<T> t) {
                return t;
            }
        };
    }

    public static <E> Codec<E> idResolverCodec(ToIntFunction<E> toInt, IntFunction<E> fromInt, int unknownId) {
        return Codec.INT.flatXmap(id -> {
            E value = id == unknownId ? null : fromInt.apply(id);
            return value != null ? DataResult.success(value) : DataResult.error(() -> "Unknown element id: " + id);
        }, element -> {
            int id = toInt.applyAsInt(element);
            return id == unknownId ? DataResult.error(() -> "Element with unknown id: " + element)
                : DataResult.success(id);
        });
    }

    public static <I, E> Codec<E> idResolverCodec(Codec<I> value, Function<I, E> fromId, Function<E, I> toId) {
        return value.flatXmap(id -> {
            E element = fromId.apply(id);
            return element == null ? DataResult.error(() -> "Unknown element id: " + id) : DataResult.success(element);
        }, element -> {
            I id = toId.apply(element);
            return id == null ? DataResult.error(() -> "Element with unknown id: " + element) : DataResult.success(id);
        });
    }

    public static <E> Codec<E> orCompressed(Codec<E> normal, Codec<E> compressed) {
        return new Codec<>() {
            @Override
            public <T> DataResult<T> encode(E input, DynamicOps<T> ops, T prefix) {
                return ops.compressMaps() ? compressed.encode(input, ops, prefix) : normal.encode(input, ops, prefix);
            }

            @Override
            public <T> DataResult<Pair<E, T>> decode(DynamicOps<T> ops, T input) {
                return ops.compressMaps() ? compressed.decode(ops, input) : normal.decode(ops, input);
            }

            @Override
            public String toString() {
                return normal + " orCompressed " + compressed;
            }
        };
    }

    public static <E> MapCodec<E> orCompressed(MapCodec<E> normal, MapCodec<E> compressed) {
        return new MapCodec<>() {
            @Override
            public <T> RecordBuilder<T> encode(E input, DynamicOps<T> ops, RecordBuilder<T> prefix) {
                return ops.compressMaps() ? compressed.encode(input, ops, prefix) : normal.encode(input, ops, prefix);
            }

            @Override
            public <T> DataResult<E> decode(DynamicOps<T> ops, MapLike<T> input) {
                return ops.compressMaps() ? compressed.decode(ops, input) : normal.decode(ops, input);
            }

            @Override
            public <T> java.util.stream.Stream<T> keys(DynamicOps<T> ops) {
                return compressed.keys(ops);
            }

            @Override
            public String toString() {
                return normal + " orCompressed " + compressed;
            }
        };
    }

    public static <E> Codec<E> overrideLifecycle(Codec<E> codec, Function<E, Lifecycle> decodeLifecycle,
        Function<E, Lifecycle> encodeLifecycle) {
        return codec.mapResult(new Codec.ResultFunction<>() {
            @Override
            public <T> DataResult<Pair<E, T>> apply(DynamicOps<T> ops, T input, DataResult<Pair<E, T>> a) {
                return a.result().map(r -> a.setLifecycle(decodeLifecycle.apply(r.getFirst()))).orElse(a);
            }

            @Override
            public <T> DataResult<T> coApply(DynamicOps<T> ops, E input, DataResult<T> t) {
                return t.setLifecycle(encodeLifecycle.apply(input));
            }

            @Override
            public String toString() {
                return "WithLifecycle[" + decodeLifecycle + " " + encodeLifecycle + "]";
            }
        });
    }

    public static <E> Codec<E> overrideLifecycle(Codec<E> codec, Function<E, Lifecycle> lifecycleGetter) {
        return overrideLifecycle(codec, lifecycleGetter, lifecycleGetter);
    }

    public static <K, V> StrictUnboundedMapCodec<K, V> strictUnboundedMap(Codec<K> keyCodec, Codec<V> elementCodec) {
        return new StrictUnboundedMapCodec<>(keyCodec, elementCodec);
    }

    public static <E> Codec<List<E>> compactListCodec(Codec<E> elementCodec) {
        return compactListCodec(elementCodec, elementCodec.listOf());
    }

    public static <E> Codec<List<E>> compactListCodec(Codec<E> elementCodec, Codec<List<E>> listCodec) {
        return Codec.either(listCodec, elementCodec).xmap(
            either -> either.map(list -> list, List::of), list -> list.size() == 1 ? com.mojang.datafixers.util.Either.right(list.get(0))
                : com.mojang.datafixers.util.Either.left(list));
    }

    private static Codec<Integer> intRangeWithMessage(int min, int max) {
        return intRangeWithMessage(min, max, value -> "Value must be within range [" + min + ";" + max + "]: " + value);
    }

    public static Codec<Integer> intRange(int minInclusive, int maxInclusive) {
        return intRangeWithMessage(minInclusive, maxInclusive);
    }

    public static Codec<Long> longRange(int minInclusive, int maxInclusive) {
        return Codec.LONG.validate(value -> value >= minInclusive && value <= maxInclusive ? DataResult.success(value)
            : DataResult.error(() -> "Value must be within range [" + minInclusive + ";" + maxInclusive + "]: " + value));
    }

    public static Codec<Float> floatRange(float minInclusive, float maxInclusive) {
        return Codec.FLOAT.validate(value -> value >= minInclusive && value <= maxInclusive ? DataResult.success(value)
            : DataResult.error(() -> "Value must be within range [" + minInclusive + ";" + maxInclusive + "]: " + value));
    }

    public static <T> Codec<List<T>> nonEmptyList(Codec<List<T>> listCodec) {
        return listCodec.validate(list -> list.isEmpty() ? DataResult.error(() -> "List must have contents")
            : DataResult.success(list));
    }

    public static <T> Codec<HolderSet<T>> nonEmptyHolderSet(Codec<HolderSet<T>> listCodec) {
        return listCodec.validate(set -> set.unwrap().right().filter(List::isEmpty).isPresent()
            ? DataResult.error(() -> "List must have contents") : DataResult.success(set));
    }

    public static <M extends Map<?, ?>> Codec<M> nonEmptyMap(Codec<M> mapCodec) {
        return mapCodec.validate(map -> map.isEmpty() ? DataResult.error(() -> "Map must have contents")
            : DataResult.success(map));
    }

    public static <E> MapCodec<E> retrieveContext(Function<DynamicOps<?>, DataResult<E>> getter) {
        return new MapCodec<>() {
            @Override
            public <T> java.util.stream.Stream<T> keys(DynamicOps<T> ops) {
                return java.util.stream.Stream.empty();
            }

            @Override
            public <T> DataResult<E> decode(DynamicOps<T> ops, MapLike<T> input) {
                return getter.apply(ops);
            }

            @Override
            public <T> RecordBuilder<T> encode(E input, DynamicOps<T> ops, RecordBuilder<T> prefix) {
                return prefix;
            }

            @Override
            public String toString() {
                return "ContextRetrievalCodec[" + getter + "]";
            }
        };
    }

    public static <E, L extends Collection<E>, T> Function<L, DataResult<L>> ensureHomogenous(
        Function<E, T> typeGetter) {
        return collection -> {
            java.util.Iterator<E> iterator = collection.iterator();
            if (iterator.hasNext()) {
                T expected = typeGetter.apply(iterator.next());
                while (iterator.hasNext()) {
                    E next = iterator.next();
                    T actual = typeGetter.apply(next);
                    if (!java.util.Objects.equals(actual, expected)) {
                        return DataResult.error(() -> "Mixed type list: element '" + next + "' had type '" + actual
                            + "', but list is of type '" + expected + "'");
                    }
                }
            }
            return DataResult.success(collection, Lifecycle.stable());
        };
    }

    public static <A> Codec<A> catchDecoderException(Codec<A> codec) {
        return Codec.of(codec, new com.mojang.serialization.Decoder<>() {
            @Override
            public <T> DataResult<Pair<A, T>> decode(DynamicOps<T> ops, T input) {
                try {
                    return codec.decode(ops, input);
                } catch (Exception e) {
                    return DataResult.error(() -> "Caught exception decoding " + input + ": " + e.getMessage());
                }
            }
        });
    }

    public static Codec<TemporalAccessor> temporalCodec(DateTimeFormatter formatter) {
        return Codec.STRING.comapFlatMap(text -> {
            try {
                return DataResult.success(formatter.parse(text));
            } catch (java.time.format.DateTimeParseException e) {
                return DataResult.error(e::getMessage);
            }
        }, formatter::format);
    }

    public static MapCodec<OptionalLong> asOptionalLong(MapCodec<Optional<Long>> fieldCodec) {
        return fieldCodec.xmap(toOptionalLong, fromOptionalLong);
    }

    public static <K, V> Codec<Map<K, V>> sizeLimitedMap(Codec<Map<K, V>> codec, int maxSizeInclusive) {
        return codec.validate(map -> map.size() > maxSizeInclusive
            ? DataResult.error(() -> "Map is too long: " + map.size() + ", expected range [0-" + maxSizeInclusive + "]")
            : DataResult.success(map));
    }

    public static <T> Codec<Object2BooleanMap<T>> object2BooleanMap(Codec<T> keyCodec) {
        return Codec.unboundedMap(keyCodec, Codec.BOOL).xmap(Object2BooleanOpenHashMap::new, Object2BooleanOpenHashMap::new);
    }

    public static <K, V> MapCodec<V> dispatchOptionalValue(String typeKey, String valueKey, Codec<K> typeCodec,
        Function<? super V, ? extends K> typeGetter, Function<? super K, ? extends Codec<? extends V>> valueCodec) {
        return ChatJson.unsupportedMap("optional dispatch codecs");
    }

    public static <A> Codec<Optional<A>> optionalEmptyMap(Codec<A> codec) {
        return new Codec<>() {
            @Override
            public <T> DataResult<Pair<Optional<A>, T>> decode(DynamicOps<T> ops, T input) {
                if (isEmptyMap(ops, input)) {
                    return DataResult.success(Pair.of(Optional.empty(), input));
                }
                return codec.decode(ops, input).map(pair -> pair.mapFirst(Optional::of));
            }

            private <T> boolean isEmptyMap(DynamicOps<T> ops, T input) {
                Optional<MapLike<T>> map = ops.getMap(input).result();
                return map.isPresent() && map.get().entries().findAny().isEmpty();
            }

            @Override
            public <T> DataResult<T> encode(Optional<A> input, DynamicOps<T> ops, T prefix) {
                return input.isPresent() ? codec.encode(input.get(), ops, prefix)
                    : DataResult.success(ops.emptyMap(), Lifecycle.stable());
            }
        };
    }

    public static <E extends Enum<E>> Codec<E> legacyEnum(Function<String, E> valueOf) {
        return Codec.STRING.comapFlatMap(name -> {
            try {
                return DataResult.success(valueOf.apply(name));
            } catch (IllegalArgumentException e) {
                return DataResult.error(() -> "No value with id: " + name);
            }
        }, Enum::toString);
    }

    public static Codec<Path> pathCodec(Function<String, Path> pathFactory) {
        return Codec.STRING.comapFlatMap(text -> {
            try {
                return DataResult.success(pathFactory.apply(text));
            } catch (RuntimeException e) {
                return DataResult.error(() -> "Invalid path " + text + ": " + e.getMessage());
            }
        }, Path::toString);
    }

    public static Codec<Path> relativeNormalizedSubPathCodec(Function<String, Path> pathFactory) {
        return pathCodec(pathFactory).validate(path -> path.isAbsolute() ? DataResult.error(() -> "Illegal absolute path: " + path)
            : !path.normalize().equals(path) || path.startsWith("..")
                ? DataResult.error(() -> "Illegal non-normalized path: " + path) : DataResult.success(path));
    }

    public static Codec<Path> guardedPathCodec(Path baseFolder) {
        return relativeNormalizedSubPathCodec(baseFolder::resolve).xmap(path -> path, path -> path);
    }

    public static <A> MapCodec<A> optionalAlwaysPresentFieldOf(Codec<A> elementCodec, String name, A defaultValue,
        boolean lenient) {
        MapCodec<Optional<A>> field = lenient ? elementCodec.lenientOptionalFieldOf(name) : elementCodec.optionalFieldOf(name);
        return field.xmap(optional -> optional.orElse(defaultValue), Optional::of);
    }

    public static <A> MapCodec<A> optionalAlwaysPresentFieldOf(Codec<A> elementCodec, String name, A defaultValue) {
        return optionalAlwaysPresentFieldOf(elementCodec, name, defaultValue, false);
    }

    public record StrictUnboundedMapCodec<K, V>(Codec<K> keyCodec, Codec<V> elementCodec)
        implements BaseMapCodec<K, V>, Codec<Map<K, V>> {
        @Override
        public <T> DataResult<Map<K, V>> decode(DynamicOps<T> ops, MapLike<T> input) {
            Map<K, V> result = new LinkedHashMap<>();
            DataResult<Map<K, V>> failure = null;
            for (var entry : (Iterable<Pair<T, T>>) input.entries()::iterator) {
                DataResult<K> key = keyCodec.parse(ops, entry.getFirst());
                DataResult<V> value = elementCodec.parse(ops, entry.getSecond());
                if (key.isError() || value.isError()) {
                    String message = key.isError() ? key.error().get().message() : value.error().get().message();
                    return DataResult.error(() -> "Failed to decode map entry " + entry.getFirst() + ": " + message);
                }
                result.put(key.getOrThrow(), value.getOrThrow());
            }
            return DataResult.success(result);
        }

        @Override
        public <T> DataResult<Pair<Map<K, V>, T>> decode(DynamicOps<T> ops, T input) {
            return ops.getMap(input).setLifecycle(Lifecycle.stable()).flatMap(map -> decode(ops, map))
                .map(map -> Pair.of(map, input));
        }

        @Override
        public <T> DataResult<T> encode(Map<K, V> input, DynamicOps<T> ops, T prefix) {
            return encode(input, ops, ops.mapBuilder()).build(prefix);
        }

        @Override
        public String toString() {
            return "StrictUnboundedMapCodec[" + keyCodec + " -> " + elementCodec + "]";
        }
    }

    public record TagOrElementLocation(Identifier id, boolean tag) {
        @Override
        public String toString() {
            return decoratedId();
        }

        private String decoratedId() {
            return tag ? "#" + id : id.toString();
        }
    }

    public static class LateBoundIdMapper<I, V> {
        private final Map<I, V> idToValue = new HashMap<>();
        private final Map<V, I> valueToId = new HashMap<>();

        public Codec<V> codec(Codec<I> idCodec) {
            return idCodec.flatXmap(id -> {
                V value = idToValue.get(id);
                return value != null ? DataResult.success(value) : DataResult.error(() -> "No value with id: " + id);
            }, value -> {
                I id = valueToId.get(value);
                return id != null ? DataResult.success(id) : DataResult.error(() -> "Element with unknown id: " + value);
            });
        }

        public LateBoundIdMapper<I, V> put(I id, V value) {
            idToValue.put(id, value);
            valueToId.put(value, id);
            return this;
        }

        public Set<V> values() {
            return new HashSet<>(idToValue.values());
        }
    }
}
