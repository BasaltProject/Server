// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Arrays;
import java.util.Collection;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import com.google.gson.JsonObject;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.chat.Component;

public class TimeArgument implements ArgumentType<Integer> {
    private static final Collection<String> EXAMPLES = Arrays.asList("0d", "0s", "0t", "0");
    private static final SimpleCommandExceptionType ERROR_INVALID_UNIT = new SimpleCommandExceptionType(Component.translatable("argument.time.invalid_unit"));
    private static final Dynamic2CommandExceptionType ERROR_TICK_COUNT_TOO_LOW = new Dynamic2CommandExceptionType(
        (minimum, ticks) -> Component.translatable("argument.time.tick_count_too_low", minimum, ticks));
    private static final Map<String, Integer> UNITS = Map.of("d", 24000, "s", 20, "t", 1, "", 1);
    private final int minimum;

    private TimeArgument(int minimum) {
        this.minimum = minimum;
    }

    public static TimeArgument time() {
        return new TimeArgument(0);
    }

    public static TimeArgument time(int minimum) {
        return new TimeArgument(minimum);
    }

    public Integer parse(StringReader reader) throws CommandSyntaxException {
        float value = reader.readFloat();
        String unit = reader.readUnquotedString();
        Integer scale = UNITS.get(unit);
        if (scale == null) {
            throw ERROR_INVALID_UNIT.createWithContext(reader);
        }
        int ticks = Math.round(value * scale);
        if (ticks < minimum) {
            throw ERROR_TICK_COUNT_TOO_LOW.createWithContext(reader, ticks, minimum);
        }
        return ticks;
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        StringReader reader = new StringReader(builder.getRemaining());
        try {
            reader.readFloat();
        } catch (CommandSyntaxException e) {
            return builder.buildFuture();
        }
        SuggestionsBuilder unitBuilder = builder.createOffset(builder.getStart() + reader.getCursor());
        for (String unit : UNITS.keySet()) {
            if (!unit.isEmpty() && unit.startsWith(unitBuilder.getRemaining())) {
                unitBuilder.suggest(unit);
            }
        }
        return unitBuilder.buildFuture();
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }

    public static class Info implements ArgumentTypeInfo<TimeArgument, Info.Template> {
        public void serializeToNetwork(Template template, FriendlyByteBuf out) {
            out.writeInt(template.minimum);
        }

        public Template deserializeFromNetwork(FriendlyByteBuf in) {
            return new Template(in.readInt());
        }

        public void serializeToJson(Template template, JsonObject out) {
            out.addProperty("min", template.minimum);
        }

        public Template unpack(TimeArgument argument) {
            return new Template(argument.minimum);
        }

        public final class Template implements ArgumentTypeInfo.Template<TimeArgument> {
            final int minimum;

            Template(int minimum) {
                this.minimum = minimum;
            }

            public TimeArgument instantiate(CommandBuildContext context) {
                return new TimeArgument(minimum);
            }

            public ArgumentTypeInfo<TimeArgument, ?> type() {
                return Info.this;
            }
        }
    }
}
