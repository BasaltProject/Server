package org.basaltmc.bukkit.facade;

import io.netty.buffer.ByteBuf;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;

/** Assembles network codecs from a table of field-by-field wire layouts (stream_specs.txt). */
public final class StreamSpecs {
    private static final Map<String, String> kClasses = new HashMap<>();
    private static final Map<String, String> kComponents = new HashMap<>();

    static {
        try (InputStream in = StreamSpecs.class.getResourceAsStream("/stream_specs.txt")) {
            if (in != null) {
                for (String line : new String(in.readAllBytes(), StandardCharsets.UTF_8).split("\\R")) {
                    if (line.length() < 3) {
                        continue;
                    }
                    int bar = line.indexOf('|');
                    (line.charAt(0) == 'K' ? kComponents : kClasses).put(line.substring(2, bar), line.substring(bar + 1));
                }
            }
        } catch (java.io.IOException failure) {
            throw new IllegalStateException(failure);
        }
    }

    private StreamSpecs() {}

    public static boolean hasComponent(String name) {
        return kComponents.containsKey(name);
    }

    /** The network codec of a data component, resolved on first use. */
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <T> StreamCodec<RegistryFriendlyByteBuf, T> component(String name) {
        String token = kComponents.get(name);
        java.util.function.Supplier<StreamCodec> resolver = new java.util.function.Supplier<>() {
            private StreamCodec resolved;

            @Override
            public synchronized StreamCodec get() {
                if (resolved == null) {
                    resolved = token(token);
                }
                return resolved;
            }
        };
        return StreamCodec.of((buf, value) -> resolver.get().encode(buf, value), buf -> (T) resolver.get().decode(buf));
    }

    /** A codec for the class's own static stream codec field, or null when the table has no layout for it. */
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static StreamCodec<?, ?> forClass(Class<?> owner, String field) {
        String layout = kClasses.get(owner.getName() + "#" + field);
        if (layout == null) {
            return null;
        }
        if (layout.startsWith("=holder:")) {
            return ByteBufCodecs.holder(registryKey(layout.substring(8)), directCodec(owner));
        }
        if (layout.startsWith("=unit:")) {
            try {
                Object instance = owner.getField(layout.substring(6)).get(null);
                return StreamCodec.unit(instance);
            } catch (ReflectiveOperationException failure) {
                throw new IllegalArgumentException("No unit instance " + layout, failure);
            }
        }
        if (layout.startsWith("=dispatch:")) {
            String[] references = layout.substring(10).split(",");
            Class<?>[] types = new Class<?>[references.length];
            StreamCodec[] codecs = new StreamCodec[references.length];
            for (int i = 0; i < references.length; i++) {
                codecs[i] = lookup(references[i]);
                try {
                    types[i] = Class.forName(references[i].substring(0, references[i].indexOf('#')));
                } catch (ClassNotFoundException failure) {
                    throw new IllegalArgumentException(references[i], failure);
                }
            }
            return StreamCodec.of(
                (buf, value) -> {
                    for (int i = 0; i < types.length; i++) {
                        if (types[i].isInstance(value)) {
                            ((FriendlyByteBuf) buf).writeVarInt(i);
                            codecs[i].encode(buf, value);
                            return;
                        }
                    }
                    throw new io.netty.handler.codec.EncoderException("Unknown variant " + value);
                },
                buf -> codecs[((FriendlyByteBuf) buf).readVarInt()].decode(buf));
        }
        String[] parts = layout.split(";");
        String[] names = new String[parts.length];
        StreamCodec[] codecs = new StreamCodec[parts.length];
        for (int i = 0; i < parts.length; i++) {
            int colon = parts[i].indexOf(':');
            names[i] = parts[i].substring(0, colon);
            codecs[i] = token(parts[i].substring(colon + 1));
        }
        Constructor<?> constructor = null;
        for (Constructor<?> each : owner.getDeclaredConstructors()) {
            if (each.getParameterCount() == parts.length && (constructor == null || each.getParameterTypes().length == parts.length && isPublicOrRecord(owner, each))) {
                constructor = each;
            }
        }
        if (constructor == null) {
            return null;
        }
        Constructor<?> chosen = constructor;
        chosen.setAccessible(true);
        return StreamCodec.of(
            (buf, value) -> {
                for (int i = 0; i < codecs.length; i++) {
                    codecs[i].encode(buf, read(value, names[i]));
                }
            },
            buf -> {
                Object[] values = new Object[codecs.length];
                for (int i = 0; i < codecs.length; i++) {
                    values[i] = adapt(codecs[i].decode(buf), chosen.getParameterTypes()[i]);
                }
                try {
                    return chosen.newInstance(values);
                } catch (ReflectiveOperationException | IllegalArgumentException failure) {
                    throw new io.netty.handler.codec.DecoderException(owner.getName() + " cannot be built from its wire fields", failure);
                }
            });
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Object adapt(Object value, Class<?> wanted) {
        if (value instanceof java.util.List<?> list && !wanted.isAssignableFrom(java.util.List.class)) {
            if (wanted.isAssignableFrom(it.unimi.dsi.fastutil.objects.ReferenceLinkedOpenHashSet.class)) {
                return new it.unimi.dsi.fastutil.objects.ReferenceLinkedOpenHashSet(list);
            }
            if (wanted.isAssignableFrom(java.util.LinkedHashSet.class)) {
                return new java.util.LinkedHashSet<>(list);
            }
        }
        return value;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static StreamCodec directCodec(Class<?> owner) {
        return StreamCodec.of(
            (buf, value) -> {
                throw new io.netty.handler.codec.EncoderException("Direct holders of " + owner.getName() + " are not supported by the Basalt facade");
            },
            buf -> {
                throw new io.netty.handler.codec.DecoderException("Direct holders of " + owner.getName() + " are not supported by the Basalt facade");
            });
    }

    private static boolean isPublicOrRecord(Class<?> owner, Constructor<?> each) {
        return owner.isRecord() || java.lang.reflect.Modifier.isPublic(each.getModifiers());
    }

    private static Object read(Object value, String name) {
        try {
            return value.getClass().getMethod(name).invoke(value);
        } catch (ReflectiveOperationException accessorFailure) {
            try {
                Field field = value.getClass().getDeclaredField(name);
                field.setAccessible(true);
                return field.get(value);
            } catch (ReflectiveOperationException failure) {
                throw new io.netty.handler.codec.EncoderException("Cannot read " + name + " of " + value.getClass().getName(), accessorFailure);
            }
        }
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static StreamCodec token(String token) {
        if (token.startsWith("list(")) {
            return (StreamCodec) token(inner(token)).apply(ByteBufCodecs.list());
        }
        if (token.startsWith("filterable(")) {
            return net.minecraft.server.network.Filterable.streamCodec(token(inner(token)));
        }
        if (token.equals("intlist")) {
            return ByteBufCodecs.INT.apply(ByteBufCodecs.list()).map(it.unimi.dsi.fastutil.ints.IntArrayList::new, java.util.ArrayList::new);
        }
        if (token.startsWith("opt(")) {
            return ByteBufCodecs.optional(token(inner(token)));
        }
        if (token.startsWith("@")) {
            return lookup(token.substring(1));
        }
        if (token.startsWith("rkey:")) {
            String reference = token.substring(5);
            try {
                Object key = Class.forName(reference.substring(0, reference.indexOf('#'))).getField(reference.substring(reference.indexOf('#') + 1)).get(null);
                return ResourceKey.streamCodec((ResourceKey) key);
            } catch (ReflectiveOperationException failure) {
                throw new IllegalArgumentException("Unknown resource key " + reference, failure);
            }
        }
        int colon = token.indexOf(':');
        if (colon > 0) {
            ResourceKey key = registryKey(token.substring(colon + 1));
            return switch (token.substring(0, colon)) {
                case "holderset" -> ByteBufCodecs.holderSet(key);
                case "holder" -> ByteBufCodecs.holderRegistry(key);
                case "reg" -> ByteBufCodecs.registry(key);
                default -> throw new IllegalArgumentException("Unknown codec token " + token);
            };
        }
        return switch (token) {
            case "bool" -> ByteBufCodecs.BOOL;
            case "varint" -> ByteBufCodecs.VAR_INT;
            case "int" -> ByteBufCodecs.INT;
            case "long" -> ByteBufCodecs.LONG;
            case "float" -> ByteBufCodecs.FLOAT;
            case "double" -> ByteBufCodecs.DOUBLE;
            case "byte" -> ByteBufCodecs.BYTE;
            case "short" -> ByteBufCodecs.SHORT;
            case "ushort" -> ByteBufCodecs.UNSIGNED_SHORT;
            case "varlong" -> ByteBufCodecs.VAR_LONG;
            case "string" -> ByteBufCodecs.STRING_UTF8;
            case "bytes" -> ByteBufCodecs.BYTE_ARRAY;
            case "rgb" -> ByteBufCodecs.RGB_COLOR;
            case "nbt" -> ByteBufCodecs.TRUSTED_COMPOUND_TAG;
            case "id" -> net.minecraft.resources.Identifier.STREAM_CODEC;
            case "comp" -> net.minecraft.network.chat.ComponentSerialization.STREAM_CODEC;
            case "unit" -> net.minecraft.util.Unit.STREAM_CODEC;
            default -> throw new IllegalArgumentException("Unknown codec token " + token);
        };
    }

    private static String inner(String token) {
        return token.substring(token.indexOf('(') + 1, token.length() - 1);
    }

    @SuppressWarnings("rawtypes")
    private static ResourceKey registryKey(String name) {
        try {
            return (ResourceKey) Registries.class.getField(name).get(null);
        } catch (ReflectiveOperationException failure) {
            throw new IllegalArgumentException("Unknown registry " + name, failure);
        }
    }

    @SuppressWarnings("rawtypes")
    private static StreamCodec lookup(String reference) {
        int hash = reference.indexOf('#');
        try {
            Class<?> owner = Class.forName(reference.substring(0, hash));
            Field field = owner.getDeclaredField(reference.substring(hash + 1));
            field.setAccessible(true);
            return (StreamCodec) field.get(null);
        } catch (ReflectiveOperationException failure) {
            throw new IllegalArgumentException("No stream codec " + reference, failure);
        }
    }
}
