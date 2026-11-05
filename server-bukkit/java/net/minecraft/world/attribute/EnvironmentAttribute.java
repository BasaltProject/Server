// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.Codec;

public class EnvironmentAttribute<Value> {
    private final AttributeType<Value> type;
    private final Value defaultValue;
    private final AttributeRange<Value> valueRange;
    private final boolean isSyncable;
    private final boolean isPositional;
    private final boolean isSpatiallyInterpolated;
    private final boolean fullResolutionBiomes;

    private EnvironmentAttribute(AttributeType<Value> type, Value defaultValue, AttributeRange<Value> valueRange, boolean isSyncable, boolean isPositional, boolean isSpatiallyInterpolated, boolean fullResolutionBiomes) {
        this.type = type;
        this.defaultValue = defaultValue;
        this.valueRange = valueRange;
        this.isSyncable = isSyncable;
        this.isPositional = isPositional;
        this.isSpatiallyInterpolated = isSpatiallyInterpolated;
        this.fullResolutionBiomes = fullResolutionBiomes;
    }

    public static <Value> Builder<Value> builder(AttributeType<Value> type) {
        return new Builder<>(type);
    }

    public AttributeType<Value> type() {
        return type;
    }

    public Value defaultValue() {
        return defaultValue;
    }

    public Codec<Value> valueCodec() {
        return type.valueCodec();
    }

    public Value sanitizeValue(Value value) {
        return valueRange.sanitize(value);
    }

    public boolean isSyncable() {
        return isSyncable;
    }

    public boolean isPositional() {
        return isPositional;
    }

    public boolean isSpatiallyInterpolated() {
        return isSpatiallyInterpolated;
    }

    public boolean isFullResolutionBiomes() {
        return fullResolutionBiomes;
    }

    @Override
    public String toString() {
        net.minecraft.resources.Identifier key = net.minecraft.core.registries.BuiltInRegistries.ENVIRONMENT_ATTRIBUTE.getKey(this);
        return key == null ? super.toString() : key.toString();
    }

    public static class Builder<Value> {
        private final AttributeType<Value> type;
        private Value defaultValue;
        private AttributeRange<Value> valueRange = AttributeRange.any();
        private boolean isSyncable;
        private boolean isPositional = true;
        private boolean isSpatiallyInterpolated;
        private boolean fullResolutionBiomes;

        public Builder(AttributeType<Value> type) {
            this.type = type;
        }

        public Builder<Value> defaultValue(Value defaultValue) {
            this.defaultValue = defaultValue;
            return this;
        }

        public Builder<Value> valueRange(AttributeRange<Value> valueRange) {
            this.valueRange = valueRange;
            return this;
        }

        public Builder<Value> syncable() {
            this.isSyncable = true;
            return this;
        }

        public Builder<Value> notPositional() {
            this.isPositional = false;
            return this;
        }

        public Builder<Value> spatiallyInterpolated() {
            this.isSpatiallyInterpolated = true;
            return this;
        }

        public Builder<Value> fullResolutionBiomes() {
            this.fullResolutionBiomes = true;
            return this;
        }

        public EnvironmentAttribute<Value> build() {
            return new EnvironmentAttribute<>(type, defaultValue, valueRange, isSyncable, isPositional, isSpatiallyInterpolated, fullResolutionBiomes);
        }
    }
}
