// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import java.util.Objects;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import io.netty.buffer.ByteBuf;
import java.nio.file.Path;
import java.util.function.UnaryOperator;
import net.minecraft.network.codec.StreamCodec;

public class Identifier implements Comparable<Identifier> {
    public static final String DEFAULT_NAMESPACE = "minecraft";
    private final String namespace;
    private final String key;

    protected Identifier(String namespace, String key) {
        this.namespace = namespace;
        this.key = key;
    }

    public static Identifier fromNamespaceAndPath(String namespace, String path) {
        if (!isValidNamespace(namespace) || !isValidPath(path)) {
            throw new IllegalArgumentException("Invalid identifier " + namespace + ":" + path);
        }
        return new Identifier(namespace, path);
    }

    public static Identifier parse(String id) {
        Identifier parsed = tryParse(id);
        if (parsed == null) {
            throw new IllegalArgumentException("Invalid identifier " + id);
        }
        return parsed;
    }

    public static Identifier tryParse(String id) {
        if (id == null) {
            return null;
        }
        int colon = id.indexOf(':');
        String namespace = colon < 0 ? DEFAULT_NAMESPACE : colon == 0 ? DEFAULT_NAMESPACE : id.substring(0, colon);
        String path = id.substring(colon + 1);
        return isValidNamespace(namespace) && isValidPath(path) ? new Identifier(namespace, path) : null;
    }

    public static Identifier withDefaultNamespace(String path) {
        return fromNamespaceAndPath(DEFAULT_NAMESPACE, path);
    }

    public static boolean isValidNamespace(String namespace) {
        for (int i = 0; i < namespace.length(); i++) {
            char c = namespace.charAt(i);
            if (!(c == '_' || c == '-' || c == '.' || c >= 'a' && c <= 'z' || c >= '0' && c <= '9')) {
                return false;
            }
        }
        return true;
    }

    public static boolean isValidPath(String path) {
        for (int i = 0; i < path.length(); i++) {
            char c = path.charAt(i);
            if (!(c == '_' || c == '-' || c == '.' || c == '/' || c >= 'a' && c <= 'z' || c >= '0' && c <= '9')) {
                return false;
            }
        }
        return true;
    }

    public String getNamespace() {
        return namespace;
    }

    public String getPath() {
        return key;
    }

    public Identifier withPath(String path) {
        return fromNamespaceAndPath(namespace, path);
    }

    @Override
    public boolean equals(Object other) {
        return this == other || other instanceof Identifier id && namespace.equals(id.namespace) && key.equals(id.key);
    }

    @Override
    public int hashCode() {
        return Objects.hash(namespace, key);
    }

    @Override
    public int compareTo(Identifier other) {
        int result = key.compareTo(other.key);
        return result != 0 ? result : namespace.compareTo(other.namespace);
    }

    @Override
    public String toString() {
        return namespace + ":" + key;
    }
    public static final Codec<Identifier> CODEC = Codec.STRING.comapFlatMap(Identifier::read, Identifier::toString);
    public static final StreamCodec<ByteBuf, Identifier> STREAM_CODEC =
        net.minecraft.network.codec.ByteBufCodecs.STRING_UTF8.map(Identifier::parse, Identifier::toString);
    public static final SimpleCommandExceptionType ERROR_INVALID =
        new SimpleCommandExceptionType(new com.mojang.brigadier.LiteralMessage("Invalid ID"));
    public static final char NAMESPACE_SEPARATOR = ':';
    public static final String REALMS_NAMESPACE = "realms";
    public static final String ALLOWED_NAMESPACE_CHARACTERS = "_-.0123456789abcdefghijklmnopqrstuvwxyz";
    public static Identifier tryBuild(String namespace, String path) {
        return isValidNamespace(namespace) && isValidPath(path) ? new Identifier(namespace, path) : null;
    }
    public static Identifier bySeparator(String identifier, char separator) {
        int index = identifier.indexOf(separator);
        if (index < 0) {
            return withDefaultNamespace(identifier);
        }
        String namespace = identifier.substring(0, index);
        return fromNamespaceAndPath(namespace.isEmpty() ? DEFAULT_NAMESPACE : namespace, identifier.substring(index + 1));
    }
    public static Identifier tryBySeparator(String identifier, char separator) {
        int index = identifier.indexOf(separator);
        if (index < 0) {
            return tryBuild(DEFAULT_NAMESPACE, identifier);
        }
        String namespace = identifier.substring(0, index);
        return tryBuild(namespace.isEmpty() ? DEFAULT_NAMESPACE : namespace, identifier.substring(index + 1));
    }
    public static DataResult<Identifier> read(String input) {
        Identifier parsed = tryParse(input);
        return parsed != null ? DataResult.success(parsed) : DataResult.error(() -> "Not a valid resource location: " + input);
    }
    public Identifier withPath(UnaryOperator<String> modifier) {
        return withPath(modifier.apply(key));
    }
    public Identifier withPrefix(String prefix) {
        return withPath(prefix + key);
    }
    public Identifier withSuffix(String suffix) {
        return withPath(key + suffix);
    }
    public Path resolveAgainst(Path root) {
        return root.resolve(namespace).resolve(key);
    }
    public String toDebugFileName() {
        return toString().replace('/', '_').replace(':', '_');
    }
    public String toLanguageKey() {
        return namespace + "." + key;
    }
    public String toShortLanguageKey() {
        return DEFAULT_NAMESPACE.equals(namespace) ? key : toLanguageKey();
    }
    public String toShortString() {
        return DEFAULT_NAMESPACE.equals(namespace) ? key : toString();
    }
    public String toLanguageKey(String prefix) {
        return prefix + "." + toLanguageKey();
    }
    public String toLanguageKey(String prefix, String suffix) {
        return prefix + "." + toLanguageKey() + "." + suffix;
    }
    public static Identifier read(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        while (reader.canRead() && isAllowedInIdentifier(reader.peek())) {
            reader.skip();
        }
        String text = reader.getString().substring(start, reader.getCursor());
        Identifier parsed = tryParse(text);
        if (parsed == null) {
            reader.setCursor(start);
            throw ERROR_INVALID.createWithContext(reader);
        }
        return parsed;
    }
    public static Identifier readNonEmpty(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        Identifier parsed = read(reader);
        if (reader.getCursor() == start) {
            throw ERROR_INVALID.createWithContext(reader);
        }
        return parsed;
    }
    public static boolean isAllowedInIdentifier(char c) {
        return c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c == '_' || c == ':' || c == '/' || c == '.' || c == '-';
    }
    public static boolean validPathChar(char c) {
        return c == '_' || c == '-' || c == '.' || c == '/' || c >= 'a' && c <= 'z' || c >= '0' && c <= '9';
    }
}
