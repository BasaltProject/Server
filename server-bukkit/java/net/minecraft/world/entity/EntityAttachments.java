// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import java.util.ArrayList;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;
import net.minecraft.util.Mth;
import net.minecraft.world.phys.Vec3;

public class EntityAttachments {
    private final Map<EntityAttachment, List<Vec3>> attachments;

    private EntityAttachments(Map<EntityAttachment, List<Vec3>> attachments) {
        this.attachments = attachments;
    }

    public static EntityAttachments createDefault(float width, float height) {
        return builder().build(width, height);
    }

    public static Builder builder() {
        return new Builder();
    }

    public EntityAttachments scale(float x, float y, float z) {
        Map<EntityAttachment, List<Vec3>> scaled = new EnumMap<>(EntityAttachment.class);
        attachments.forEach((attachment, points) -> {
            List<Vec3> out = new ArrayList<>(points.size());
            for (Vec3 point : points) {
                out.add(point.multiply(x, y, z));
            }
            scaled.put(attachment, out);
        });
        return new EntityAttachments(scaled);
    }

    public Vec3 getNullable(EntityAttachment attachment, int index, float rotY) {
        List<Vec3> points = attachments.get(attachment);
        if (index < 0 || index >= points.size()) {
            return null;
        }
        return points.get(index).yRot(-rotY * ((float) Math.PI / 180.0F));
    }

    public Vec3 get(EntityAttachment attachment, int index, float rotY) {
        Vec3 point = getNullable(attachment, index, rotY);
        if (point == null) {
            throw new IllegalStateException("Had no attachment point of type: " + attachment + " for index: " + index);
        }
        return point;
    }

    public Vec3 getAverage(EntityAttachment attachment) {
        List<Vec3> points = attachments.get(attachment);
        if (points == null || points.isEmpty()) {
            return Vec3.ZERO;
        }
        Vec3 sum = Vec3.ZERO;
        for (Vec3 point : points) {
            sum = sum.add(point);
        }
        return sum.scale(1.0F / points.size());
    }

    public Vec3 getClamped(EntityAttachment attachment, int index, float rotY) {
        List<Vec3> points = attachments.get(attachment);
        if (points.isEmpty()) {
            throw new IllegalStateException("Had no attachment points of type: " + attachment);
        }
        return points.get(Mth.clamp(index, 0, points.size() - 1)).yRot(-rotY * ((float) Math.PI / 180.0F));
    }

    public static class Builder {
        private final Map<EntityAttachment, List<Vec3>> attachments = new EnumMap<>(EntityAttachment.class);

        private Builder() {
        }

        public Builder attach(EntityAttachment attachment, float x, float y, float z) {
            return attach(attachment, new Vec3(x, y, z));
        }

        public Builder attach(EntityAttachment attachment, Vec3 point) {
            attachments.computeIfAbsent(attachment, key -> new ArrayList<>(1)).add(point);
            return this;
        }

        public EntityAttachments build(float width, float height) {
            Map<EntityAttachment, List<Vec3>> built = new EnumMap<>(EntityAttachment.class);
            for (EntityAttachment attachment : EntityAttachment.values()) {
                List<Vec3> points = attachments.get(attachment);
                built.put(attachment, points != null ? List.copyOf(points) : attachment.createFallbackPoints(width, height));
            }
            return new EntityAttachments(built);
        }
    }
}
