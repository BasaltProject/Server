// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import net.minecraft.util.ARGB;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public record CuboidGizmo(AABB aabb, GizmoStyle style, boolean coloredCornerStroke) implements Gizmo {
    private static Vec3 v(double x, double y, double z) {
        return new Vec3(x, y, z);
    }
    public void emit(GizmoPrimitives p, float alphaMultiplier) {
        double x0 = aabb.minX, y0 = aabb.minY, z0 = aabb.minZ;
        double x1 = aabb.maxX, y1 = aabb.maxY, z1 = aabb.maxZ;
        if (style.hasFill()) {
            int c = style.multipliedFill(alphaMultiplier);
            p.addQuad(v(x1, y0, z0), v(x1, y1, z0), v(x1, y1, z1), v(x1, y0, z1), c);
            p.addQuad(v(x0, y0, z0), v(x0, y0, z1), v(x0, y1, z1), v(x0, y1, z0), c);
            p.addQuad(v(x0, y0, z0), v(x0, y1, z0), v(x1, y1, z0), v(x1, y0, z0), c);
            p.addQuad(v(x0, y0, z1), v(x1, y0, z1), v(x1, y1, z1), v(x0, y1, z1), c);
            p.addQuad(v(x0, y1, z0), v(x0, y1, z1), v(x1, y1, z1), v(x1, y1, z0), c);
            p.addQuad(v(x0, y0, z0), v(x1, y0, z0), v(x1, y0, z1), v(x0, y0, z1), c);
        }
        if (!style.hasStroke()) {
            return;
        }
        int c = style.multipliedStroke(alphaMultiplier);
        float w = style.strokeWidth();
        p.addLine(v(x0, y0, z0), v(x1, y0, z0), coloredCornerStroke ? ARGB.multiply(c, -34953) : c, w);
        p.addLine(v(x0, y0, z0), v(x0, y1, z0), coloredCornerStroke ? ARGB.multiply(c, -8913033) : c, w);
        p.addLine(v(x0, y0, z0), v(x0, y0, z1), coloredCornerStroke ? ARGB.multiply(c, -8947713) : c, w);
        double[][][] edges = {
            {{x1, y0, z0}, {x1, y1, z0}}, {{x1, y1, z0}, {x0, y1, z0}}, {{x0, y1, z0}, {x0, y1, z1}},
            {{x0, y1, z1}, {x0, y0, z1}}, {{x0, y0, z1}, {x1, y0, z1}}, {{x1, y0, z1}, {x1, y0, z0}},
            {{x0, y1, z1}, {x1, y1, z1}}, {{x1, y0, z1}, {x1, y1, z1}}, {{x1, y1, z0}, {x1, y1, z1}}};
        for (double[][] e : edges) {
            p.addLine(v(e[0][0], e[0][1], e[0][2]), v(e[1][0], e[1][1], e[1][2]), c, w);
        }
    }
}
