// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import net.minecraft.util.ARGB;
import net.minecraft.world.phys.Vec3;
public record PointGizmo(Vec3 pos, int color, float size) implements Gizmo {
    public void emit(GizmoPrimitives primitives, float alphaMultiplier) {
        primitives.addPoint(pos, ARGB.multiplyAlpha(color, alphaMultiplier), size);
    }
}
