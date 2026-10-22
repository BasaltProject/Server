package net.minecraft.world.phys.shapes;

public class VoxelShape {
    public static final VoxelShape EMPTY = new VoxelShape(false, true);
    public static final VoxelShape BLOCK = new VoxelShape(true, false);
    private static final VoxelShape kPartial = new VoxelShape(false, false);
    private final boolean fullBlock;
    private final boolean empty;

    private VoxelShape(boolean fullBlock, boolean empty) {
        this.fullBlock = fullBlock;
        this.empty = empty;
    }

    public static VoxelShape of(boolean fullBlock, boolean empty) {
        return fullBlock ? BLOCK : empty ? EMPTY : kPartial;
    }

    public boolean isFullBlock() {
        return fullBlock;
    }

    public boolean isEmpty() {
        return empty;
    }
}
