// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.level.GameType;

public class GameModeArgument implements ArgumentType<GameType> {
    private static final Collection<String> EXAMPLES = Arrays.asList("survival", "creative");
    private static final DynamicCommandExceptionType ERROR_INVALID = new DynamicCommandExceptionType(
        name -> new LiteralMessage("Invalid game mode '" + name + "'"));

    public GameType parse(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        String name = reader.readUnquotedString();
        GameType type = GameType.byName(name, null);
        if (type == null) {
            reader.setCursor(start);
            throw ERROR_INVALID.createWithContext(reader, name);
        }
        return type;
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        for (GameType type : GameType.values()) {
            if (type.getName().startsWith(builder.getRemainingLowerCase())) {
                builder.suggest(type.getName());
            }
        }
        return builder.buildFuture();
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }

    public static GameModeArgument gameMode() {
        return new GameModeArgument();
    }

    public static GameType getGameMode(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, GameType.class);
    }
}
