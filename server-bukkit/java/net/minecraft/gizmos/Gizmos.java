// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public class Gizmos {
    private static final ThreadLocal<GizmoCollector> current = new ThreadLocal<>();
    private Gizmos() {
    }
    public static TemporaryCollection withCollector(GizmoCollector collector) {
        TemporaryCollection scope = new TemporaryCollection();
        current.set(collector);
        return scope;
    }
    public static GizmoProperties addGizmo(Gizmo gizmo) {
        GizmoCollector target = current.get();
        if (target == null) {
            throw new IllegalStateException("No GizmoCollector has been registered for this thread");
        }
        return target.add(gizmo);
    }
    public static GizmoProperties cuboid(AABB aabb, GizmoStyle style) {
        return cuboid(aabb, style, false);
    }
    public static GizmoProperties cuboid(AABB aabb, GizmoStyle style, boolean coloredCorner) {
        Gizmo shape = new CuboidGizmo(aabb, style, coloredCorner);
        return addGizmo(shape);
    }
    public static GizmoProperties cuboid(BlockPos blockPos, GizmoStyle style) {
        AABB box = new AABB(blockPos);
        return cuboid(box, style);
    }
    public static GizmoProperties cuboid(BlockPos blockPos, float padding, GizmoStyle style) {
        return cuboid(new AABB(blockPos).inflate(padding), style);
    }
    public static GizmoProperties circle(Vec3 pos, float radius, GizmoStyle style) {
        Gizmo shape = new CircleGizmo(pos, radius, style);
        return addGizmo(shape);
    }
    public static GizmoProperties line(Vec3 start, Vec3 end, int argb) {
        return line(start, end, argb, 3.0F);
    }
    public static GizmoProperties line(Vec3 start, Vec3 end, int argb, float width) {
        Gizmo shape = new LineGizmo(start, end, argb, width);
        return addGizmo(shape);
    }
    public static GizmoProperties arrow(Vec3 start, Vec3 end, int argb) {
        return arrow(start, end, argb, 2.5F);
    }
    public static GizmoProperties arrow(Vec3 start, Vec3 end, int argb, float width) {
        Gizmo shape = new ArrowGizmo(start, end, argb, width);
        return addGizmo(shape);
    }
    public static GizmoProperties rect(Vec3 cuboidCornerA, Vec3 cuboidCornerB, Direction face, GizmoStyle style) {
        Gizmo shape = RectGizmo.fromCuboidFace(cuboidCornerA, cuboidCornerB, face, style);
        return addGizmo(shape);
    }
    public static GizmoProperties rect(Vec3 cornerA, Vec3 cornerB, Vec3 cornerC, Vec3 cornerD, GizmoStyle style) {
        Gizmo shape = new RectGizmo(cornerA, cornerB, cornerC, cornerD, style);
        return addGizmo(shape);
    }
    public static GizmoProperties point(Vec3 position, int argb, float size) {
        Gizmo shape = new PointGizmo(position, argb, size);
        return addGizmo(shape);
    }
    public static GizmoProperties billboardTextOverBlock(String text, BlockPos pos, int row, int color, float scale) {
        Vec3 at = Vec3.atLowerCornerWithOffset(pos, 0.5, 1.3 + row * 0.2, 0.5);
        GizmoProperties props = billboardText(text, at, TextGizmo.Style.forColorAndCentered(color).withScale(scale));
        return props.setAlwaysOnTop();
    }
    public static GizmoProperties billboardTextOverMob(Entity entity, int row, String text, int color, float scale) {
        Vec3 at = new Vec3(entity.getBlockX() + 0.5, entity.getY() + 2.4 + row * 0.25, entity.getBlockZ() + 0.5);
        GizmoProperties props = billboardText(text, at, TextGizmo.Style.forColor(color).withScale(scale).withLeftAlignment(0.5F));
        return props.setAlwaysOnTop();
    }
    public static GizmoProperties billboardText(String name, Vec3 pos, TextGizmo.Style style) {
        Gizmo shape = new TextGizmo(pos, name, style);
        return addGizmo(shape);
    }
    public static class TemporaryCollection implements AutoCloseable {
        private final GizmoCollector previous = current.get();
        private boolean closed;
        private TemporaryCollection() {
        }
        public void close() {
            if (!closed) {
                closed = true;
                current.set(previous);
            }
        }
    }
}
