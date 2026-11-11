// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import net.minecraft.util.ARGB;
import net.minecraft.world.phys.Vec3;
public record LineGizmo(Vec3 start, Vec3 end, int color, float width) implements Gizmo {
    public static final float DEFAULT_WIDTH = 3.0F;
    public void emit(GizmoPrimitives primitives, float alphaMultiplier) {
        primitives.addLine(start, end, ARGB.multiplyAlpha(color, alphaMultiplier), width);
    }
}
