// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.math;
import com.mojang.serialization.Codec;
public enum Quadrant {
    R0(0, OctahedralGroup.IDENTITY, OctahedralGroup.IDENTITY, OctahedralGroup.IDENTITY),
    R90(1, OctahedralGroup.BLOCK_ROT_X_90, OctahedralGroup.BLOCK_ROT_Y_90, OctahedralGroup.BLOCK_ROT_Z_90),
    R180(2, OctahedralGroup.BLOCK_ROT_X_180, OctahedralGroup.BLOCK_ROT_Y_180, OctahedralGroup.BLOCK_ROT_Z_180),
    R270(3, OctahedralGroup.BLOCK_ROT_X_270, OctahedralGroup.BLOCK_ROT_Y_270, OctahedralGroup.BLOCK_ROT_Z_270);
    public static final Codec<Quadrant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(com.mojang.math.Quadrant.class, "CODEC");
    public final int shift;
    public final OctahedralGroup rotationX;
    public final OctahedralGroup rotationY;
    public final OctahedralGroup rotationZ;
    Quadrant(int shift, OctahedralGroup x, OctahedralGroup y, OctahedralGroup z) {
        this.shift = shift;
        this.rotationX = x;
        this.rotationY = y;
        this.rotationZ = z;
    }
    public static Quadrant parseJson(int degrees) {
        int turn = Math.floorMod(degrees, 360);
        if (turn % 90 != 0) {
            throw new com.google.gson.JsonParseException("Invalid rotation " + degrees + " found, only 0/90/180/270 allowed");
        }
        return values()[turn / 90];
    }
    public static OctahedralGroup fromXYAngles(Quadrant xRotation, Quadrant yRotation) {
        return yRotation.rotationY.compose(xRotation.rotationX);
    }
    public static OctahedralGroup fromXYZAngles(Quadrant xRotation, Quadrant yRotation, Quadrant zRotation) {
        return zRotation.rotationZ.compose(yRotation.rotationY.compose(xRotation.rotationX));
    }
    public int rotateVertexIndex(int index) {
        return (index + shift) % 4;
    }
}
