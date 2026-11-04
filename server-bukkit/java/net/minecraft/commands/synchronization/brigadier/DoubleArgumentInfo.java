// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization.brigadier;

import com.google.gson.JsonObject;
import com.mojang.brigadier.arguments.DoubleArgumentType;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.network.FriendlyByteBuf;

public class DoubleArgumentInfo implements ArgumentTypeInfo<DoubleArgumentType, DoubleArgumentInfo.Template> {
    private static final int kHasMin = 1;
    private static final int kHasMax = 2;

    public void serializeToNetwork(Template template, FriendlyByteBuf out) {
        boolean hasMin = template.min != -Double.MAX_VALUE;
        boolean hasMax = template.max != Double.MAX_VALUE;
        out.writeByte((hasMin ? kHasMin : 0) | (hasMax ? kHasMax : 0));
        if (hasMin) {
            out.writeDouble(template.min);
        }
        if (hasMax) {
            out.writeDouble(template.max);
        }
    }

    public Template deserializeFromNetwork(FriendlyByteBuf in) {
        byte flags = in.readByte();
        double min = (flags & kHasMin) != 0 ? in.readDouble() : -Double.MAX_VALUE;
        double max = (flags & kHasMax) != 0 ? in.readDouble() : Double.MAX_VALUE;
        return new Template(min, max);
    }

    public void serializeToJson(Template template, JsonObject out) {
        if (template.min != -Double.MAX_VALUE) {
            out.addProperty("min", template.min);
        }
        if (template.max != Double.MAX_VALUE) {
            out.addProperty("max", template.max);
        }
    }

    public Template unpack(DoubleArgumentType argument) {
        return new Template(argument.getMinimum(), argument.getMaximum());
    }

    public final class Template implements ArgumentTypeInfo.Template<DoubleArgumentType> {
        final double min;
        final double max;

        Template(double min, double max) {
            this.min = min;
            this.max = max;
        }

        public DoubleArgumentType instantiate(CommandBuildContext context) {
            return DoubleArgumentType.doubleArg(min, max);
        }

        public ArgumentTypeInfo<DoubleArgumentType, ?> type() {
            return DoubleArgumentInfo.this;
        }
    }
}
