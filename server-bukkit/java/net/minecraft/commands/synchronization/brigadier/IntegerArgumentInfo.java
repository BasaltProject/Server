// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization.brigadier;

import com.google.gson.JsonObject;
import com.mojang.brigadier.arguments.IntegerArgumentType;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.network.FriendlyByteBuf;

public class IntegerArgumentInfo implements ArgumentTypeInfo<IntegerArgumentType, IntegerArgumentInfo.Template> {
    private static final int kHasMin = 1;
    private static final int kHasMax = 2;

    public void serializeToNetwork(Template template, FriendlyByteBuf out) {
        boolean hasMin = template.min != Integer.MIN_VALUE;
        boolean hasMax = template.max != Integer.MAX_VALUE;
        out.writeByte((hasMin ? kHasMin : 0) | (hasMax ? kHasMax : 0));
        if (hasMin) {
            out.writeInt(template.min);
        }
        if (hasMax) {
            out.writeInt(template.max);
        }
    }

    public Template deserializeFromNetwork(FriendlyByteBuf in) {
        byte flags = in.readByte();
        int min = (flags & kHasMin) != 0 ? in.readInt() : Integer.MIN_VALUE;
        int max = (flags & kHasMax) != 0 ? in.readInt() : Integer.MAX_VALUE;
        return new Template(min, max);
    }

    public void serializeToJson(Template template, JsonObject out) {
        if (template.min != Integer.MIN_VALUE) {
            out.addProperty("min", template.min);
        }
        if (template.max != Integer.MAX_VALUE) {
            out.addProperty("max", template.max);
        }
    }

    public Template unpack(IntegerArgumentType argument) {
        return new Template(argument.getMinimum(), argument.getMaximum());
    }

    public final class Template implements ArgumentTypeInfo.Template<IntegerArgumentType> {
        final int min;
        final int max;

        Template(int min, int max) {
            this.min = min;
            this.max = max;
        }

        public IntegerArgumentType instantiate(CommandBuildContext context) {
            return IntegerArgumentType.integer(min, max);
        }

        public ArgumentTypeInfo<IntegerArgumentType, ?> type() {
            return IntegerArgumentInfo.this;
        }
    }
}
