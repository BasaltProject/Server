// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public interface IntRangePredicate extends Validatable {
    Codec<Point> POINT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.IntRangePredicate.class, "POINT_CODEC");
    Codec<Line> LINE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.IntRangePredicate.class, "LINE_CODEC");
    Codec<IntRangePredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.IntRangePredicate.class, "CODEC");
    static IntRangePredicate exact(int value) {
        return new Point(ContextIntProviders.exactly(value));
    }
    static IntRangePredicate range(int min, int max) {
        return new Line(Optional.of(ContextIntProviders.exactly(min)), Optional.of(ContextIntProviders.exactly(max)));
    }
    static IntRangePredicate lowerBound(int value) {
        return new Line(Optional.of(ContextIntProviders.exactly(value)), Optional.empty());
    }
    static IntRangePredicate upperBound(int value) {
        return new Line(Optional.empty(), Optional.of(ContextIntProviders.exactly(value)));
    }
    boolean test(LootContext context, int input);
    default boolean test(LootContext context, ContextIntProvider input) {
        return test(context, input.getInt(context));
    }
    public static record Point(Holder<ContextIntProvider> value) implements IntRangePredicate {
        public void validate(ValidationContext context) {
            Validatable.validateHolder(context, value);
        }
        public boolean test(LootContext context, int input) {
            return value.value().getInt(context) == input;

        }
    }
    public static final class Line implements IntRangePredicate {
        private final Optional<Holder<ContextIntProvider>> min;
        private final Optional<Holder<ContextIntProvider>> max;
        private final IntChecker bounds;
        private Line(Optional<Holder<ContextIntProvider>> min, Optional<Holder<ContextIntProvider>> max) {
            this.min = min;
            this.max = max;
            Holder<ContextIntProvider> lo = min.orElse(null);
            Holder<ContextIntProvider> hi = max.orElse(null);
            if (lo != null && hi != null) {
                this.bounds = (ctx, v) -> v >= lo.value().getInt(ctx) && v <= hi.value().getInt(ctx);
            } else if (lo != null) {
                this.bounds = (ctx, v) -> v >= lo.value().getInt(ctx);
            } else if (hi != null) {
                this.bounds = (ctx, v) -> v <= hi.value().getInt(ctx);
            } else {
                this.bounds = null;
            }
        }
        public void validate(ValidationContext context) {
            Validatable.validateHolder(context, "min", min);
            Validatable.validateHolder(context, "max", max);
        }
        public boolean test(LootContext context, int input) {
            return bounds == null || bounds.test(context, input);
        }
        public boolean test(LootContext context, ContextIntProvider input) {
            return bounds == null || bounds.test(context, input.getInt(context));
        }
        interface IntChecker {
            boolean test(LootContext context, int value);
        }
    }
}
