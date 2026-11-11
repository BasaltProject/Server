// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.math;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.joml.Vector3fc;
public interface Axis {
    float DEG_TO_RAD = (float) Math.PI / 180.0F;
    Axis XN = signed(1.0F, 0.0F, 0.0F, "-X");
    Axis XP = signed(1.0F, 0.0F, 0.0F, "+X");
    Axis YN = signed(0.0F, 1.0F, 0.0F, "-Y");
    Axis YP = signed(0.0F, 1.0F, 0.0F, "+Y");
    Axis ZN = signed(0.0F, 0.0F, 1.0F, "-Z");
    Axis ZP = signed(0.0F, 0.0F, 1.0F, "+Z");
    private static Axis signed(float x, float y, float z, String label) {
        float sign = label.charAt(0) == '-' ? -1.0F : 1.0F;
        Vector3f dir = new Vector3f(x, y, z);
        return new Axis() {
            public Quaternionf rotation(float angle) {
                return new Quaternionf().rotationAxis(sign * angle, dir);
            }
            public Matrix3f rotate(Matrix3f matrix, float angle) {
                return matrix.rotate(sign * angle, dir);
            }
            public Matrix4f rotate(Matrix4f matrix, float angle) {
                return matrix.rotate(sign * angle, dir);
            }
            public String toString() {
                return "<rotation around " + label + ">";
            }
        };
    }
    static Axis of(Vector3fc axis) {
        return new Axis() {
            public Quaternionf rotation(float angle) {
                return new Quaternionf().rotationAxis(angle, axis);
            }
            public Matrix3f rotate(Matrix3f matrix, float angle) {
                return matrix.rotate(angle, axis);
            }
            public Matrix4f rotate(Matrix4f matrix, float angle) {
                return matrix.rotate(angle, axis);
            }
            public String toString() {
                return "<rotation around " + axis + ">";
            }
        };
    }
    Quaternionf rotation(float angle);
    default Quaternionf rotationDegrees(float angle) {
        return rotation(angle * DEG_TO_RAD);
    }
    Matrix3f rotate(Matrix3f matrix, float angle);
    default Matrix3f rotateDegrees(Matrix3f matrix, float angle) {
        return rotate(matrix, angle * DEG_TO_RAD);
    }
    Matrix4f rotate(Matrix4f matrix, float angle);
    default Matrix4f rotateDegrees(Matrix4f matrix, float angle) {
        return rotate(matrix, angle * DEG_TO_RAD);
    }
}
