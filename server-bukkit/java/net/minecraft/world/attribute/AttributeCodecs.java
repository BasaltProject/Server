// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import org.joml.Vector3f;
import org.joml.Vector3fc;
import org.joml.Vector4f;
import org.joml.Vector4fc;

final class AttributeCodecs {
    static final Codec<Vector3fc> RGB = Codec.STRING.comapFlatMap(text -> parse(text, 6).map(AttributeCodecs::rgb),
        vector -> String.format("#%06x", pack(vector.x(), vector.y(), vector.z()) & 0xFFFFFF));
    static final Codec<Vector4fc> ARGB = Codec.STRING.comapFlatMap(text -> parse(text, 8).map(AttributeCodecs::argb),
        vector -> String.format("#%08x", (pack(vector.x(), vector.y(), vector.z()) & 0xFFFFFF) | (Math.round(vector.w() * 255.0F) << 24)));

    private AttributeCodecs() {}

    private static DataResult<Integer> parse(String text, int digits) {
        if (text.length() != digits + 1 || text.charAt(0) != '#') {
            return DataResult.error(() -> "expected #" + "x".repeat(digits) + " but got " + text);
        }
        try {
            return DataResult.success((int) Long.parseLong(text.substring(1), 16));
        } catch (NumberFormatException e) {
            return DataResult.error(() -> "not a hex color: " + text);
        }
    }

    private static int pack(float r, float g, float b) {
        return (Math.round(r * 255.0F) << 16) | (Math.round(g * 255.0F) << 8) | Math.round(b * 255.0F);
    }

    private static Vector3fc rgb(int value) {
        return new Vector3f(((value >> 16) & 0xFF) / 255.0F, ((value >> 8) & 0xFF) / 255.0F, (value & 0xFF) / 255.0F);
    }

    private static Vector4fc argb(int value) {
        return new Vector4f(((value >> 16) & 0xFF) / 255.0F, ((value >> 8) & 0xFF) / 255.0F, (value & 0xFF) / 255.0F, ((value >>> 24) & 0xFF) / 255.0F);
    }
}
