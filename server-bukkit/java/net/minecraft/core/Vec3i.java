package net.minecraft.core;

public class Vec3i implements Comparable<Vec3i> {
    public static final Vec3i ZERO = new Vec3i(0, 0, 0);
    private int x;
    private int y;
    private int z;

    public Vec3i(int x, int y, int z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public int getX() {
        return x;
    }

    public int getY() {
        return y;
    }

    public int getZ() {
        return z;
    }

    protected Vec3i setX(int x) {
        this.x = x;
        return this;
    }

    protected Vec3i setY(int y) {
        this.y = y;
        return this;
    }

    protected Vec3i setZ(int z) {
        this.z = z;
        return this;
    }

    @Override
    public boolean equals(Object other) {
        return this == other || other instanceof Vec3i v && x == v.x && y == v.y && z == v.z;
    }

    @Override
    public int hashCode() {
        return (y + z * 31) * 31 + x;
    }

    @Override
    public int compareTo(Vec3i other) {
        return y != other.y ? y - other.y : z != other.z ? z - other.z : x - other.x;
    }

    @Override
    public String toString() {
        return "Vec3i{x=" + x + ", y=" + y + ", z=" + z + "}";
    }
}
