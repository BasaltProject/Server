// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.item;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.datafixers.util.Either;
import com.mojang.datafixers.util.Pair;
import java.util.Collection;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.functions.CommandFunction;
import net.minecraft.resources.Identifier;
public class FunctionArgument implements ArgumentType<FunctionArgument.Result> {
    private static final Collection<String> kExamples = java.util.List.of("foo", "foo:bar", "#foo");
    private static final DynamicCommandExceptionType kUnknownTag = new DynamicCommandExceptionType(id -> Component.translatableEscape("arguments.function.tag.unknown", id));
    private static final DynamicCommandExceptionType kUnknownFunction = new DynamicCommandExceptionType(id -> Component.translatableEscape("arguments.function.unknown", id));

    private record Resolved(Identifier id, boolean tag) implements Result {
        private CommandFunction<CommandSourceStack> one(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
            var found = context.getSource().getServer().getFunctions().get(id);
            if (found.isEmpty()) {
                throw kUnknownFunction.create(id.toString());
            }
            return found.get();
        }

        private Collection<CommandFunction<CommandSourceStack>> many(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
            Collection<CommandFunction<CommandSourceStack>> found = context.getSource().getServer().getFunctions().getTag(id);
            if (found == null) {
                throw kUnknownTag.create(id.toString());
            }
            return found;
        }

        public Collection<CommandFunction<CommandSourceStack>> create(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
            return tag ? many(context) : java.util.Collections.singleton(one(context));
        }

        public Pair<Identifier, Either<CommandFunction<CommandSourceStack>, Collection<CommandFunction<CommandSourceStack>>>> unwrap(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
            return Pair.of(id, tag ? Either.right(many(context)) : Either.left(one(context)));
        }

        public Pair<Identifier, Collection<CommandFunction<CommandSourceStack>>> unwrapToCollection(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
            return Pair.of(id, create(context));
        }
    }

    public static FunctionArgument functions() {
        return new FunctionArgument();
    }
    public Result parse(StringReader reader) throws CommandSyntaxException {
        boolean tag = reader.canRead() && reader.peek() == '#';
        if (tag) {
            reader.skip();
        }
        Identifier id = Identifier.read(reader);
        return new Resolved(id, tag);
    }
    public static Collection<CommandFunction<CommandSourceStack>> getFunctions(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, Result.class).create(context);
    }
    public static Pair<Identifier, Either<CommandFunction<CommandSourceStack>, Collection<CommandFunction<CommandSourceStack>>>> getFunctionOrTag(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, Result.class).unwrap(context);
    }
    public static Pair<Identifier, Collection<CommandFunction<CommandSourceStack>>> getFunctionCollection(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, Result.class).unwrapToCollection(context);
    }
    public Collection<String> getExamples() {
        return kExamples;
    }
    public interface Result {
        Collection<CommandFunction<CommandSourceStack>> create(CommandContext<CommandSourceStack> context) throws CommandSyntaxException;
        Pair<Identifier, Either<CommandFunction<CommandSourceStack>, Collection<CommandFunction<CommandSourceStack>>>> unwrap(CommandContext<CommandSourceStack> context) throws CommandSyntaxException;
        Pair<Identifier, Collection<CommandFunction<CommandSourceStack>>> unwrapToCollection(CommandContext<CommandSourceStack> context) throws CommandSyntaxException;
    }
}
