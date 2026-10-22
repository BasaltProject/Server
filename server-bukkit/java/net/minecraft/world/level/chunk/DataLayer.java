package net.minecraft.world.level.chunk;

public class DataLayer {
    private final byte[] data;

    public DataLayer(byte[] values) {
        this.data = values;
    }

    public int get(int x, int y, int z) {
        int index = y << 8 | z << 4 | x;
        int packed = data[index >> 1];
        return (index & 1) == 0 ? packed & 15 : packed >> 4 & 15;
    }

    public void set(int x, int y, int z, int val) {
        int index = y << 8 | z << 4 | x;
        int at = index >> 1;
        if ((index & 1) == 0) {
            data[at] = (byte) (data[at] & 240 | val & 15);
        } else {
            data[at] = (byte) (data[at] & 15 | (val & 15) << 4);
        }
    }

    public byte[] getData() {
        return data;
    }
}
