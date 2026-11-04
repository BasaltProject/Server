// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization.brigadier;

import com.google.gson.JsonObject;
import com.mojang.brigadier.arguments.StringArgumentType;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.network.FriendlyByteBuf;

public class StringArgumentSerializer implements ArgumentTypeInfo<StringArgumentType, StringArgumentSerializer.Template> {
    public void serializeToNetwork(Template template, FriendlyByteBuf out) {
        out.writeEnum(template.type);
    }

    public Template deserializeFromNetwork(FriendlyByteBuf in) {
        return new Template(in.readEnum(StringArgumentType.StringType.class));
    }

    public void serializeToJson(Template template, JsonObject out) {
        out.addProperty("type", switch (template.type) {
            case SINGLE_WORD -> "word";
            case QUOTABLE_PHRASE -> "phrase";
            case GREEDY_PHRASE -> "greedy";
        });
    }

    public Template unpack(StringArgumentType argument) {
        return new Template(argument.getType());
    }

    public final class Template implements ArgumentTypeInfo.Template<StringArgumentType> {
        final StringArgumentType.StringType type;

        public Template(StringArgumentType.StringType type) {
            this.type = type;
        }

        public StringArgumentType instantiate(CommandBuildContext context) {
            return switch (type) {
                case SINGLE_WORD -> StringArgumentType.word();
                case QUOTABLE_PHRASE -> StringArgumentType.string();
                case GREEDY_PHRASE -> StringArgumentType.greedyString();
            };
        }

        public ArgumentTypeInfo<StringArgumentType, ?> type() {
            return StringArgumentSerializer.this;
        }
    }
}
