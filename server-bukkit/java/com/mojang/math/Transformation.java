// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.math;
import com.mojang.serialization.Codec;
import java.util.Optional;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Quaternionf;
import org.joml.Quaternionfc;
import org.joml.Vector3f;
import org.joml.Vector3fc;
public final class Transformation {
    public static final Codec<Transformation> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(com.mojang.math.Transformation.class, "CODEC");
    public static final Codec<Transformation> EXTENDED_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(com.mojang.math.Transformation.class, "EXTENDED_CODEC");
    public static final Transformation IDENTITY = new Transformation(new Matrix4f());
    private final Vector3f translation;
    private final Quaternionf leftRotation;
    private final Vector3f scale;
    private final Quaternionf rightRotation;
    private Matrix4f matrix;
    public Transformation(Matrix4fc matrix) {
        Matrix4f copy = new Matrix4f(matrix);
        this.matrix = copy;
        this.translation = copy.getTranslation(new Vector3f());
        this.scale = copy.getScale(new Vector3f());
        this.leftRotation = copy.getNormalizedRotation(new Quaternionf());
        this.rightRotation = new Quaternionf();
    }
    public Transformation(Vector3fc translation, Quaternionfc leftRotation, Vector3fc scale, Quaternionfc rightRotation) {
        this.translation = translation == null ? new Vector3f() : new Vector3f(translation);
        this.leftRotation = leftRotation == null ? new Quaternionf() : new Quaternionf(leftRotation);
        this.scale = scale == null ? new Vector3f(1.0F) : new Vector3f(scale);
        this.rightRotation = rightRotation == null ? new Quaternionf() : new Quaternionf(rightRotation);
    }
    public Transformation compose(Transformation that) {
        return new Transformation(getMatrixCopy().mul(that.getMatrix()));
    }
    public Transformation inverse() {
        return new Transformation(getMatrixCopy().invert());
    }
    public Matrix4fc getMatrix() {
        if (matrix == null) {
            matrix = new Matrix4f().translation(translation).rotate(leftRotation).scale(scale).rotate(rightRotation);
        }
        return matrix;
    }
    public Matrix4f getMatrixCopy() {
        return new Matrix4f(getMatrix());
    }
    public Vector3fc translation() {
        return translation;
    }
    public Quaternionfc leftRotation() {
        return leftRotation;
    }
    public Vector3fc scale() {
        return scale;
    }
    public Quaternionfc rightRotation() {
        return rightRotation;
    }
    public boolean equals(Object o) {
        return o instanceof Transformation that && translation.equals(that.translation) && leftRotation.equals(that.leftRotation)
            && scale.equals(that.scale) && rightRotation.equals(that.rightRotation);
    }
    public int hashCode() {
        return java.util.Objects.hash(translation, leftRotation, scale, rightRotation);
    }
    public Transformation slerp(Transformation that, float progress) {
        return new Transformation(translation.lerp(that.translation, progress, new Vector3f()),
            leftRotation.slerp(that.leftRotation, progress, new Quaternionf()), scale.lerp(that.scale, progress, new Vector3f()),
            rightRotation.slerp(that.rightRotation, progress, new Quaternionf()));
    }
    public static Matrix4fc compose(Matrix4fc parent, Optional<Transformation> transform) {
        return transform.isPresent() ? new Matrix4f(parent).mul(transform.get().getMatrix()) : parent;
    }
}
