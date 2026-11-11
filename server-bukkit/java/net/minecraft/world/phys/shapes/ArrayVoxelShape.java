// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import it.unimi.dsi.fastutil.doubles.DoubleList;
import net.minecraft.core.Direction;
public class ArrayVoxelShape extends VoxelShape {
    private final DoubleList xs;
    private final DoubleList ys;
    private final DoubleList zs;
    ArrayVoxelShape(DiscreteVoxelShape shape, double[] xs, double[] ys, double[] zs) {
        this(shape, it.unimi.dsi.fastutil.doubles.DoubleArrayList.wrap(java.util.Arrays.copyOf(xs, shape.getXSize() + 1)), it.unimi.dsi.fastutil.doubles.DoubleArrayList.wrap(java.util.Arrays.copyOf(ys, shape.getYSize() + 1)), it.unimi.dsi.fastutil.doubles.DoubleArrayList.wrap(java.util.Arrays.copyOf(zs, shape.getZSize() + 1)));
    }
    ArrayVoxelShape(DiscreteVoxelShape shape, DoubleList xs, DoubleList ys, DoubleList zs) {
        super(shape);
        this.xs = xs;
        this.ys = ys;
        this.zs = zs;
    }
    public DoubleList getCoords(Direction.Axis axis) {
        return switch (axis) {
            case X -> xs;
            case Y -> ys;
            case Z -> zs;
        };


    }
}
