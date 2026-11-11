// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import it.unimi.dsi.fastutil.doubles.DoubleList;
import net.minecraft.core.Direction;
public class SliceShape extends VoxelShape {
    private final VoxelShape delegate;
    private final Direction.Axis axis;
    private static final DoubleList SLICE_COORDS = new CubePointRange(1);
    public SliceShape(VoxelShape delegate, Direction.Axis axis, int point) {
        super((DiscreteVoxelShape) null);
        this.delegate = delegate;
        this.axis = axis;
    }
    public DoubleList getCoords(Direction.Axis axis) {
        return axis == this.axis ? SLICE_COORDS : delegate.getCoords(axis);


    }
}
