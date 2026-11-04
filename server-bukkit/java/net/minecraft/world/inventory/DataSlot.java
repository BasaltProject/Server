// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

public abstract class DataSlot {
    private int prevValue;

    public static DataSlot forContainer(ContainerData container, int dataId) {
        return new DataSlot() {
            @Override
            public int get() {
                return container.get(dataId);
            }

            @Override
            public void set(int value) {
                container.set(dataId, value);
            }
        };
    }

    public static DataSlot shared(int[] storage, int index) {
        return new DataSlot() {
            @Override
            public int get() {
                return storage[index];
            }

            @Override
            public void set(int value) {
                storage[index] = value;
            }
        };
    }

    public static DataSlot standalone() {
        return new DataSlot() {
            private int value;

            @Override
            public int get() {
                return value;
            }

            @Override
            public void set(int value) {
                this.value = value;
            }
        };
    }

    public abstract int get();

    public abstract void set(int value);

    public boolean checkAndClearUpdateFlag() {
        int current = get();
        boolean changed = current != prevValue;
        prevValue = current;
        return changed;
    }
}
