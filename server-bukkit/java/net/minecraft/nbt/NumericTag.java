// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.util.Optional;

public interface NumericTag extends PrimitiveTag {
    long longValue();

    int intValue();

    short shortValue();

    byte byteValue();

    double doubleValue();

    float floatValue();

    Number box();
    default Optional<Number> asNumber() {
        return Optional.of(box());
    }
    default Optional<Byte> asByte() {
        return Optional.of(byteValue());
    }
    default Optional<Short> asShort() {
        return Optional.of(shortValue());
    }
    default Optional<Integer> asInt() {
        return Optional.of(intValue());
    }
    default Optional<Long> asLong() {
        return Optional.of(longValue());
    }
    default Optional<Float> asFloat() {
        return Optional.of(floatValue());
    }
    default Optional<Double> asDouble() {
        return Optional.of(doubleValue());
    }
    default Optional<Boolean> asBoolean() {
        return Optional.of(byteValue() != 0);
    }
}
