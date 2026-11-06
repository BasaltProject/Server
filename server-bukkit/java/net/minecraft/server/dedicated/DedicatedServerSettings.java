// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dedicated;

import java.nio.file.Path;
import java.util.function.UnaryOperator;

public class DedicatedServerSettings {
    private final Path source;
    private DedicatedServerProperties properties;

    public DedicatedServerSettings(Path source) {
        this.source = source;
        this.properties = java.nio.file.Files.exists(source) ? DedicatedServerProperties.fromFile(source) : new DedicatedServerProperties(new java.util.Properties());
    }

    public DedicatedServerProperties getProperties() {
        return properties;
    }

    public void forceSave() {
        properties.store(source);
    }

    public DedicatedServerSettings update(UnaryOperator<DedicatedServerProperties> mutator) {
        properties = mutator.apply(properties);
        forceSave();
        return this;
    }
}
