package net.minecraft.resources;

import java.util.Objects;

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
}
