// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute.modifier;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.ARGB;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.Mth;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.LerpFunction;
import org.joml.Vector3fc;
import org.joml.Vector4fc;
public interface ColorModifier<Subject, Argument> extends AttributeModifier<Subject, Argument> {
    ArgbModifier<Vector3fc> ALPHA_BLEND_RGB = (base, layer) -> ARGB.alphaBlend(base, layer);
    ArgbModifier<Vector4fc> ALPHA_BLEND_ARGB = (base, layer) -> ARGB.alphaBlend(base, layer);
    RgbModifier<Vector3fc> ADD_RGB = (base, layer) -> ARGB.addRgb(base, layer);
    RgbModifier<Vector3fc> SUBTRACT_RGB = (base, layer) -> ARGB.subtractRgb(base, layer);
    RgbModifier<Vector3fc> MULTIPLY_RGB = (base, layer) -> ARGB.multiply(base, layer);
    RgbModifier<Vector4fc> ADD_ARGB = (base, layer) -> ARGB.addRgb(base, layer);
    RgbModifier<Vector4fc> SUBTRACT_ARGB = (base, layer) -> ARGB.subtractRgb(base, layer);
    ArgbModifier<Vector4fc> MULTIPLY_ARGB = (base, layer) -> ARGB.multiply(base, layer);
    ColorModifier<Vector3fc, BlendToGray> BLEND_TO_GRAY_RGB = new ColorModifier<>() {
        public Vector3fc apply(Vector3fc subject, BlendToGray argument) {
            Vector3fc grey = ARGB.scaleRGB(ARGB.greyscale(subject), argument.brightness);
            return ARGB.srgbLerp(argument.factor, subject, grey);
        }
        public Codec<BlendToGray> argumentCodec(EnvironmentAttribute<Vector3fc> type) {
            return BlendToGray.CODEC;
        }
        public LerpFunction<BlendToGray> argumentKeyframeLerp(EnvironmentAttribute<Vector3fc> type) {
            return BlendToGray::lerp;
        }
    };
    ColorModifier<Vector4fc, BlendToGray> BLEND_TO_GRAY_ARGB = new ColorModifier<>() {
        public Vector4fc apply(Vector4fc subject, BlendToGray argument) {
            Vector4fc grey = ARGB.scaleRGB(ARGB.greyscale(subject), argument.brightness);
            return ARGB.srgbLerp(argument.factor, subject, grey);
        }
        public Codec<BlendToGray> argumentCodec(EnvironmentAttribute<Vector4fc> type) {
            return BlendToGray.CODEC;
        }
        public LerpFunction<BlendToGray> argumentKeyframeLerp(EnvironmentAttribute<Vector4fc> type) {
            return BlendToGray::lerp;
        }
    };
    public interface RgbModifier<Subject> extends ColorModifier<Subject, Vector3fc> {
        default Codec<Vector3fc> argumentCodec(EnvironmentAttribute<Subject> type) {
            Codec<Vector3fc> codec = ExtraCodecs.STRING_RGB_VEC3_COLOR;
            return codec;
        }
        default LerpFunction<Vector3fc> argumentKeyframeLerp(EnvironmentAttribute<Subject> type) {
            return LerpFunction.ofColorVec3();
        }
    }
    @FunctionalInterface
    public interface ArgbModifier<Subject> extends ColorModifier<Subject, Vector4fc> {
        default Codec<Vector4fc> argumentCodec(EnvironmentAttribute<Subject> type) {
            Codec<Vector4fc> codec = ExtraCodecs.STRING_ARGB_VEC4_COLOR;
            return codec;
        }
        default LerpFunction<Vector4fc> argumentKeyframeLerp(EnvironmentAttribute<Subject> type) {
            return LerpFunction.ofColorVec4();
        }
    }
    public static record BlendToGray(float brightness, float factor) {
        public static final Codec<BlendToGray> CODEC = RecordCodecBuilder.create(i -> i.group(
                Codec.floatRange(0.0F, 1.0F).fieldOf("brightness").forGetter(BlendToGray::brightness),
                Codec.floatRange(0.0F, 1.0F).fieldOf("factor").forGetter(BlendToGray::factor)).apply(i, BlendToGray::new));
        public static BlendToGray lerp(float alpha, BlendToGray from, BlendToGray to) {
            return new BlendToGray(Mth.lerp(alpha, from.brightness, to.brightness), Mth.lerp(alpha, from.factor, to.factor));
        }
    }
}
