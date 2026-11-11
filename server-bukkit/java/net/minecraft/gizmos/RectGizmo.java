// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.Vec3;
public record RectGizmo(Vec3 a, Vec3 b, Vec3 c, Vec3 d, GizmoStyle style) implements Gizmo {
    public static RectGizmo fromCuboidFace(Vec3 p, Vec3 q, Direction face, GizmoStyle style) {
        return switch (face) {
            case DOWN -> new RectGizmo(new Vec3(p.x, p.y, p.z), new Vec3(q.x, p.y, p.z), new Vec3(q.x, p.y, q.z), new Vec3(p.x, p.y, q.z), style);
            case UP -> new RectGizmo(new Vec3(p.x, q.y, p.z), new Vec3(p.x, q.y, q.z), new Vec3(q.x, q.y, q.z), new Vec3(q.x, q.y, p.z), style);
            case NORTH -> new RectGizmo(new Vec3(p.x, p.y, p.z), new Vec3(p.x, q.y, p.z), new Vec3(q.x, q.y, p.z), new Vec3(q.x, p.y, p.z), style);
            case SOUTH -> new RectGizmo(new Vec3(p.x, p.y, q.z), new Vec3(q.x, p.y, q.z), new Vec3(q.x, q.y, q.z), new Vec3(p.x, q.y, q.z), style);
            case WEST -> new RectGizmo(new Vec3(p.x, p.y, p.z), new Vec3(p.x, p.y, q.z), new Vec3(p.x, q.y, q.z), new Vec3(p.x, q.y, p.z), style);
            case EAST -> new RectGizmo(new Vec3(q.x, p.y, p.z), new Vec3(q.x, q.y, p.z), new Vec3(q.x, q.y, q.z), new Vec3(q.x, p.y, q.z), style);
        };
    }
    public void emit(GizmoPrimitives primitives, float alphaMultiplier) {
        if (style.hasFill()) {
            primitives.addQuad(a, b, c, d, style.multipliedFill(alphaMultiplier));
        }
        if (style.hasStroke()) {
            int shade = style.multipliedStroke(alphaMultiplier);
            Vec3[] ring = {a, b, c, d, a};
            for (int i = 0; i < 4; i++) {
                primitives.addLine(ring[i], ring[i + 1], shade, style.strokeWidth());
            }
        }
    }
}
