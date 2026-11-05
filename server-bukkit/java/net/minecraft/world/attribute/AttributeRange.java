// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.DataResult;

public interface AttributeRange<Value> {
    AttributeRange<Float> UNIT_FLOAT = ofFloat(0.0F, 1.0F);
    AttributeRange<Float> UNIT_FLOAT_EPSILON = ofFloat(1.0E-5F, 1.0F);
    AttributeRange<Float> NON_NEGATIVE_FLOAT = ofFloat(0.0F, Float.POSITIVE_INFINITY);

    @SuppressWarnings("unchecked")
    static <Value> AttributeRange<Value> any() {
        return (AttributeRange<Value>) Unbounded.INSTANCE;
    }

    static AttributeRange<Float> ofFloat(float minValue, float maxValue) {
        return new Bounded(minValue, maxValue);
    }

    DataResult<Value> validate(Value value);

    Value sanitize(Value value);

    final class Unbounded implements AttributeRange<Object> {
        static final Unbounded INSTANCE = new Unbounded();

        @Override
        public DataResult<Object> validate(Object value) {
            return DataResult.success(value);
        }

        @Override
        public Object sanitize(Object value) {
            return value;
        }
    }

    record Bounded(float min, float max) implements AttributeRange<Float> {
        @Override
        public DataResult<Float> validate(Float value) {
            return value >= min && value <= max
                ? DataResult.success(value)
                : DataResult.error(() -> value + " is outside of [" + min + ", " + max + "]");
        }

        @Override
        public Float sanitize(Float value) {
            return Math.max(min, Math.min(max, value));
        }
    }
}
