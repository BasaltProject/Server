// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import net.minecraft.core.Registry;

public interface LevelBasedValue {
    static MapCodec<? extends LevelBasedValue> bootstrap(Registry<MapCodec<? extends LevelBasedValue>> registry) {
        return null;
    }

    static Constant constant(float value) {
        return new Constant(value);
    }

    static Linear perLevel(float base, float perLevelAboveFirst) {
        return new Linear(base, perLevelAboveFirst);
    }

    static Linear perLevel(float perLevel) {
        return new Linear(perLevel, perLevel);
    }

    static Lookup lookup(List<Float> values, LevelBasedValue fallback) {
        return new Lookup(values, fallback);
    }

    float calculate(int level);

    /** Reads the datapack form: a bare number or an object with a `type`. */
    static LevelBasedValue fromData(Object raw) {
        if (raw instanceof Number number) {
            return new Constant(number.floatValue());
        }
        if (!(raw instanceof Map<?, ?> map)) {
            return new Constant(0.0F);
        }
        String type = String.valueOf(map.get("type"));
        switch (type.startsWith("minecraft:") ? type.substring(10) : type) {
            case "linear":
                return new Linear(number(map.get("base")), number(map.get("per_level_above_first")));
            case "levels_squared":
                return new LevelsSquared(number(map.get("added")));
            case "clamped":
                return new Clamped(fromData(map.get("value")), number(map.get("min")), number(map.get("max")));
            case "fraction":
                return new Fraction(fromData(map.get("numerator")), fromData(map.get("denominator")));
            case "exponent":
                return new Exponent(fromData(map.get("base")), fromData(map.get("power")));
            case "lookup":
                List<Float> values = new ArrayList<>();
                if (map.get("values") instanceof List<?> list) {
                    for (Object entry : list) {
                        values.add(number(entry));
                    }
                }
                return new Lookup(values, fromData(map.get("fallback")));
            default:
                return new Constant(number(map.get("value")));
        }
    }

    private static float number(Object value) {
        return value instanceof Number number ? number.floatValue() : 0.0F;
    }

    public static record Constant(float value) implements LevelBasedValue {
        public static final Codec<Constant> CODEC = Codec.FLOAT.xmap(Constant::new, Constant::value);
        public static final MapCodec<Constant> TYPED_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.LevelBasedValue.Constant.class, "TYPED_CODEC");

        @Override
        public float calculate(int level) {
            return value;
        }
    }

    public static record Lookup(List<Float> values, LevelBasedValue fallback) implements LevelBasedValue {
        public static final MapCodec<Lookup> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.LevelBasedValue.Lookup.class, "CODEC");

        @Override
        public float calculate(int level) {
            return level <= values.size() ? values.get(level - 1) : fallback.calculate(level);
        }
    }

    public static record Linear(float base, float perLevelAboveFirst) implements LevelBasedValue {
        public static final MapCodec<Linear> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.LevelBasedValue.Linear.class, "CODEC");

        @Override
        public float calculate(int level) {
            return base + perLevelAboveFirst * (level - 1);
        }
    }

    public static record Clamped(LevelBasedValue value, float min, float max) implements LevelBasedValue {
        public static final MapCodec<Clamped> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.LevelBasedValue.Clamped.class, "CODEC");

        @Override
        public float calculate(int level) {
            return Math.clamp(value.calculate(level), min, max);
        }
    }

    public static record Fraction(LevelBasedValue numerator, LevelBasedValue denominator) implements LevelBasedValue {
        public static final MapCodec<Fraction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.LevelBasedValue.Fraction.class, "CODEC");

        @Override
        public float calculate(int level) {
            float bottom = denominator.calculate(level);
            return bottom == 0.0F ? 0.0F : numerator.calculate(level) / bottom;
        }
    }

    public static record Exponent(LevelBasedValue base, LevelBasedValue power) implements LevelBasedValue {
        public static final MapCodec<Exponent> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.LevelBasedValue.Exponent.class, "CODEC");

        @Override
        public float calculate(int level) {
            return (float) Math.pow(base.calculate(level), power.calculate(level));
        }
    }

    public static record LevelsSquared(float added) implements LevelBasedValue {
        public static final MapCodec<LevelsSquared> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.LevelBasedValue.LevelsSquared.class, "CODEC");

        @Override
        public float calculate(int level) {
            return level * level + added;
        }
    }
}
