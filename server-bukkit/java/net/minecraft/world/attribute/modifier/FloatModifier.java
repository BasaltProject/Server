// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute.modifier;

import com.mojang.serialization.Codec;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.LerpFunction;

public interface FloatModifier<Argument> extends AttributeModifier<Float, Argument> {
    FloatModifier<FloatWithAlpha> ALPHA_BLEND = new FloatModifier<>() {
        @Override
        public Float apply(Float subject, FloatWithAlpha argument) {
            return subject + (argument.value() - subject) * argument.alpha();
        }

        @Override
        public Codec<FloatWithAlpha> argumentCodec(EnvironmentAttribute<Float> type) {
            return FloatWithAlpha.CODEC;
        }

        @Override
        public LerpFunction<FloatWithAlpha> argumentKeyframeLerp(EnvironmentAttribute<Float> type) {
            return (alpha, from, to) -> new FloatWithAlpha(from.value() + (to.value() - from.value()) * alpha,
                from.alpha() + (to.alpha() - from.alpha()) * alpha);
        }
    };
    FloatModifier<Float> ADD = (Simple) (subject, argument) -> subject + argument;
    FloatModifier<Float> SUBTRACT = (Simple) (subject, argument) -> subject - argument;
    FloatModifier<Float> MULTIPLY = (Simple) (subject, argument) -> subject * argument;
    FloatModifier<Float> MINIMUM = (Simple) Math::min;
    FloatModifier<Float> MAXIMUM = (Simple) Math::max;

    @FunctionalInterface
    interface Simple extends FloatModifier<Float> {
        @Override
        default Codec<Float> argumentCodec(EnvironmentAttribute<Float> type) {
            return Codec.FLOAT;
        }

        @Override
        default LerpFunction<Float> argumentKeyframeLerp(EnvironmentAttribute<Float> type) {
            return LerpFunction.ofFloat();
        }
    }
}
