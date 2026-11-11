// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.timeline;
import net.minecraft.util.KeyframeTrack;
import net.minecraft.util.KeyframeTrackSampler;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.attribute.EnvironmentAttributeLayer;
import net.minecraft.world.attribute.LerpFunction;
import net.minecraft.world.attribute.modifier.AttributeModifier;
import net.minecraft.world.clock.ClockManager;
import net.minecraft.world.clock.WorldClock;
public class AttributeTrackSampler<Value, Argument> implements EnvironmentAttributeLayer.TimeBased<Value> {
    private final Holder<WorldClock> clock;
    private final AttributeModifier<Value, Argument> modifier;
    private final KeyframeTrackSampler<Argument> sampler;
    private final ClockManager clocks;
    private int cachedTick;
    private Argument cached;
    public AttributeTrackSampler(Holder<WorldClock> clock, Optional<Integer> periodTicks, AttributeModifier<Value, Argument> modifier,
            KeyframeTrack<Argument> argumentTrack, LerpFunction<Argument> argumentLerp, ClockManager clockManager) {
        this.clock = clock;
        this.modifier = modifier;
        this.clocks = clockManager;
        this.sampler = argumentTrack.bakeSampler(periodTicks, argumentLerp);
    }
    public Value applyTimeBased(Value baseValue, int cacheTickId) {
        if (cached == null || cacheTickId != cachedTick) {
            cachedTick = cacheTickId;
            cached = sampler.sample(clocks.getInstance(clock).totalTicks());
        }
        return modifier.apply(baseValue, cached);
    }
}
