// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.world.attribute.LerpFunction;

public class KeyframeTrackSampler<T> {
    private final Optional<Integer> periodTicks;
    private final LerpFunction<T> lerp;
    private final List<Segment<T>> segments;

    KeyframeTrackSampler(KeyframeTrack<T> track, Optional<Integer> periodTicks, LerpFunction<T> lerp) {
        this.periodTicks = periodTicks;
        this.lerp = lerp;
        this.segments = bake(track, periodTicks);
    }

    private static <T> List<Segment<T>> bake(KeyframeTrack<T> track, Optional<Integer> periodTicks) {
        List<Keyframe<T>> frames = track.keyframes();
        if (frames.size() == 1) {
            T only = frames.get(0).value();
            return List.of(new Segment<>(EasingType.CONSTANT, only, 0, only, 0));
        }
        List<Segment<T>> result = new ArrayList<>();
        Keyframe<T> first = frames.get(0);
        Keyframe<T> last = frames.get(frames.size() - 1);
        if (periodTicks.isPresent()) {
            result.add(new Segment<>(track, last, last.ticks() - periodTicks.get(), first, first.ticks()));
        }
        for (int i = 0; i < frames.size() - 1; i++) {
            result.add(new Segment<>(track, frames.get(i), frames.get(i).ticks(), frames.get(i + 1), frames.get(i + 1).ticks()));
        }
        if (periodTicks.isPresent()) {
            result.add(new Segment<>(track, last, last.ticks(), first, first.ticks() + periodTicks.get()));
        }
        return List.copyOf(result);
    }

    public T sample(long ticks) {
        long local = periodTicks.isPresent() ? Math.floorMod(ticks, periodTicks.get()) : ticks;
        Segment<T> segment = segmentAt(local);
        if (local <= segment.fromTicks) {
            return segment.fromValue;
        }
        if (local >= segment.toTicks) {
            return segment.toValue;
        }
        float alpha = (float) (local - segment.fromTicks) / (float) (segment.toTicks - segment.fromTicks);
        return lerp.apply(segment.easing.apply(alpha), segment.fromValue, segment.toValue);
    }

    private Segment<T> segmentAt(long ticks) {
        for (Segment<T> segment : segments) {
            if (ticks < segment.toTicks) {
                return segment;
            }
        }
        return segments.get(segments.size() - 1);
    }

    static record Segment<T>(EasingType easing, T fromValue, int fromTicks, T toValue, int toTicks) {
        public Segment(KeyframeTrack<T> track, Keyframe<T> from, int fromTicks, Keyframe<T> to, int toTicks) {
            this(track.easingType(), from.value(), fromTicks, to.value(), toTicks);
        }
    }
}
