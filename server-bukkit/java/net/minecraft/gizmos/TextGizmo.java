// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import java.util.OptionalDouble;
import net.minecraft.util.ARGB;
import net.minecraft.world.phys.Vec3;
public record TextGizmo(Vec3 pos, String text, Style style) implements Gizmo {
    public void emit(GizmoPrimitives primitives, float alphaMultiplier) {
        Style shown = alphaMultiplier < 1.0F ? new Style(ARGB.multiplyAlpha(style.color, alphaMultiplier), style.scale, style.adjustLeft) : style;
        primitives.addText(pos, text, shown);
    }
    public static record Style(int color, float scale, OptionalDouble adjustLeft) {
        public static final float DEFAULT_SCALE = 0.32F;
        public static Style whiteAndCentered() {
            return forColorAndCentered(-1);
        }
        public static Style forColorAndCentered(int argb) {
            return new Style(argb, DEFAULT_SCALE, OptionalDouble.empty());
        }
        public static Style forColor(int argb) {
            return new Style(argb, DEFAULT_SCALE, OptionalDouble.of(0.0));
        }
        public Style withScale(float scale) {
            return new Style(color, scale, adjustLeft);
        }
        public Style withLeftAlignment(float adjustLeft) {
            return new Style(color, scale, OptionalDouble.of(adjustLeft));
        }
    }
}
