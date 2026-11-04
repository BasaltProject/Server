// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import net.minecraft.core.Direction;

public class SegmentedAnglePrecision {
    private final int mask;
    private final int bits;
    private final float degreeToAngle;
    private final float angleToDegree;

    public SegmentedAnglePrecision(int bitPrecision) {
        if (bitPrecision > 30) {
            throw new IllegalArgumentException("Bit precision must be less than 31");
        }
        this.bits = bitPrecision;
        this.mask = (1 << bitPrecision) - 1;
        this.degreeToAngle = (1 << bitPrecision) / 360.0F;
        this.angleToDegree = 360.0F / (1 << bitPrecision);
    }

    public boolean isSameAxis(int binaryAngleA, int binaryAngleB) {
        int half = this.getMask() >> 1;
        return (binaryAngleA & half) == (binaryAngleB & half);
    }

    public int fromDirection(Direction direction) {
        if (direction.getAxis().isVertical()) {
            return 0;
        }
        int horizontal = direction.get2DDataValue();
        return horizontal << this.bits - 2;
    }

    public int fromDegreesWithTurns(float degrees) {
        return Math.round(degrees * this.degreeToAngle);
    }

    public int fromDegrees(float degrees) {
        return this.normalize(this.fromDegreesWithTurns(degrees));
    }

    public float toDegreesWithTurns(int binaryAngle) {
        return binaryAngle * this.angleToDegree;
    }

    public float toDegrees(int binaryAngle) {
        return Mth.wrapDegrees(this.toDegreesWithTurns(this.normalize(binaryAngle)));
    }

    public int normalize(int binaryAngle) {
        return binaryAngle & this.mask;
    }

    public int getMask() {
        return this.mask;
    }
}
