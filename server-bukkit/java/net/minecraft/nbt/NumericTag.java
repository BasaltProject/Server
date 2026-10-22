package net.minecraft.nbt;

public interface NumericTag extends PrimitiveTag {
    long longValue();

    int intValue();

    short shortValue();

    byte byteValue();

    double doubleValue();

    float floatValue();

    Number box();
}
