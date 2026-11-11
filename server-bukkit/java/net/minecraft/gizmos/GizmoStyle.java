// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import net.minecraft.util.ARGB;
public record GizmoStyle(int stroke, float strokeWidth, int fill) {
    private static final float kDefaultWidth = 2.5F;
    public static GizmoStyle stroke(int argb) {
        return new GizmoStyle(argb, kDefaultWidth, 0);
    }
    public static GizmoStyle stroke(int argb, float width) {
        return new GizmoStyle(argb, width, 0);
    }
    public static GizmoStyle fill(int argb) {
        return new GizmoStyle(0, 0.0F, argb);
    }
    public static GizmoStyle strokeAndFill(int stroke, float strokeWidth, int fill) {
        return new GizmoStyle(stroke, strokeWidth, fill);
    }
    public boolean hasFill() {
        return fill != 0;
    }
    public boolean hasStroke() {
        return stroke != 0 && strokeWidth > 0.0F;
    }
    public int multipliedStroke(float alphaMultiplier) {
        return ARGB.multiplyAlpha(stroke, alphaMultiplier);
    }
    public int multipliedFill(float alphaMultiplier) {
        return ARGB.multiplyAlpha(fill, alphaMultiplier);
    }
}
