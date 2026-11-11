// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import it.unimi.dsi.fastutil.doubles.AbstractDoubleList;
public class CubePointRange extends AbstractDoubleList {
    private final int parts;
    public CubePointRange(int parts) {
        if (parts <= 0) {
            throw new IllegalArgumentException("Need at least 1 part");
        }
        this.parts = parts;
    }
    public double getDouble(int index) {
        return (double) index / parts;
    }
    public int size() {
        return parts + 1;
    }
}
