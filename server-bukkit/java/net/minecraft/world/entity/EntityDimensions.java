// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

public record EntityDimensions(float width, float height, float eyeHeight, EntityAttachments attachments, boolean fixed) {
    private EntityDimensions(float width, float height, boolean fixed) {
        this(width, height, defaultEyeHeight(height), EntityAttachments.createDefault(width, height), fixed);
    }

    private static float defaultEyeHeight(float height) {
        return height * 0.85F;
    }

    public AABB makeBoundingBox(Vec3 pos) {
        return makeBoundingBox(pos.x, pos.y, pos.z);
    }

    public AABB makeBoundingBox(double x, double y, double z) {
        double half = width / 2.0F;
        return new AABB(x - half, y, z - half, x + half, y + height, z + half);
    }

    public EntityDimensions scale(float scaleFactor) {
        return scale(scaleFactor, scaleFactor);
    }

    public EntityDimensions scale(float widthScaleFactor, float heightScaleFactor) {
        if (fixed || (widthScaleFactor == 1.0F && heightScaleFactor == 1.0F)) {
            return this;
        }
        return new EntityDimensions(width * widthScaleFactor, height * heightScaleFactor, eyeHeight * heightScaleFactor,
            attachments.scale(widthScaleFactor, heightScaleFactor, widthScaleFactor), false);
    }

    public static EntityDimensions scalable(float width, float height) {
        return new EntityDimensions(width, height, false);
    }

    public static EntityDimensions fixed(float width, float height) {
        return new EntityDimensions(width, height, true);
    }

    public EntityDimensions withEyeHeight(float eyeHeight) {
        return new EntityDimensions(width, height, eyeHeight, attachments, fixed);
    }

    public EntityDimensions withAttachments(EntityAttachments.Builder attachments) {
        return new EntityDimensions(width, height, eyeHeight, attachments.build(width, height), fixed);
    }
}
