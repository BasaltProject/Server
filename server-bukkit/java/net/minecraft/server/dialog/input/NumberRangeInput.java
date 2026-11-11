// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog.input;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.server.dialog.Dialog;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.Mth;
public record NumberRangeInput(int width, Component label, String labelFormat, RangeInfo rangeInfo) implements InputControl {
    public static final MapCodec<NumberRangeInput> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            Dialog.WIDTH_CODEC.optionalFieldOf("width", 200).forGetter(NumberRangeInput::width),
            ComponentSerialization.CODEC.fieldOf("label").forGetter(NumberRangeInput::label),
            Codec.STRING.optionalFieldOf("label_format", "options.generic_value").forGetter(NumberRangeInput::labelFormat),
            RangeInfo.MAP_CODEC.forGetter(NumberRangeInput::rangeInfo))
            .apply(i, NumberRangeInput::new));
    public MapCodec<NumberRangeInput> mapCodec() {
        return MAP_CODEC;
    }
    public Component computeLabel(String value) {
        return Component.translatable(labelFormat, label, value);
    }
    public static record RangeInfo(float start, float end, Optional<Float> initial, Optional<Float> step) {
        public static final MapCodec<RangeInfo> MAP_CODEC = RecordCodecBuilder.<RangeInfo>mapCodec(i -> i.group(
                Codec.FLOAT.fieldOf("start").forGetter(RangeInfo::start),
                Codec.FLOAT.fieldOf("end").forGetter(RangeInfo::end),
                Codec.FLOAT.optionalFieldOf("initial").forGetter(RangeInfo::initial),
                ExtraCodecs.POSITIVE_FLOAT.optionalFieldOf("step").forGetter(RangeInfo::step))
                .apply(i, RangeInfo::new))
                .validate(RangeInfo::checkInitial);
        private static DataResult<RangeInfo> checkInitial(RangeInfo range) {
            if (range.initial.isPresent()) {
                double value = range.initial.get();
                double low = Math.min(range.start, range.end);
                double high = Math.max(range.start, range.end);
                if (value < low || value > high) {
                    return DataResult.error(() -> "Initial value " + value + " is outside of range [" + low + ", " + high + "]");
                }
            }
            return DataResult.success(range);
        }
        public float computeScaledValue(float sliderValue) {
            float inRange = Mth.lerp(sliderValue, start, end);
            if (step.isEmpty()) {
                return inRange;
            }
            float stepSize = step.get();
            float origin = initialScaledValue();
            int steps = Math.round((inRange - origin) / stepSize);
            float snapped = origin + steps * stepSize;
            if (!outsideSlider(snapped)) {
                return snapped;
            }
            return origin + (steps - Mth.sign((double) steps)) * stepSize;
        }
        private boolean outsideSlider(float scaled) {
            float position = toSlider(scaled);
            return position < 0.0 || position > 1.0;
        }
        private float initialScaledValue() {
            return initial.isPresent() ? initial.get() : (start + end) / 2.0F;
        }
        public float initialSliderValue() {
            return toSlider(initialScaledValue());
        }
        private float toSlider(float value) {
            return start == end ? 0.5F : Mth.inverseLerp(value, start, end);
        }
    }
}
