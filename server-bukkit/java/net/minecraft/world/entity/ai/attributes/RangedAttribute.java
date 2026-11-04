// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.attributes;

import net.minecraft.util.Mth;

public class RangedAttribute extends Attribute {
    private final double minValue;
    private final double maxValue;

    public RangedAttribute(String descriptionId, double defaultValue, double minValue, double maxValue) {
        super(descriptionId, defaultValue);
        if (minValue > maxValue) {
            throw new IllegalArgumentException("Minimum value cannot be bigger than maximum value!");
        }
        if (defaultValue < minValue || defaultValue > maxValue) {
            throw new IllegalArgumentException("Default value cannot be outside of minimum and maximum value!");
        }
        this.minValue = minValue;
        this.maxValue = maxValue;
    }

    public double getMinValue() {
        return minValue;
    }

    public double getMaxValue() {
        return maxValue;
    }

    @Override
    public double sanitizeValue(double value) {
        return Double.isNaN(value) ? minValue : Mth.clamp(value, minValue, maxValue);
    }
}
