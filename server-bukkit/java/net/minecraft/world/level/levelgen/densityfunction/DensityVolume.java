// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

import net.minecraft.util.Mth;
import net.minecraft.world.level.levelgen.structure.BoundingBox;

public record DensityVolume(int sizeX, int sizeY, int sizeZ, int minBlockX, int minBlockY, int minBlockZ, int stepBlockX, int stepBlockY, int stepBlockZ) {
    public static final int NO_BLOCK = -1;

    public DensityVolume {
        if (sizeX <= 0 || sizeY <= 0 || sizeZ <= 0) {
            throw new IllegalArgumentException("A volume needs a positive size, not " + sizeX + "x" + sizeY + "x" + sizeZ);
        }
        if (stepBlockX <= 0 || stepBlockY <= 0 || stepBlockZ <= 0) {
            throw new IllegalArgumentException("A volume needs a positive step, not " + stepBlockX + "x" + stepBlockY + "x" + stepBlockZ);
        }
    }

    public DensityVolume(int sizeX, int sizeY, int sizeZ, int minBlockX, int minBlockY, int minBlockZ) {
        this(sizeX, sizeY, sizeZ, minBlockX, minBlockY, minBlockZ, 1, 1, 1);
    }

    public int indexUnchecked(int indexX, int indexY, int indexZ) {
        return indexY + (indexX + indexZ * sizeX) * sizeY;
    }

    public int blockX(int x) {
        return minBlockX + x * stepBlockX;
    }

    public int blockY(int y) {
        return minBlockY + y * stepBlockY;
    }

    public int blockZ(int z) {
        return minBlockZ + z * stepBlockZ;
    }

    public int maxBlockX() {
        return minBlockX + sizeX * stepBlockX - 1;
    }

    public int maxBlockY() {
        return minBlockY + sizeY * stepBlockY - 1;
    }

    public int maxBlockZ() {
        return minBlockZ + sizeZ * stepBlockZ - 1;
    }

    public int size() {
        return sizeX * sizeY * sizeZ;
    }

    public boolean intersects(BoundingBox box) {
        return box.intersects(minBlockX, minBlockY, minBlockZ, maxBlockX(), maxBlockY(), maxBlockZ());
    }

    private boolean onGrid(int dx, int dy, int dz) {
        return dx >= 0 && dy >= 0 && dz >= 0 && dx < sizeX * stepBlockX && dy < sizeY * stepBlockY && dz < sizeZ * stepBlockZ
            && Math.floorMod(dx, stepBlockX) == 0 && Math.floorMod(dy, stepBlockY) == 0 && Math.floorMod(dz, stepBlockZ) == 0;
    }

    public int indexOfBlock(int blockX, int blockY, int blockZ) {
        int dx = blockX - minBlockX;
        int dy = blockY - minBlockY;
        int dz = blockZ - minBlockZ;
        if (stepBlockX == 1 && stepBlockY == 1 && stepBlockZ == 1) {
            return dx >= 0 && dy >= 0 && dz >= 0 && dx < sizeX && dy < sizeY && dz < sizeZ ? indexUnchecked(dx, dy, dz) : NO_BLOCK;
        }
        return onGrid(dx, dy, dz)
            ? indexUnchecked(Mth.floorDiv(dx, stepBlockX), Mth.floorDiv(dy, stepBlockY), Mth.floorDiv(dz, stepBlockZ))
            : NO_BLOCK;
    }
}
