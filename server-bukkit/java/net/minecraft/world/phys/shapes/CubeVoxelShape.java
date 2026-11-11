// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import it.unimi.dsi.fastutil.doubles.DoubleList;
import net.minecraft.core.Direction;
public final class CubeVoxelShape extends VoxelShape {
    public CubeVoxelShape(DiscreteVoxelShape shape) {
        super(shape);
    }
    public DoubleList getCoords(Direction.Axis axis) {
        return new CubePointRange(this.shape.getSize(axis));
    }
    protected int findIndex(Direction.Axis axis, double coord) {
        int cells = this.shape.getSize(axis);
        return net.minecraft.util.Mth.floor(net.minecraft.util.Mth.clamp(coord * cells, -1.0, cells));
    }
}
