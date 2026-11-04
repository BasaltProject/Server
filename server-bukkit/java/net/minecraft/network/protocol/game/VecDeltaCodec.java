// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import java.util.List;
import net.minecraft.world.entity.PositionPath;
import net.minecraft.world.phys.Vec3;

public class VecDeltaCodec {
    private Vec3 base = Vec3.ZERO;

    public VecDeltaCodec() {}

    static long encode(double input) {
        return Math.round(input * 4096.0);
    }

    static double decode(long v) {
        return v / 4096.0;
    }

    public static double encodingPrecisionLoss(double d) {
        return decode(encode(d)) - d;
    }

    public static boolean isDeltaTooBig(long xa, long ya, long za) {
        return xa < Short.MIN_VALUE || xa > Short.MAX_VALUE || ya < Short.MIN_VALUE || ya > Short.MAX_VALUE
            || za < Short.MIN_VALUE || za > Short.MAX_VALUE;
    }

    public Vec3 decode(long xa, long ya, long za) {
        if (xa == 0L && ya == 0L && za == 0L) {
            return base;
        }
        double x = xa == 0L ? base.x : decode(encode(base.x) + xa);
        double y = ya == 0L ? base.y : decode(encode(base.y) + ya);
        double z = za == 0L ? base.z : decode(encode(base.z) + za);
        return new Vec3(x, y, z);
    }

    public VecDelta tryEncode(PositionPath position) {
        if (position instanceof PositionPath.Stepped stepped) {
            return VecDelta.Stepped.tryEncode(this, stepped.steps());
        }
        return tryEncode(position.endPosition());
    }

    public VecDelta tryEncode(Vec3 pos) {
        long xa = encodeX(pos);
        long ya = encodeY(pos);
        long za = encodeZ(pos);
        return isDeltaTooBig(xa, ya, za) ? null : new VecDelta.Linear((short) xa, (short) ya, (short) za);
    }

    public long encodeX(Vec3 pos) {
        return encode(pos.x) - encode(base.x);
    }

    public long encodeY(Vec3 pos) {
        return encode(pos.y) - encode(base.y);
    }

    public long encodeZ(Vec3 pos) {
        return encode(pos.z) - encode(base.z);
    }

    public Vec3 delta(Vec3 pos) {
        return pos.subtract(base);
    }

    public void setBase(Vec3 base) {
        this.base = base;
    }

    public Vec3 getBase() {
        return base;
    }
}
