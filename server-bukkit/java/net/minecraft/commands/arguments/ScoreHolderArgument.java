// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.google.gson.JsonObject;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.function.Supplier;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.world.scores.ScoreHolder;

public class ScoreHolderArgument implements ArgumentType<ScoreHolderArgument.Result> {
    private static final Collection<String> EXAMPLES = Arrays.asList("Player", "0123", "*");
    private static final SimpleCommandExceptionType ERROR_NO_RESULTS = new SimpleCommandExceptionType(new LiteralMessage("No scores matched the selector"));
    private final boolean multiple;

    public ScoreHolderArgument(boolean multiple) {
        this.multiple = multiple;
    }

    public static ScoreHolder getName(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return getNames(context, name).iterator().next();
    }

    public static Collection<ScoreHolder> getNames(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return getNames(context, name, List::of);
    }

    public static Collection<ScoreHolder> getNamesWithDefaultWildcard(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return getNames(context, name, () -> context.getSource().getServer().getScoreboard().getTrackedPlayers());
    }

    public static Collection<ScoreHolder> getNames(CommandContext<CommandSourceStack> context, String name, Supplier<Collection<ScoreHolder>> wildcard) throws CommandSyntaxException {
        Collection<ScoreHolder> found = context.getArgument(name, Result.class).getNames(context.getSource(), wildcard);
        if (found.isEmpty()) {
            throw ERROR_NO_RESULTS.create();
        }
        return found;
    }

    public static ScoreHolderArgument scoreHolder() {
        return new ScoreHolderArgument(false);
    }

    public static ScoreHolderArgument scoreHolders() {
        return new ScoreHolderArgument(true);
    }

    public Result parse(StringReader reader) throws CommandSyntaxException {
        if (!reader.canRead()) {
            throw ERROR_NO_RESULTS.createWithContext(reader);
        }
        if (reader.peek() == '@') {
            net.minecraft.commands.arguments.selector.EntitySelector selector = new EntityArgument(!multiple, false).parse(reader);
            return new SelectorResult(selector);
        }
        String text = reader.readUnquotedString();
        if (text.equals("*")) {
            return (sender, wildcard) -> wildcard.get();
        }
        List<ScoreHolder> single = List.of(ScoreHolder.forNameOnly(text));
        return (sender, wildcard) -> single;
    }

    public <S> Result parse(StringReader reader, S source) throws CommandSyntaxException {
        return parse(reader);
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }

    public static class SelectorResult implements Result {
        private final net.minecraft.commands.arguments.selector.EntitySelector selector;

        public SelectorResult(net.minecraft.commands.arguments.selector.EntitySelector selector) {
            this.selector = selector;
        }

        public Collection<ScoreHolder> getNames(CommandSourceStack sender, Supplier<Collection<ScoreHolder>> wildcard) throws CommandSyntaxException {
            List<? extends net.minecraft.world.entity.Entity> found = selector.findEntities(sender);
            if (found.isEmpty()) {
                throw net.minecraft.commands.arguments.EntityArgument.NO_ENTITIES_FOUND.create();
            }
            return List.copyOf(found);
        }
    }

    public static class Info implements ArgumentTypeInfo<ScoreHolderArgument, Info.Template> {
        private static final int kMultiple = 1;

        public void serializeToNetwork(Template template, FriendlyByteBuf out) {
            out.writeByte(template.multiple ? kMultiple : 0);
        }

        public Template deserializeFromNetwork(FriendlyByteBuf in) {
            return new Template((in.readByte() & kMultiple) != 0);
        }

        public void serializeToJson(Template template, JsonObject out) {
            out.addProperty("amount", template.multiple ? "multiple" : "single");
        }

        public Template unpack(ScoreHolderArgument argument) {
            return new Template(argument.multiple);
        }

        public final class Template implements ArgumentTypeInfo.Template<ScoreHolderArgument> {
            final boolean multiple;

            private Template(boolean multiple) {
                this.multiple = multiple;
            }

            public ScoreHolderArgument instantiate(CommandBuildContext context) {
                return new ScoreHolderArgument(multiple);
            }

            public ArgumentTypeInfo<ScoreHolderArgument, ?> type() {
                return Info.this;
            }
        }
    }

    @FunctionalInterface
    public interface Result {
        Collection<ScoreHolder> getNames(CommandSourceStack sender, Supplier<Collection<ScoreHolder>> wildcard) throws CommandSyntaxException;
    }
}
