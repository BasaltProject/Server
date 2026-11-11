// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public interface FloatRangePredicate extends Validatable {
    Codec<Point> POINT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.FloatRangePredicate.class, "POINT_CODEC");
    Codec<Line> LINE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.FloatRangePredicate.class, "LINE_CODEC");
    Codec<FloatRangePredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.FloatRangePredicate.class, "CODEC");
    static FloatRangePredicate exact(float value) {
        return new Point(ContextFloatProviders.exactly(value));
    }
    static FloatRangePredicate range(float min, float max) {
        return new Line(Optional.of(ContextFloatProviders.exactly(min)), Optional.of(ContextFloatProviders.exactly(max)));
    }
    static FloatRangePredicate lowerBound(float value) {
        return new Line(Optional.of(ContextFloatProviders.exactly(value)), Optional.empty());
    }
    static FloatRangePredicate upperBound(float value) {
        return new Line(Optional.empty(), Optional.of(ContextFloatProviders.exactly(value)));
    }
    boolean test(LootContext context, float input);
    default boolean test(LootContext context, ContextFloatProvider input) {
        return test(context, input.getFloat(context));
    }
    public static record Point(Holder<ContextFloatProvider> value) implements FloatRangePredicate {
        public void validate(ValidationContext context) {
            Validatable.validateHolder(context, value);
        }
        public boolean test(LootContext context, float input) {
            return value.value().getFloat(context) == input;

        }
    }
    public static final class Line implements FloatRangePredicate {
        private final Optional<Holder<ContextFloatProvider>> min;
        private final Optional<Holder<ContextFloatProvider>> max;
        private final FloatChecker bounds;
        private Line(Optional<Holder<ContextFloatProvider>> min, Optional<Holder<ContextFloatProvider>> max) {
            this.min = min;
            this.max = max;
            Holder<ContextFloatProvider> lo = min.orElse(null);
            Holder<ContextFloatProvider> hi = max.orElse(null);
            if (lo != null && hi != null) {
                this.bounds = (ctx, v) -> v >= lo.value().getFloat(ctx) && v <= hi.value().getFloat(ctx);
            } else if (lo != null) {
                this.bounds = (ctx, v) -> v >= lo.value().getFloat(ctx);
            } else if (hi != null) {
                this.bounds = (ctx, v) -> v <= hi.value().getFloat(ctx);
            } else {
                this.bounds = null;
            }
        }
        public void validate(ValidationContext context) {
            Validatable.validateHolder(context, "min", min);
            Validatable.validateHolder(context, "max", max);
        }
        public boolean test(LootContext context, float input) {
            return bounds == null || bounds.test(context, input);
        }
        public boolean test(LootContext context, ContextFloatProvider input) {
            return bounds == null || bounds.test(context, input.getFloat(context));
        }
        interface FloatChecker {
            boolean test(LootContext context, float value);
        }
    }
}
