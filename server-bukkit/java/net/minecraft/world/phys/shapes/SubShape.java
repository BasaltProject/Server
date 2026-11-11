// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import net.minecraft.core.Direction;
public final class SubShape extends DiscreteVoxelShape {
    private final DiscreteVoxelShape parent;
    private final int startX;
    private final int startY;
    private final int startZ;
    private final int endX;
    private final int endY;
    private final int endZ;
    SubShape(DiscreteVoxelShape parent, int startX, int startY, int startZ, int endX, int endY, int endZ) {
        super(endX - startX, endY - startY, endZ - startZ);
        this.parent = parent;
        this.startX = startX;
        this.startY = startY;
        this.startZ = startZ;
        this.endX = endX;
        this.endY = endY;
        this.endZ = endZ;
    }
    public boolean isFull(int x, int y, int z) {
        return parent.isFull(startX + x, startY + y, startZ + z);
    }
    public void fill(int x, int y, int z) {
        parent.fill(startX + x, startY + y, startZ + z);
    }
    public int firstFull(Direction.Axis axis) {
        return confine(axis, parent.firstFull(axis));
    }
    private int confine(Direction.Axis axis, int fromParent) {
        int low = axis.choose(startX, startY, startZ);
        int high = axis.choose(endX, endY, endZ);
        return net.minecraft.util.Mth.clamp(fromParent, low, high) - low;
    }
    public int lastFull(Direction.Axis axis) {
        return confine(axis, parent.lastFull(axis));
    }
}
