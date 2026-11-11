// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import net.minecraft.world.phys.Vec3;
public record CircleGizmo(Vec3 pos, float radius, GizmoStyle style) implements Gizmo {
    private static final int kSegments = 20;
    private static final float kStep = (float) Math.PI / 10.0F;
    public void emit(GizmoPrimitives primitives, float alphaMultiplier) {
        if (!style.hasStroke() && !style.hasFill()) {
            return;
        }
        Vec3[] ring = new Vec3[kSegments + 1];
        for (int i = 0; i < kSegments; i++) {
            float angle = i * kStep;
            ring[i] = pos.add((float) (radius * Math.cos(angle)), 0.0, (float) (radius * Math.sin(angle)));
        }
        ring[kSegments] = ring[0];
        if (style.hasFill()) {
            primitives.addTriangleFan(ring, style.multipliedFill(alphaMultiplier));
        }
        if (style.hasStroke()) {
            int shade = style.multipliedStroke(alphaMultiplier);
            for (int i = 0; i < kSegments; i++) {
                primitives.addLine(ring[i], ring[i + 1], shade, style.strokeWidth());
            }
        }
    }
}
