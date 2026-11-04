// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
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
    public static final int LAYER_COUNT = 16;
    public static final int LAYER_SIZE = 128;
    public static final int SIZE = 2048;
    public DataLayer() {
        this(new byte[2048]);
    }
    public DataLayer(int defaultValue) {
        this(new byte[2048]);
        fill(defaultValue);
    }
    public void fill(int value) {
        java.util.Arrays.fill(data, (byte) ((value & 15) | (value & 15) << 4));
    }
    public DataLayer copy() {
        return new DataLayer(data.clone());
    }
    public String toString() {
        return "DataLayer[" + data.length + "]";
    }
    public String layerToString(int layer) {
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < 256; i++) {
            text.append(Integer.toHexString(get(i & 15, layer, i >> 4)));
            if ((i & 15) == 15) {
                text.append('\n');
            }
        }
        return text.toString();
    }
    public boolean isDefinitelyHomogenous() {
        for (byte value : data) {
            if (value != data[0]) {
                return false;
            }
        }
        return (data[0] & 15) == (data[0] >> 4 & 15);
    }
    public boolean isDefinitelyFilledWith(int value) {
        return isDefinitelyHomogenous() && (data[0] & 15) == value;
    }
    public boolean isEmpty() {
        return isDefinitelyFilledWith(0);
    }
}
