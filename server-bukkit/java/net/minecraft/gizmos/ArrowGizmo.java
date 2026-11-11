// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import net.minecraft.util.ARGB;
import net.minecraft.util.Mth;
import net.minecraft.world.phys.Vec3;
import org.joml.Quaternionf;
import org.joml.Vector3f;
public record ArrowGizmo(Vec3 start, Vec3 end, int color, float width) implements Gizmo {
    public static final float DEFAULT_WIDTH = 2.5F;
    public void emit(GizmoPrimitives primitives, float alphaMultiplier) {
        int shade = ARGB.multiplyAlpha(color, alphaMultiplier);
        primitives.addLine(start, end, shade, width);
        Vector3f dir = end.subtract(start).toVector3f().normalize();
        Quaternionf turn = new Quaternionf().rotationTo(new Vector3f(1.0F, 0.0F, 0.0F), dir);
        float head = (float) Mth.clamp(end.distanceTo(start) * 0.1, 0.1, 1.0);
        float[][] offsets = {{-head, head, 0.0F}, {-head, 0.0F, head}, {-head, -head, 0.0F}, {-head, 0.0F, -head}};
        for (float[] o : offsets) {
            Vector3f t = turn.transform(o[0], o[1], o[2], new Vector3f());
            primitives.addLine(end.add(t.x, t.y, t.z), end, shade, width);
        }
    }
}
