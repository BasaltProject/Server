// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dedicated;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Properties;
import java.util.function.Function;
import java.util.function.IntFunction;
import java.util.function.Supplier;
import java.util.function.UnaryOperator;
import net.minecraft.core.RegistryAccess;

public abstract class Settings<T extends Settings<T>> {
    protected final Properties properties;

    public Settings(Properties properties) {
        this.properties = properties;
    }

    public static Properties loadFromFile(Path file) {
        Properties loaded = new Properties();
        try (InputStream in = Files.newInputStream(file)) {
            loaded.load(in);
        } catch (IOException failure) {
            throw new java.io.UncheckedIOException(failure);
        }
        return loaded;
    }

    public void store(Path output) {
        try (OutputStream out = Files.newOutputStream(output)) {
            properties.store(out, "Minecraft server properties");
        } catch (IOException failure) {
            throw new java.io.UncheckedIOException(failure);
        }
    }

    protected static <V> Function<String, V> dispatchNumberOrString(IntFunction<V> intDeserializer, Function<String, V> stringDeserializer) {
        return text -> {
            try {
                return intDeserializer.apply(Integer.parseInt(text));
            } catch (NumberFormatException notNumber) {
                return stringDeserializer.apply(text);
            }
        };
    }

    private String raw(String key) {
        return properties.getProperty(key);
    }

    protected <V> V getLegacy(String key, Function<String, V> deserializer) {
        String value = raw(key);
        if (value == null) {
            return null;
        }
        properties.remove(key);
        return deserializer.apply(value);
    }

    protected <V> V get(String key, Function<String, V> deserializer, Function<V, String> serializer, V defaultValue) {
        return get(key, deserializer, value -> value, serializer, defaultValue);
    }

    protected <V> MutableValue<V> getMutable(String key, Function<String, V> deserializer, Function<V, String> serializer, V defaultValue) {
        V value = get(key, deserializer, serializer, defaultValue);
        return new MutableValue<>(key, value, serializer);
    }

    protected <V> V get(String key, Function<String, V> deserializer, UnaryOperator<V> validator, Function<V, String> serializer, V defaultValue) {
        String text = raw(key);
        V value = defaultValue;
        if (text != null) {
            try {
                V parsed = deserializer.apply(text);
                value = parsed == null ? defaultValue : validator.apply(parsed);
            } catch (RuntimeException invalid) {
                value = defaultValue;
            }
        }
        properties.setProperty(key, serializer.apply(value));
        return value;
    }

    protected <V> V get(String key, Function<String, V> deserializer, V defaultValue) {
        return get(key, deserializer, String::valueOf, defaultValue);
    }

    protected <V> MutableValue<V> getMutable(String key, Function<String, V> deserializer, V defaultValue) {
        return getMutable(key, deserializer, String::valueOf, defaultValue);
    }

    protected String get(String key, String defaultValue) {
        return get(key, text -> text, text -> text, defaultValue);
    }

    protected String getLegacyString(String key) {
        return getLegacy(key, text -> text);
    }

    protected int get(String key, int defaultValue) {
        return get(key, Integer::parseInt, Integer.valueOf(defaultValue));
    }

    protected MutableValue<Integer> getMutable(String key, int defaultValue) {
        return getMutable(key, Integer::parseInt, Integer.valueOf(defaultValue));
    }

    protected MutableValue<String> getMutable(String key, String defaultValue) {
        return getMutable(key, text -> text, text -> text, defaultValue);
    }

    protected int get(String key, UnaryOperator<Integer> validator, int defaultValue) {
        return get(key, Integer::parseInt, validator, String::valueOf, Integer.valueOf(defaultValue));
    }

    protected long get(String key, long defaultValue) {
        return get(key, Long::parseLong, Long.valueOf(defaultValue));
    }

    protected boolean get(String key, boolean defaultValue) {
        return get(key, Boolean::parseBoolean, Boolean.valueOf(defaultValue));
    }

    protected MutableValue<Boolean> getMutable(String key, boolean defaultValue) {
        return getMutable(key, Boolean::parseBoolean, Boolean.valueOf(defaultValue));
    }

    protected Boolean getLegacyBoolean(String key) {
        return getLegacy(key, Boolean::valueOf);
    }

    protected Properties cloneProperties() {
        Properties copy = new Properties();
        copy.putAll(properties);
        return copy;
    }

    protected abstract T reload(RegistryAccess registryAccess, Properties properties);

    public class MutableValue<V> implements Supplier<V> {
        private final String key;
        private final V value;
        private final Function<V, String> serializer;

        private MutableValue(String key, V value, Function<V, String> serializer) {
            this.key = key;
            this.value = value;
            this.serializer = serializer;
        }

        @Override
        public V get() {
            return value;
        }

        public T update(RegistryAccess registryAccess, V newValue) {
            Properties updated = cloneProperties();
            updated.put(key, serializer.apply(newValue));
            return reload(registryAccess, updated);
        }
    }
}
