// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization.brigadier;

import com.google.gson.JsonObject;
import com.mojang.brigadier.arguments.FloatArgumentType;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.network.FriendlyByteBuf;

public class FloatArgumentInfo implements ArgumentTypeInfo<FloatArgumentType, FloatArgumentInfo.Template> {
    private static final int kHasMin = 1;
    private static final int kHasMax = 2;

    public void serializeToNetwork(Template template, FriendlyByteBuf out) {
        boolean hasMin = template.min != -Float.MAX_VALUE;
        boolean hasMax = template.max != Float.MAX_VALUE;
        out.writeByte((hasMin ? kHasMin : 0) | (hasMax ? kHasMax : 0));
        if (hasMin) {
            out.writeFloat(template.min);
        }
        if (hasMax) {
            out.writeFloat(template.max);
        }
    }

    public Template deserializeFromNetwork(FriendlyByteBuf in) {
        byte flags = in.readByte();
        float min = (flags & kHasMin) != 0 ? in.readFloat() : -Float.MAX_VALUE;
        float max = (flags & kHasMax) != 0 ? in.readFloat() : Float.MAX_VALUE;
        return new Template(min, max);
    }

    public void serializeToJson(Template template, JsonObject out) {
        if (template.min != -Float.MAX_VALUE) {
            out.addProperty("min", template.min);
        }
        if (template.max != Float.MAX_VALUE) {
            out.addProperty("max", template.max);
        }
    }

    public Template unpack(FloatArgumentType argument) {
        return new Template(argument.getMinimum(), argument.getMaximum());
    }

    public final class Template implements ArgumentTypeInfo.Template<FloatArgumentType> {
        final float min;
        final float max;

        Template(float min, float max) {
            this.min = min;
            this.max = max;
        }

        public FloatArgumentType instantiate(CommandBuildContext context) {
            return FloatArgumentType.floatArg(min, max);
        }

        public ArgumentTypeInfo<FloatArgumentType, ?> type() {
            return FloatArgumentInfo.this;
        }
    }
}
