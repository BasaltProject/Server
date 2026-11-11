// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;
import com.mojang.math.MatrixUtil;
import com.mojang.math.Transformation;
import java.util.EnumMap;
import java.util.Map;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;
public class BlockMath {
    private static final Map<Direction, Transformation> kLocalToGlobal = new EnumMap<>(Direction.class);
    private static final Map<Direction, Transformation> kGlobalToLocal = new EnumMap<>(Direction.class);
    static {
        float quarter = (float) Math.PI / 2.0F;
        kLocalToGlobal.put(Direction.SOUTH, Transformation.IDENTITY);
        kLocalToGlobal.put(Direction.EAST, turn(new Quaternionf().rotateY(quarter)));
        kLocalToGlobal.put(Direction.WEST, turn(new Quaternionf().rotateY(-quarter)));
        kLocalToGlobal.put(Direction.NORTH, turn(new Quaternionf().rotateY((float) Math.PI)));
        kLocalToGlobal.put(Direction.UP, turn(new Quaternionf().rotateX(-quarter)));
        kLocalToGlobal.put(Direction.DOWN, turn(new Quaternionf().rotateX(quarter)));
        kLocalToGlobal.forEach((side, t) -> kGlobalToLocal.put(side, t.inverse()));
    }
    private static Transformation turn(Quaternionf q) {
        return new Transformation(null, q, null, null);
    }
    public static Transformation blockCenterToCorner(Transformation transform) {
        Matrix4f m = new Matrix4f().translation(0.5F, 0.5F, 0.5F);
        m.mul(transform.getMatrix());
        m.translate(-0.5F, -0.5F, -0.5F);
        return new Transformation(m);
    }
    public static Transformation blockCornerToCenter(Transformation transform) {
        Matrix4f m = new Matrix4f().translation(-0.5F, -0.5F, -0.5F);
        m.mul(transform.getMatrix());
        m.translate(0.5F, 0.5F, 0.5F);
        return new Transformation(m);
    }
    public static Transformation getFaceTransformation(Transformation transformation, Direction originalSide) {
        if (MatrixUtil.isIdentity(transformation.getMatrix())) {
            return transformation;
        }
        Transformation moved = transformation.compose(kLocalToGlobal.get(originalSide));
        Vector3f normal = moved.getMatrix().transformDirection(new Vector3f(0.0F, 0.0F, 1.0F));
        Direction side = Direction.getApproximateNearest(normal.x, normal.y, normal.z);
        return kGlobalToLocal.get(side).compose(moved);
    }
}
