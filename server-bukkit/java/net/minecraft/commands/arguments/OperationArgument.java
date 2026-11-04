// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.Component;
import net.minecraft.world.scores.ScoreAccess;

public class OperationArgument implements ArgumentType<OperationArgument.Operation> {
    private static final Collection<String> EXAMPLES = Arrays.asList("=", ">", "<");
    private static final String[] kNames = {"=", "+=", "-=", "*=", "/=", "%=", "<", ">", "><"};
    private static final SimpleCommandExceptionType ERROR_INVALID_OPERATION = new SimpleCommandExceptionType(Component.translatable("arguments.operation.invalid"));
    private static final SimpleCommandExceptionType ERROR_DIVIDE_BY_ZERO = new SimpleCommandExceptionType(Component.translatable("arguments.operation.div0"));

    public static OperationArgument operation() {
        return new OperationArgument();
    }

    public static Operation getOperation(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Operation.class);
    }

    public Operation parse(StringReader reader) throws CommandSyntaxException {
        if (!reader.canRead()) {
            throw ERROR_INVALID_OPERATION.createWithContext(reader);
        }
        int start = reader.getCursor();
        while (reader.canRead() && reader.peek() != ' ') {
            reader.skip();
        }
        return named(reader.getString().substring(start, reader.getCursor()), reader);
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        for (String name : kNames) {
            if (name.startsWith(builder.getRemaining())) {
                builder.suggest(name);
            }
        }
        return builder.buildFuture();
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }

    private static Operation named(String name, StringReader reader) throws CommandSyntaxException {
        return switch (name) {
            case "=" -> (SimpleOperation) (a, b) -> b;
            case "+=" -> (SimpleOperation) (a, b) -> a + b;
            case "-=" -> (SimpleOperation) (a, b) -> a - b;
            case "*=" -> (SimpleOperation) (a, b) -> a * b;
            case "/=" -> (SimpleOperation) (a, b) -> {
                if (b == 0) {
                    throw ERROR_DIVIDE_BY_ZERO.create();
                }
                return Math.floorDiv(a, b);
            };
            case "%=" -> (SimpleOperation) (a, b) -> {
                if (b == 0) {
                    throw ERROR_DIVIDE_BY_ZERO.create();
                }
                return Math.floorMod(a, b);
            };
            case "<" -> (SimpleOperation) Math::min;
            case ">" -> (SimpleOperation) Math::max;
            case "><" -> (a, b) -> {
                int held = a.get();
                a.set(b.get());
                b.set(held);
            };
            default -> throw ERROR_INVALID_OPERATION.createWithContext(reader);
        };
    }

    interface SimpleOperation extends Operation {
        int apply(int a, int b) throws CommandSyntaxException;

        default void apply(ScoreAccess a, ScoreAccess b) throws CommandSyntaxException {
            a.set(apply(a.get(), b.get()));
        }
    }

    public interface Operation {
        void apply(ScoreAccess a, ScoreAccess b) throws CommandSyntaxException;
    }
}
