// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization.brigadier;

import com.google.gson.JsonObject;
import com.mojang.brigadier.arguments.LongArgumentType;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.network.FriendlyByteBuf;

public class LongArgumentInfo implements ArgumentTypeInfo<LongArgumentType, LongArgumentInfo.Template> {
    private static final int kHasMin = 1;
    private static final int kHasMax = 2;

    public void serializeToNetwork(Template template, FriendlyByteBuf out) {
        boolean hasMin = template.min != Long.MIN_VALUE;
        boolean hasMax = template.max != Long.MAX_VALUE;
        out.writeByte((hasMin ? kHasMin : 0) | (hasMax ? kHasMax : 0));
        if (hasMin) {
            out.writeLong(template.min);
        }
        if (hasMax) {
            out.writeLong(template.max);
        }
    }

    public Template deserializeFromNetwork(FriendlyByteBuf in) {
        byte flags = in.readByte();
        long min = (flags & kHasMin) != 0 ? in.readLong() : Long.MIN_VALUE;
        long max = (flags & kHasMax) != 0 ? in.readLong() : Long.MAX_VALUE;
        return new Template(min, max);
    }

    public void serializeToJson(Template template, JsonObject out) {
        if (template.min != Long.MIN_VALUE) {
            out.addProperty("min", template.min);
        }
        if (template.max != Long.MAX_VALUE) {
            out.addProperty("max", template.max);
        }
    }

    public Template unpack(LongArgumentType argument) {
        return new Template(argument.getMinimum(), argument.getMaximum());
    }

    public final class Template implements ArgumentTypeInfo.Template<LongArgumentType> {
        final long min;
        final long max;

        Template(long min, long max) {
            this.min = min;
            this.max = max;
        }

        public LongArgumentType instantiate(CommandBuildContext context) {
            return LongArgumentType.longArg(min, max);
        }

        public ArgumentTypeInfo<LongArgumentType, ?> type() {
            return LongArgumentInfo.this;
        }
    }
}
