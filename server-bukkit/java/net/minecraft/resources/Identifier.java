package net.minecraft.resources;

public class Identifier {
    private final String key;
    private final String namespace;

    protected Identifier(String namespace, String key) {
        this.namespace = namespace;
        this.key = key;
    }

    @Override
    public String toString() {
        return namespace + ":" + key;
    }
}
