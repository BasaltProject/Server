// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.math;
import net.minecraft.core.Direction;
import org.joml.Matrix3f;
import org.joml.Matrix3fc;
import org.joml.Vector3f;
import org.joml.Vector3i;
public enum SymmetricGroup3 {
    P123(0, 1, 2), P213(1, 0, 2), P132(0, 2, 1), P312(2, 0, 1), P231(1, 2, 0), P321(2, 1, 0);
    private final int[] map;
    private final Matrix3fc matrix;
    private static SymmetricGroup3[][] compositions;
    SymmetricGroup3(int a, int b, int c) {
        map = new int[] {a, b, c};
        matrix = new Matrix3f().zero().set(a, 0, 1.0F).set(b, 1, 1.0F).set(c, 2, 1.0F);
    }
    private static SymmetricGroup3 find(int a, int b, int c) {
        for (SymmetricGroup3 g : values()) {
            if (g.map[0] == a && g.map[1] == b && g.map[2] == c) {
                return g;
            }
        }
        throw new IllegalStateException();
    }
    public SymmetricGroup3 compose(SymmetricGroup3 that) {
        if (compositions == null) {
            SymmetricGroup3[] all = values();
            SymmetricGroup3[][] table = new SymmetricGroup3[all.length][all.length];
            for (SymmetricGroup3 x : all) {
                for (SymmetricGroup3 y : all) {
                    table[x.ordinal()][y.ordinal()] = find(x.permute(y.map[0]), x.permute(y.map[1]), x.permute(y.map[2]));
                }
            }
            compositions = table;
        }
        return compositions[ordinal()][that.ordinal()];
    }
    public SymmetricGroup3 inverse() {
        for (SymmetricGroup3 g : values()) {
            if (compose(g) == P123) {
                return g;
            }
        }
        throw new IllegalStateException();
    }
    public int permute(int i) {
        if (i < 0 || i > 2) {
            throw new IllegalArgumentException("Must be 0, 1 or 2, but got " + i);
        }
        return map[i];
    }
    public Direction.Axis permuteAxis(Direction.Axis axis) {
        return Direction.Axis.VALUES[permute(axis.ordinal())];
    }
    public Vector3f permuteVector(Vector3f v) {
        return v.set(v.get(map[0]), v.get(map[1]), v.get(map[2]));
    }
    public Vector3i permuteVector(Vector3i v) {
        return v.set(v.get(map[0]), v.get(map[1]), v.get(map[2]));
    }
    public Matrix3fc transformation() {
        return matrix;
    }
}
