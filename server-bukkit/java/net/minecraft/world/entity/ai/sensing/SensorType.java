// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.function.Supplier;

public class SensorType<U extends Sensor<?>> {
    private final Supplier<U> factory;

    public SensorType(Supplier<U> factory) {
        this.factory = factory;
    }

    public U create() {
        return factory.get();
    }
}
