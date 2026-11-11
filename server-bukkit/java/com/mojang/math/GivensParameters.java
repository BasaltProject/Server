// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.math;
import org.joml.Matrix3f;
import org.joml.Quaternionf;
public record GivensParameters(float sinHalf, float cosHalf) {
    public static GivensParameters fromUnnormalized(float sinHalf, float cosHalf) {
        float inv = org.joml.Math.invsqrt(sinHalf * sinHalf + cosHalf * cosHalf);
        return new GivensParameters(inv * sinHalf, inv * cosHalf);
    }
    public static GivensParameters fromPositiveAngle(float angle) {
        float half = angle / 2.0F;
        float sin = org.joml.Math.sin(half);
        return new GivensParameters(sin, org.joml.Math.cosFromSin(sin, half));
    }
    public GivensParameters inverse() {
        return new GivensParameters(-sinHalf, cosHalf);
    }
    public Quaternionf aroundX(Quaternionf input) {
        return input.set(sinHalf, 0.0F, 0.0F, cosHalf);
    }
    public Quaternionf aroundY(Quaternionf input) {
        return input.set(0.0F, sinHalf, 0.0F, cosHalf);
    }
    public Quaternionf aroundZ(Quaternionf input) {
        return input.set(0.0F, 0.0F, sinHalf, cosHalf);
    }
    public float cos() {
        return cosHalf * cosHalf - sinHalf * sinHalf;
    }
    public float sin() {
        return 2.0F * sinHalf * cosHalf;
    }
    public Matrix3f aroundX(Matrix3f input) {
        float c = cos();
        float s = sin();
        return input.set(1.0F, 0.0F, 0.0F, 0.0F, c, s, 0.0F, -s, c);
    }
    public Matrix3f aroundY(Matrix3f input) {
        float c = cos();
        float s = sin();
        return input.set(c, 0.0F, s, 0.0F, 1.0F, 0.0F, -s, 0.0F, c);
    }
    public Matrix3f aroundZ(Matrix3f input) {
        float c = cos();
        float s = sin();
        return input.set(c, s, 0.0F, -s, c, 0.0F, 0.0F, 0.0F, 1.0F);
    }
}
