// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

public class SimpleContainerData implements ContainerData {
    private final int[] ints;

    public SimpleContainerData(int count) {
        this.ints = new int[count];
    }

    @Override
    public int get(int dataId) {
        return ints[dataId];
    }

    @Override
    public void set(int dataId, int value) {
        ints[dataId] = value;
    }

    @Override
    public int getCount() {
        return ints.length;
    }
}
