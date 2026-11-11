// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.math;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Quaternionf;
import org.joml.Vector3f;
public class MatrixUtil {
    private static final float kGamma = 3.0F + 2.0F * org.joml.Math.sqrt(2.0F);
    private static final GivensParameters kQuarterPi = GivensParameters.fromPositiveAngle((float) Math.PI / 4.0F);
    private MatrixUtil() {
    }
    public static Matrix4f mulComponentWise(Matrix4f m, float factor) {
        return m.set(m.m00() * factor, m.m01() * factor, m.m02() * factor, m.m03() * factor,
                m.m10() * factor, m.m11() * factor, m.m12() * factor, m.m13() * factor,
                m.m20() * factor, m.m21() * factor, m.m22() * factor, m.m23() * factor,
                m.m30() * factor, m.m31() * factor, m.m32() * factor, m.m33() * factor);
    }
    private static GivensParameters approx(float a11, float a12, float a22) {
        float d = 2.0F * (a11 - a22);
        return kGamma * a12 * a12 < d * d ? GivensParameters.fromUnnormalized(a12, d) : kQuarterPi;
    }
    private static GivensParameters qr(float a1, float a2) {
        float norm = (float) Math.hypot(a1, a2);
        float s = norm > 1.0E-6F ? a2 : 0.0F;
        float c = org.joml.Math.abs(a1) + org.joml.Math.max(norm, 1.0E-6F);
        return a1 < 0.0F ? GivensParameters.fromUnnormalized(c, s) : GivensParameters.fromUnnormalized(s, c);
    }
    private static void conjugate(Matrix3f a, Matrix3f q) {
        a.mul(q);
        q.transpose();
        q.mul(a);
        a.set(q);
    }
    private static void sweep(Matrix3f m, Matrix3f tmpMat, Quaternionf tmpQ, Quaternionf out) {
        if (m.m01 * m.m01 + m.m10 * m.m10 > 1.0E-6F) {
            GivensParameters g = approx(m.m00, 0.5F * (m.m01 + m.m10), m.m11);
            out.mul(g.aroundZ(tmpQ));
            g.aroundZ(tmpMat);
            conjugate(m, tmpMat);
        }
        if (m.m02 * m.m02 + m.m20 * m.m20 > 1.0E-6F) {
            GivensParameters g = approx(m.m00, 0.5F * (m.m02 + m.m20), m.m22).inverse();
            out.mul(g.aroundY(tmpQ));
            g.aroundY(tmpMat);
            conjugate(m, tmpMat);
        }
        if (m.m12 * m.m12 + m.m21 * m.m21 > 1.0E-6F) {
            GivensParameters g = approx(m.m11, 0.5F * (m.m12 + m.m21), m.m22);
            out.mul(g.aroundX(tmpQ));
            g.aroundX(tmpMat);
            conjugate(m, tmpMat);
        }
    }
    public static void eigenvalueJacobi(Matrix3f inOut, int steps, Quaternionf result) {
        result.identity();
        Matrix3f mat = new Matrix3f();
        Quaternionf quat = new Quaternionf();
        for (int i = 0; i < steps; i++) {
            sweep(inOut, mat, quat, result);
        }
        result.normalize();
    }
    public static void svdDecompose(Matrix4fc input, Vector3f t, Quaternionf u, Vector3f s, Quaternionf v) {
        float k = 1.0F / input.m33();
        svd3(new Matrix3f(input).scale(k), u, s, v);
        input.getTranslation(t).mul(k);
    }
    private static void svd3(Matrix3f input, Quaternionf u, Vector3f s, Quaternionf v) {
        Matrix3f gram = new Matrix3f(input);
        gram.transpose();
        gram.mul(input);
        eigenvalueJacobi(gram, 5, v);
        boolean dead0 = gram.m00 < 1.0E-6;
        boolean dead1 = gram.m11 < 1.0E-6;
        Matrix3f a = input.rotate(v);
        u.identity();
        Quaternionf q = new Quaternionf();
        GivensParameters g = dead0 ? qr(a.m11, -a.m10) : qr(a.m00, a.m01);
        Matrix3f b = g.aroundZ(gram);
        u.mul(g.aroundZ(q));
        b.transpose().mul(a);
        g = (dead0 ? qr(b.m22, -b.m20) : qr(b.m00, b.m02)).inverse();
        Matrix3f c = g.aroundY(a);
        u.mul(g.aroundY(q));
        c.transpose().mul(b);
        g = dead1 ? qr(c.m22, -c.m21) : qr(c.m11, c.m12);
        Matrix3f d = g.aroundX(b);
        u.mul(g.aroundX(q));
        d.transpose().mul(c);
        s.set(d.m00, d.m11, d.m22);
        v.conjugate();
    }
    public static boolean checkPropertyRaw(Matrix4fc matrix, int property) {
        return (matrix.properties() & property) != 0;
    }
    public static boolean checkProperty(Matrix4fc matrix, int property) {
        if (checkPropertyRaw(matrix, property)) {
            return true;
        }
        if (!(matrix instanceof Matrix4f mutable)) {
            return false;
        }
        int known = mutable.properties();
        mutable.determineProperties();
        mutable.assume(mutable.properties() | known);
        return checkPropertyRaw(matrix, property);
    }
    public static boolean isIdentity(Matrix4fc matrix) {
        return checkProperty(matrix, Matrix4fc.PROPERTY_IDENTITY);
    }
    public static boolean isPureTranslation(Matrix4fc matrix) {
        return checkProperty(matrix, Matrix4fc.PROPERTY_TRANSLATION);
    }
}
