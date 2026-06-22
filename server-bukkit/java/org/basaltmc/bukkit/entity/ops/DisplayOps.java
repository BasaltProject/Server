package org.basaltmc.bukkit.entity.ops;

import java.util.List;
import java.util.Locale;
import java.util.Map;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Color;
import org.bukkit.entity.Display;
import org.bukkit.util.Transformation;
import org.joml.AxisAngle4f;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;

public interface DisplayOps extends Display, Cored {
    private static Quaternionf rotation(Object stored) {
        List<Object> list = Nbt.list(stored);
        if (list != null && list.size() >= 4) {
            return new Quaternionf(EntityNbt.at(list, 0, 0), EntityNbt.at(list, 1, 0), EntityNbt.at(list, 2, 0),
                EntityNbt.at(list, 3, 1));
        }
        Map<String, Object> axisAngle = Nbt.map(stored);
        if (axisAngle != null) {
            List<Object> axis = Nbt.list(axisAngle.get("axis"));
            return new Quaternionf(new AxisAngle4f(EntityNbt.real(axisAngle.get("angle"), 0), EntityNbt.at(axis, 0, 0),
                EntityNbt.at(axis, 1, 1), EntityNbt.at(axis, 2, 0)));
        }
        return new Quaternionf();
    }

    private static List<Object> quaternion(Quaternionf rotation) {
        return EntityNbt.floats(rotation.x, rotation.y, rotation.z, rotation.w);
    }

    default Transformation getTransformation() {
        Map<String, Object> stored = Nbt.map(EntityNbt.compound(core(), "").get("transformation"));
        if (stored == null) {
            return new Transformation(new Vector3f(), new Quaternionf(), new Vector3f(1, 1, 1), new Quaternionf());
        }
        List<Object> translation = Nbt.list(stored.get("translation"));
        List<Object> scale = Nbt.list(stored.get("scale"));
        return new Transformation(
            new Vector3f(EntityNbt.at(translation, 0, 0), EntityNbt.at(translation, 1, 0),
                EntityNbt.at(translation, 2, 0)),
            rotation(stored.get("left_rotation")),
            new Vector3f(EntityNbt.at(scale, 0, 1), EntityNbt.at(scale, 1, 1), EntityNbt.at(scale, 2, 1)),
            rotation(stored.get("right_rotation")));
    }

    default void setTransformation(Transformation transformation) {
        if (transformation == null) {
            throw new IllegalArgumentException("Transformation cannot be null");
        }
        Map<String, Object> stored = Nbt.compound();
        Vector3f translation = transformation.getTranslation();
        Vector3f scale = transformation.getScale();
        stored.put("translation", EntityNbt.floats(translation.x, translation.y, translation.z));
        stored.put("left_rotation", quaternion(transformation.getLeftRotation()));
        stored.put("scale", EntityNbt.floats(scale.x, scale.y, scale.z));
        stored.put("right_rotation", quaternion(transformation.getRightRotation()));
        EntityNbt.put(core(), "transformation", stored);
    }

    default void setTransformationMatrix(Matrix4f matrix) {
        if (matrix == null) {
            throw new IllegalArgumentException("Transformation matrix cannot be null");
        }
        setTransformation(new Transformation(matrix.getTranslation(new Vector3f()),
            matrix.getNormalizedRotation(new Quaternionf()), matrix.getScale(new Vector3f()), new Quaternionf()));
    }

    default int getInterpolationDuration() {
        return core().nbtInt("interpolation_duration");
    }

    default void setInterpolationDuration(int duration) {
        core().nbtSet("interpolation_duration", "int", duration);
    }

    default int getInterpolationDelay() {
        return core().nbtInt("start_interpolation");
    }

    default void setInterpolationDelay(int ticks) {
        core().nbtSet("start_interpolation", "int", ticks);
    }

    default int getTeleportDuration() {
        return core().nbtInt("teleport_duration");
    }

    default void setTeleportDuration(int duration) {
        if (duration < 0 || duration > 59) {
            throw new IllegalArgumentException("duration (" + duration + ") cannot be lower than 0 or higher than 59");
        }
        core().nbtSet("teleport_duration", "int", duration);
    }

    default float getViewRange() {
        return (float) (core().nbtValue("view_range") instanceof Number range ? range.doubleValue() : 1.0);
    }

    default void setViewRange(float range) {
        core().nbtSet("view_range", "float", (double) range);
    }

    default float getShadowRadius() {
        return (float) core().nbtReal("shadow_radius");
    }

    default void setShadowRadius(float radius) {
        core().nbtSet("shadow_radius", "float", (double) radius);
    }

    default float getShadowStrength() {
        return (float) (core().nbtValue("shadow_strength") instanceof Number strength ? strength.doubleValue() : 1.0);
    }

    default void setShadowStrength(float strength) {
        core().nbtSet("shadow_strength", "float", (double) strength);
    }

    default float getDisplayWidth() {
        return (float) core().nbtReal("width");
    }

    default void setDisplayWidth(float width) {
        core().nbtSet("width", "float", (double) width);
    }

    default float getDisplayHeight() {
        return (float) core().nbtReal("height");
    }

    default void setDisplayHeight(float height) {
        core().nbtSet("height", "float", (double) height);
    }

    default Display.Billboard getBillboard() {
        String name = core().nbtText("billboard");
        return name == null ? Display.Billboard.FIXED : Display.Billboard.valueOf(name.toUpperCase(Locale.ROOT));
    }

    default void setBillboard(Display.Billboard billboard) {
        if (billboard == null) {
            throw new IllegalArgumentException("Billboard cannot be null");
        }
        core().nbtSet("billboard", "string", billboard.name().toLowerCase(Locale.ROOT));
    }

    default Color getGlowColorOverride() {
        int color = core().nbtInt("glow_color_override", -1);
        return color == -1 ? null : Color.fromRGB(color & 0xFFFFFF);
    }

    default void setGlowColorOverride(Color color) {
        core().nbtSet("glow_color_override", "int", color == null ? -1 : color.asRGB());
    }

    default Display.Brightness getBrightness() {
        Map<String, Object> stored = Nbt.map(EntityNbt.compound(core(), "").get("brightness"));
        return stored == null ? null
            : new Display.Brightness(Nbt.integer(stored, "block", 0), Nbt.integer(stored, "sky", 0));
    }

    default void setBrightness(Display.Brightness brightness) {
        if (brightness == null) {
            core().nbtRemove("brightness");
            return;
        }
        Map<String, Object> stored = Nbt.compound();
        stored.put("block", brightness.getBlockLight());
        stored.put("sky", brightness.getSkyLight());
        EntityNbt.put(core(), "brightness", stored);
    }
}
