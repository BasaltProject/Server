// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.timeline;
import net.minecraft.util.KeyframeTrack;
import net.minecraft.util.KeyframeTrackSampler;
import com.google.common.collect.HashMultimap;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.Maps;
import com.google.common.collect.Multimap;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.Registries;
import net.minecraft.core.registries.codec.RegistryCodecs;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.Util;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.EnvironmentAttributes;
import net.minecraft.world.attribute.modifier.AttributeModifier;
import net.minecraft.world.clock.ClockManager;
import net.minecraft.world.clock.ClockTimeMarker;
import net.minecraft.world.clock.WorldClock;
public class Timeline {
    public static final Codec<Holder<Timeline>> CODEC = RegistryCodecs.holder(Registries.TIMELINE);
    private static final Codec<Map<EnvironmentAttribute<?>, AttributeTrack<?, ?>>> kTracksCodec = Codec.dispatchedMap(EnvironmentAttributes.CODEC, Util.memoize(AttributeTrack::createCodec));
    public static final Codec<Timeline> DIRECT_CODEC = RecordCodecBuilder.<Timeline>create(i -> i.group(
            WorldClock.CODEC.fieldOf("clock").forGetter(t -> t.clock),
            ExtraCodecs.POSITIVE_INT.optionalFieldOf("period_ticks").forGetter(t -> t.periodTicks),
            kTracksCodec.optionalFieldOf("tracks", Map.of()).forGetter(t -> t.tracks),
            Codec.unboundedMap(ClockTimeMarker.KEY_CODEC, TimeMarkerInfo.CODEC).optionalFieldOf("time_markers", Map.of()).forGetter(t -> t.timeMarkers))
            .apply(i, Timeline::new)).validate(Timeline::validateInternal);
    public static final Codec<Timeline> NETWORK_CODEC = DIRECT_CODEC.xmap(Timeline::syncableOnly, Timeline::syncableOnly);
    private final Holder<WorldClock> clock;
    private final Optional<Integer> periodTicks;
    private final Map<EnvironmentAttribute<?>, AttributeTrack<?, ?>> tracks;
    private final Map<ResourceKey<ClockTimeMarker>, TimeMarkerInfo> timeMarkers;
    private static Timeline syncableOnly(Timeline timeline) {
        Map<EnvironmentAttribute<?>, AttributeTrack<?, ?>> kept = Map.copyOf(Maps.filterKeys(timeline.tracks, EnvironmentAttribute::isSyncable));
        return new Timeline(timeline.clock, timeline.periodTicks, kept, timeline.timeMarkers);
    }
    private Timeline(Holder<WorldClock> clock, Optional<Integer> periodTicks, Map<EnvironmentAttribute<?>, AttributeTrack<?, ?>> tracks,
            Map<ResourceKey<ClockTimeMarker>, TimeMarkerInfo> timeMarkers) {
        this.clock = clock;
        this.periodTicks = periodTicks;
        this.tracks = tracks;
        this.timeMarkers = timeMarkers;
    }
    public static void validateRegistry(Registry<Timeline> timelines, Map<ResourceKey<?>, Exception> loadingErrors) {
        Multimap<Holder<WorldClock>, ResourceKey<ClockTimeMarker>> seen = HashMultimap.create();
        timelines.listElements().forEach(holder -> {
            Holder<WorldClock> owner = holder.value().clock();
            for (ResourceKey<ClockTimeMarker> marker : holder.value().timeMarkers.keySet()) {
                if (!seen.put(owner, marker)) {
                    loadingErrors.put(holder.key(), new IllegalStateException(marker + " was defined multiple times in " + owner.getRegisteredName()));
                }
            }
        });
    }
    private static DataResult<Timeline> validateInternal(Timeline timeline) {
        if (timeline.periodTicks.isEmpty()) {
            return DataResult.success(timeline);
        }
        int period = timeline.periodTicks.get();
        for (Map.Entry<ResourceKey<ClockTimeMarker>, TimeMarkerInfo> entry : timeline.timeMarkers.entrySet()) {
            int at = entry.getValue().ticks();
            if (at < 0 || at >= period) {
                return DataResult.error(() -> "Time Marker " + entry.getKey() + " must be in range [0; " + period + ")");
            }
        }
        DataResult<Timeline> result = DataResult.success(timeline);
        for (AttributeTrack<?, ?> track : timeline.tracks.values()) {
            result = result.apply2stable((t, ignored) -> t, AttributeTrack.validatePeriod(track, period));
        }
        return result;
    }
    public static Builder builder(Holder<WorldClock> clock) {
        return new Builder(clock);
    }
    public int getPeriodCount(ClockManager clockManager) {
        return periodTicks.isEmpty() ? 0 : (int) (getTotalTicks(clockManager) / periodTicks.get());
    }
    public long getCurrentTicks(ClockManager clockManager) {
        long total = getTotalTicks(clockManager);
        return periodTicks.isEmpty() ? total : total % periodTicks.get();
    }
    public long getTotalTicks(ClockManager clockManager) {
        return clockManager.getInstance(clock).totalTicks();
    }
    public Holder<WorldClock> clock() {
        return clock;
    }
    public Optional<Integer> periodTicks() {
        return periodTicks;
    }
    public void registerTimeMarkers(BiConsumer<ResourceKey<ClockTimeMarker>, ClockTimeMarker> output) {
        timeMarkers.forEach((key, info) -> output.accept(key, new ClockTimeMarker(clock, info.ticks, periodTicks, info.showInCommands)));
    }
    public Set<EnvironmentAttribute<?>> attributes() {
        return tracks.keySet();
    }
    @SuppressWarnings("unchecked")
    public <Value> AttributeTrackSampler<Value, ?> createTrackSampler(EnvironmentAttribute<Value> attribute, ClockManager clockManager) {
        AttributeTrack<Value, ?> track = (AttributeTrack<Value, ?>) tracks.get(attribute);
        if (track == null) {
            throw new IllegalStateException("Timeline has no track for " + attribute);
        }
        return track.bakeSampler(attribute, clock, periodTicks, clockManager);
    }
    private static record TimeMarkerInfo(int ticks, boolean showInCommands) {
        private static final Codec<TimeMarkerInfo> kFull = RecordCodecBuilder.create(i -> i.group(
                ExtraCodecs.NON_NEGATIVE_INT.fieldOf("ticks").forGetter(TimeMarkerInfo::ticks),
                Codec.BOOL.optionalFieldOf("show_in_commands", false).forGetter(TimeMarkerInfo::showInCommands)).apply(i, TimeMarkerInfo::new));
        public static final Codec<TimeMarkerInfo> CODEC = Codec.either(ExtraCodecs.NON_NEGATIVE_INT, kFull).xmap(
                either -> either.map(t -> new TimeMarkerInfo(t, false), t -> t),
                marker -> marker.showInCommands ? Either.right(marker) : Either.left(marker.ticks));
    }
    public static class Builder {
        private final Holder<WorldClock> clock;
        private Optional<Integer> periodTicks = Optional.empty();
        private final ImmutableMap.Builder<EnvironmentAttribute<?>, AttributeTrack<?, ?>> tracks = ImmutableMap.builder();
        private final ImmutableMap.Builder<ResourceKey<ClockTimeMarker>, TimeMarkerInfo> timeMarkers = ImmutableMap.builder();
        private Builder(Holder<WorldClock> clock) {
            this.clock = clock;
        }
        public Builder setPeriodTicks(int periodTicks) {
            this.periodTicks = Optional.of(periodTicks);
            return this;
        }
        public <Value, Argument> Builder addModifierTrack(EnvironmentAttribute<Value> attribute, AttributeModifier<Value, Argument> modifier,
                Consumer<KeyframeTrack.Builder<Argument>> builder) {
            attribute.type().checkAllowedModifier(modifier);
            KeyframeTrack.Builder<Argument> keyframes = new KeyframeTrack.Builder<>();
            builder.accept(keyframes);
            tracks.put(attribute, new AttributeTrack<>(modifier, keyframes.build()));
            return this;
        }
        public <Value> Builder addTrack(EnvironmentAttribute<Value> attribute, Consumer<KeyframeTrack.Builder<Value>> builder) {
            return addModifierTrack(attribute, AttributeModifier.override(), builder);
        }
        public Builder addTimeMarker(ResourceKey<ClockTimeMarker> id, int ticks) {
            return addTimeMarker(id, ticks, false);
        }
        public Builder addTimeMarker(ResourceKey<ClockTimeMarker> id, int ticks, boolean showInCommands) {
            timeMarkers.put(id, new TimeMarkerInfo(ticks, showInCommands));
            return this;
        }
        public Timeline build() {
            return new Timeline(clock, periodTicks, tracks.build(), timeMarkers.build());
        }
    }
}
