// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.clock.WorldClock;
import net.minecraft.world.level.storage.loot.IntRangePredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public record TimeCheck(Holder<WorldClock> clock, Optional<Long> period, IntRangePredicate value) implements LootItemCondition {
    public static final MapCodec<TimeCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.TimeCheck.class, "MAP_CODEC");
    public MapCodec<TimeCheck> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void validate(ValidationContext context) {
    }
    public boolean test(LootContext context) {
        long ticks = context.getLevel().clockManager().getInstance(clock).totalTicks();
        if (period.isPresent()) {
            ticks %= period.get();
        }
        return value.test(context, (int) ticks);
    }
    public static Builder time(Holder<WorldClock> clock, IntRangePredicate value) {
        return new Builder(clock, value);
    }
    public static class Builder implements LootItemCondition.Builder {
        private Optional<Long> period = Optional.empty();
        private Holder<WorldClock> clockHolder;
        private IntRangePredicate range;
        public Builder(Holder<WorldClock> clock, IntRangePredicate value) {
            this.clockHolder = clock;
            this.range = value;
        }
        public Builder setPeriod(long period) {
            this.period = Optional.of(period);
            return this;
        }
        public TimeCheck build() {
            return new TimeCheck(clockHolder, period, range);
        }
    }
}
