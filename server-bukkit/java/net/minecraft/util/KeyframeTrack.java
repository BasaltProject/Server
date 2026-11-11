// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.world.attribute.LerpFunction;

public record KeyframeTrack<T>(List<Keyframe<T>> keyframes, EasingType easingType) {
    public KeyframeTrack {
        if (keyframes.isEmpty()) {
            throw new IllegalArgumentException("Track has no keyframes");
        }
    }

    public static <T> MapCodec<KeyframeTrack<T>> mapCodec(Codec<T> valueCodec) {
        Codec<List<Keyframe<T>>> keyframes = Keyframe.codec(valueCodec).listOf().validate(KeyframeTrack::validateKeyframes);
        return RecordCodecBuilder.mapCodec(instance -> instance.group(
            keyframes.fieldOf("keyframes").forGetter(KeyframeTrack::keyframes),
            EasingType.CODEC.optionalFieldOf("ease", EasingType.LINEAR).forGetter(KeyframeTrack::easingType)).apply(instance, KeyframeTrack::new));
    }

    private static <T> DataResult<List<Keyframe<T>>> validateKeyframes(List<Keyframe<T>> keyframes) {
        if (keyframes.isEmpty()) {
            return DataResult.error(() -> "Keyframe track needs at least one keyframe");
        }
        for (int i = 1; i < keyframes.size(); i++) {
            if (keyframes.get(i).ticks() <= keyframes.get(i - 1).ticks()) {
                return DataResult.error(() -> "Keyframes must be in strictly ascending tick order");
            }
        }
        return DataResult.success(keyframes);
    }

    public static DataResult<KeyframeTrack<?>> validatePeriod(KeyframeTrack<?> track, int periodTicks) {
        for (Keyframe<?> keyframe : track.keyframes()) {
            if (keyframe.ticks() < 0 || keyframe.ticks() > periodTicks) {
                return DataResult.error(() -> "Keyframe at tick " + keyframe.ticks() + " must be in range [0; " + periodTicks + "]");
            }
        }
        return DataResult.success(track);
    }

    public KeyframeTrackSampler<T> bakeSampler(Optional<Integer> periodTicks, LerpFunction<T> lerp) {
        return new KeyframeTrackSampler<>(this, periodTicks, lerp);
    }

    public static class Builder<T> {
        private final List<Keyframe<T>> keyframes = new ArrayList<>();
        private EasingType easing = EasingType.LINEAR;

        public Builder() {
        }

        public Builder<T> addKeyframe(int ticks, T value) {
            keyframes.add(new Keyframe<>(ticks, value));
            return this;
        }

        public Builder<T> setEasing(EasingType easing) {
            this.easing = easing;
            return this;
        }

        public KeyframeTrack<T> build() {
            List<Keyframe<T>> sorted = new ArrayList<>(keyframes);
            sorted.sort(java.util.Comparator.comparingInt(Keyframe::ticks));
            return new KeyframeTrack<>(List.copyOf(sorted), easing);
        }
    }
}
