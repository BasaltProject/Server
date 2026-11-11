// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.concurrent.TimeUnit;
import java.util.function.LongSupplier;

public interface TimeSource {
    long get(TimeUnit timeUnit);

    static NanoTimeSource constant(long value) {
        return () -> value;
    }

    public interface NanoTimeSource extends LongSupplier, TimeSource {
        @Override
        default long get(TimeUnit timeUnit) {
            return timeUnit.convert(getAsLong(), TimeUnit.NANOSECONDS);
        }
    }
}
