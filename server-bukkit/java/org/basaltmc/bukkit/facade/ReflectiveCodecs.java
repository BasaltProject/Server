package org.basaltmc.bukkit.facade;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapLike;
import java.lang.reflect.GenericArrayType;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.RecordComponent;
import java.lang.reflect.Type;
import java.lang.reflect.WildcardType;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Function;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.resources.Identifier;
import net.minecraft.util.StringRepresentable;

/** Builds codecs for plain value types by walking their record components, so every record-shaped component serialises. */
public final class ReflectiveCodecs {
    private static final Map<Type, Optional<Codec<?>>> kCache = new ConcurrentHashMap<>();

    private static final java.util.Set<String> kTransparent = java.util.Set.of("BannerPatternLayers", "SuspiciousStewEffects",
        "ChargedProjectiles", "ItemAttributeModifiers", "MapDecorations", "BlockItemStateProperties", "UseRemainder", "Bees",
        "SulfurCubeContent", "InstrumentComponent");

    private ReflectiveCodecs() {}

    public static boolean attachAll(Class<?> holder) {
        for (java.lang.reflect.Field field : holder.getDeclaredFields()) {
            try {
                if (field.getType() == net.minecraft.core.component.DataComponentType.class
                    && java.lang.reflect.Modifier.isStatic(field.getModifiers())) {
                    Object found = field.get(null);
                    if (found != null && field.getGenericType() instanceof ParameterizedType generic) {
                        net.minecraft.core.component.DataComponentType.Builder.attach(
                            (net.minecraft.core.component.DataComponentType<?>) found, generic.getActualTypeArguments()[0]);
                    }
                }
            } catch (ReflectiveOperationException | RuntimeException failure) {
                continue;
            }
        }
        return true;
    }

    public static Codec<?> forType(Type type) {
        Optional<Codec<?>> found = kCache.get(type);
        if (found == null) {
            Codec<?> built;
            try {
                built = build(type);
            } catch (RuntimeException | LinkageError failure) {
                built = null;
            }
            found = Optional.ofNullable(built);
            kCache.put(type, found);
        }
        return found.orElse(null);
    }

    private static Class<?> raw(Type type) {
        if (type instanceof Class<?> cls) {
            return cls;
        }
        if (type instanceof ParameterizedType parameterized) {
            return (Class<?>) parameterized.getRawType();
        }
        if (type instanceof WildcardType wildcard) {
            return raw(wildcard.getUpperBounds()[0]);
        }
        return Object.class;
    }

    private static Type argument(Type type, int index) {
        return type instanceof ParameterizedType parameterized ? parameterized.getActualTypeArguments()[index] : Object.class;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Codec<?> build(Type type) {
        Class<?> cls = raw(type);
        if (cls == Integer.class || cls == int.class) {
            return Codec.INT;
        }
        if (cls == Boolean.class || cls == boolean.class) {
            return Codec.BOOL;
        }
        if (cls == Float.class || cls == float.class) {
            return Codec.FLOAT;
        }
        if (cls == Double.class || cls == double.class) {
            return Codec.DOUBLE;
        }
        if (cls == Long.class || cls == long.class) {
            return Codec.LONG;
        }
        if (cls == Byte.class || cls == byte.class) {
            return Codec.BYTE;
        }
        if (cls == Short.class || cls == short.class) {
            return Codec.SHORT;
        }
        if (cls == String.class) {
            return Codec.STRING;
        }
        if (cls == Component.class) {
            return ComponentSerialization.CODEC;
        }
        if (cls == Identifier.class) {
            return Identifier.CODEC;
        }
        if (cls == net.minecraft.util.Unit.class) {
            return net.minecraft.util.Unit.CODEC;
        }
        if (cls == net.minecraft.world.item.DyeColor.class) {
            return net.minecraft.world.item.DyeColor.CODEC;
        }
        Codec<?> declared = declaredCodec(cls);
        if (declared != null) {
            return declared;
        }
        if (cls == net.minecraft.core.Holder.class) {
            Class<?> valueClass = raw(argument(type, 0));
            return Identifier.CODEC.xmap(id -> HolderRefs.of(valueClass, id), holder -> holder.unwrapKey().map(
                key -> key.identifier()).orElseThrow(() -> new IllegalStateException("Direct holder")));
        }
        if (cls == net.minecraft.core.HolderSet.class) {
            return holderSetCodec(raw(argument(type, 0)));
        }
        if (cls == net.minecraft.resources.ResourceKey.class) {
            Class<?> valueClass = raw(argument(type, 0));
            return Identifier.CODEC.xmap(id -> net.minecraft.resources.ResourceKey.create(
                (net.minecraft.resources.ResourceKey) HolderRefs.registryKey(valueClass), id),
                key -> ((net.minecraft.resources.ResourceKey<?>) key).identifier());
        }
        if (cls == net.minecraft.world.item.component.TypedEntityData.class) {
            Codec<?> id = registryValueCodec(raw(argument(type, 0)));
            return id == null ? null : net.minecraft.world.item.component.TypedEntityData.codec((Codec) id);
        }
        if (cls == net.minecraft.server.network.Filterable.class) {
            Codec<?> value = forType(argument(type, 0));
            return value == null ? null : net.minecraft.server.network.Filterable.codec((Codec) value);
        }
        if (cls == net.minecraft.core.component.DataComponentType.class) {
            return net.minecraft.core.component.DataComponentType.CODEC;
        }
        if (cls == net.minecraft.nbt.CompoundTag.class) {
            return net.minecraft.nbt.CompoundTag.CODEC;
        }
        if (cls == java.util.UUID.class) {
            return net.minecraft.core.UUIDUtil.CODEC;
        }
        if (cls == it.unimi.dsi.fastutil.ints.IntList.class) {
            return Codec.INT.listOf().xmap(list -> (it.unimi.dsi.fastutil.ints.IntList) new it.unimi.dsi.fastutil.ints.IntArrayList(list),
                list -> (List<Integer>) list);
        }
        if (cls == java.util.Set.class || cls == java.util.SequencedSet.class) {
            Codec<?> element = forType(argument(type, 0));
            return element == null ? null : ((Codec) element).listOf().xmap(list -> new java.util.LinkedHashSet<Object>((List) list),
                set -> new ArrayList<Object>((java.util.Collection) set));
        }
        if (cls == Map.class) {
            Codec<?> key = forType(argument(type, 0));
            Codec<?> value = forType(argument(type, 1));
            return key == null || value == null ? null : Codec.unboundedMap((Codec) key, (Codec) value);
        }
        if (cls == List.class) {
            Codec<?> element = forType(argument(type, 0));
            return element == null ? null : ((Codec) element).listOf();
        }
        if (cls == Optional.class) {
            Codec<?> element = forType(argument(type, 0));
            return element == null ? null : optionalOf((Codec) element);
        }
        if (cls.isEnum()) {
            return enumCodec((Class) cls);
        }
        if (cls.isRecord()) {
            return recordCodec(cls);
        }
        return null;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Codec<?> registryValueCodec(Class<?> valueClass) {
        net.minecraft.core.Registry registry = null;
        if (valueClass == net.minecraft.world.entity.EntityType.class) {
            registry = net.minecraft.core.registries.BuiltInRegistries.ENTITY_TYPE;
        } else if (valueClass == net.minecraft.world.level.block.entity.BlockEntityType.class) {
            registry = net.minecraft.core.registries.BuiltInRegistries.BLOCK_ENTITY_TYPE;
        }
        if (registry == null) {
            return null;
        }
        final net.minecraft.core.Registry found = registry;
        return Identifier.CODEC.flatXmap(id -> {
            Object value = found.getValue(id);
            return value == null ? DataResult.error(() -> "Unknown " + id) : DataResult.success(value);
        }, value -> DataResult.success(found.getKey(value)));
    }

    private static Codec<?> declaredCodec(Class<?> cls) {
        try {
            java.lang.reflect.Field field = cls.getDeclaredField("CODEC");
            if (java.lang.reflect.Modifier.isStatic(field.getModifiers()) && Codec.class.isAssignableFrom(field.getType())) {
                field.setAccessible(true);
                Object found = field.get(null);
                return found instanceof LazyCodecs.Lazy<?> ? null : (Codec<?>) found;
            }
        } catch (ReflectiveOperationException | LinkageError failure) {
            return null;
        }
        return null;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <T> Codec<net.minecraft.core.HolderSet<T>> holderSet(Class<T> valueClass) {
        return (Codec) holderSetCodec(valueClass);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Codec<?> holderSetCodec(Class<?> valueClass) {
        return holderSetCodec(HolderRefs.registryKey(valueClass), id -> HolderRefs.of(valueClass, id));
    }

    public static <T> Codec<net.minecraft.core.HolderSet<T>> holderSet(net.minecraft.resources.ResourceKey<? extends net.minecraft.core.Registry<T>> key) {
        return (Codec) holderSetCodec((net.minecraft.resources.ResourceKey) key, id -> HolderRefs.ofKey((net.minecraft.resources.ResourceKey) key, id));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Codec<?> holderSetCodec(net.minecraft.resources.ResourceKey<? extends net.minecraft.core.Registry<Object>> registryKey, Function<Identifier, net.minecraft.core.Holder<?>> holderOf) {
        Codec<Object> text = Codec.STRING.flatXmap(raw -> {
            if (raw.startsWith("#")) {
                Identifier id = Identifier.tryParse(raw.substring(1));
                return id == null ? DataResult.error(() -> "Bad tag " + raw) : DataResult.success(
                    (Object) new net.minecraft.core.HolderSet.Named(net.minecraft.tags.TagKey.create(
                        (net.minecraft.resources.ResourceKey) registryKey, id)));
            }
            Identifier id = Identifier.tryParse(raw);
            return id == null ? DataResult.error(() -> "Bad id " + raw)
                : DataResult.success((Object) net.minecraft.core.HolderSet.direct(holderOf.apply(id)));
        }, set -> {
            net.minecraft.core.HolderSet<?> holders = (net.minecraft.core.HolderSet<?>) set;
            if (holders.unwrapKey().isPresent()) {
                return DataResult.success("#" + holders.unwrapKey().get().location());
            }
            return holders.size() == 1 ? DataResult.success(holders.get(0).getRegisteredName())
                : DataResult.error(() -> "Multiple holders");
        });
        Codec<Object> list = Identifier.CODEC.listOf().xmap(ids -> (Object) net.minecraft.core.HolderSet.direct(
            (List) ids.stream().map(id -> (net.minecraft.core.Holder) holderOf.apply(id)).toList()),
            set -> ((net.minecraft.core.HolderSet<?>) set).stream().map(h -> h.unwrapKey().get().identifier()).toList());
        return Codec.either(text, list).xmap(either -> either.map(a -> a, b -> b), set -> {
            net.minecraft.core.HolderSet<?> holders = (net.minecraft.core.HolderSet<?>) set;
            return holders.unwrapKey().isPresent() || holders.size() == 1 ? com.mojang.datafixers.util.Either.left(set)
                : com.mojang.datafixers.util.Either.right(set);
        });
    }

    private static <T> Codec<Optional<T>> optionalOf(Codec<T> element) {
        return element.xmap(Optional::of, optional -> optional.orElse(null));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <E extends Enum<E>> Codec<E> enumCodec(Class<E> cls) {
        Function<E, String> name = value -> value instanceof StringRepresentable named ? named.getSerializedName()
            : value.name().toLowerCase(Locale.ROOT);
        return Codec.STRING.comapFlatMap(text -> {
            for (E constant : cls.getEnumConstants()) {
                if (name.apply(constant).equals(text)) {
                    return DataResult.success(constant);
                }
            }
            return DataResult.error(() -> "Unknown " + cls.getSimpleName() + ": " + text);
        }, name);
    }

    private static String snake(String name) {
        StringBuilder out = new StringBuilder();
        for (char c : name.toCharArray()) {
            if (Character.isUpperCase(c)) {
                out.append('_').append(Character.toLowerCase(c));
            } else {
                out.append(c);
            }
        }
        return out.toString();
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Codec<?> recordCodec(Class<?> cls) {
        RecordComponent[] components = cls.getRecordComponents();
        if (components.length == 1 && kTransparent.contains(cls.getSimpleName())) {
            Codec inner = forType(components[0].getGenericType());
            if (inner == null) {
                return null;
            }
            try {
                java.lang.reflect.Constructor<?> constructor = cls.getDeclaredConstructor(components[0].getType());
                constructor.setAccessible(true);
                java.lang.reflect.Method accessor = components[0].getAccessor();
                accessor.setAccessible(true);
                return inner.xmap(value -> {
                    try {
                        return constructor.newInstance(value);
                    } catch (ReflectiveOperationException failure) {
                        throw new IllegalStateException(failure);
                    }
                }, value -> {
                    try {
                        return accessor.invoke(value);
                    } catch (ReflectiveOperationException failure) {
                        throw new IllegalStateException(failure);
                    }
                });
            } catch (ReflectiveOperationException failure) {
                return null;
            }
        }
        Codec[] codecs = new Codec[components.length];
        for (int i = 0; i < components.length; i++) {
            Type declared = components[i].getGenericType();
            Codec<?> codec = components[i].getType() == Optional.class ? forType(argument(declared, 0)) : forType(declared);
            if (codec == null) {
                return null;
            }
            codecs[i] = codec;
        }
        java.lang.reflect.Constructor<?> constructor;
        try {
            Class<?>[] types = new Class<?>[components.length];
            for (int i = 0; i < types.length; i++) {
                types[i] = components[i].getType();
            }
            constructor = cls.getDeclaredConstructor(types);
            constructor.setAccessible(true);
        } catch (ReflectiveOperationException failure) {
            return null;
        }
        final java.lang.reflect.Constructor<?> made = constructor;
        return new Codec<Object>() {
            @Override
            public <T> DataResult<com.mojang.datafixers.util.Pair<Object, T>> decode(DynamicOps<T> ops, T input) {
                DataResult<MapLike<T>> map = ops.getMap(input);
                if (map.error().isPresent()) {
                    return DataResult.error(() -> "Not a map for " + cls.getSimpleName());
                }
                MapLike<T> fields = map.result().get();
                Object[] values = new Object[components.length];
                for (int i = 0; i < values.length; i++) {
                    T raw = fields.get(snake(components[i].getName()));
                    boolean optional = components[i].getType() == Optional.class;
                    if (raw == null) {
                        if (optional) {
                            values[i] = Optional.empty();
                            continue;
                        }
                        if (components[i].getType().isPrimitive() || components[i].getType() == List.class) {
                            values[i] = components[i].getType() == List.class ? List.of() : null;
                            if (values[i] == null) {
                                return DataResult.error(() -> "Missing field");
                            }
                            continue;
                        }
                        values[i] = null;
                        continue;
                    }
                    DataResult<com.mojang.datafixers.util.Pair<Object, T>> parsed = codecs[i].decode(ops, raw);
                    if (parsed.error().isPresent()) {
                        return DataResult.error(() -> "Bad field");
                    }
                    Object decoded = parsed.result().get().getFirst();
                    values[i] = optional ? Optional.of(decoded) : decoded;
                }
                try {
                    return DataResult.success(new com.mojang.datafixers.util.Pair<>(made.newInstance(values), input));
                } catch (ReflectiveOperationException | IllegalArgumentException failure) {
                    return DataResult.error(() -> "Cannot build " + cls.getSimpleName() + ": " + failure.getMessage());
                }
            }

            @Override
            public <T> DataResult<T> encode(Object input, DynamicOps<T> ops, T prefix) {
                Map<T, T> out = new LinkedHashMap<>();
                try {
                    for (RecordComponent component : components) {
                        java.lang.reflect.Method accessor = component.getAccessor();
                        accessor.setAccessible(true);
                        Object value = accessor.invoke(input);
                        if (value == null || value instanceof Optional<?> optional && optional.isEmpty()) {
                            continue;
                        }
                        Codec codec = codecs[java.util.Arrays.asList(components).indexOf(component)];
                        Object unwrapped = value instanceof Optional<?> optional ? optional.get() : value;
                        DataResult<T> encoded = codec.encodeStart(ops, unwrapped);
                        if (encoded.error().isPresent()) {
                            return DataResult.error(() -> "Bad " + component.getName());
                        }
                        out.put(ops.createString(snake(component.getName())), encoded.result().get());
                    }
                } catch (ReflectiveOperationException failure) {
                    return DataResult.error(() -> "Cannot read " + cls.getSimpleName());
                }
                return DataResult.success(ops.createMap(out));
            }
        };
    }
}
