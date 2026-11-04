// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
public class HexColorArgument implements ArgumentType<Integer> {
    public static final DynamicCommandExceptionType ERROR_INVALID_HEX = new com.mojang.brigadier.exceptions.DynamicCommandExceptionType(value -> new com.mojang.brigadier.LiteralMessage("Invalid hex color: " + value));
    private HexColorArgument() {
        
    }
    public static HexColorArgument hexColor() {
        return new HexColorArgument();
    }
    public static Integer getHexColor(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Integer.class);
    }
    public Integer parse(StringReader reader) throws CommandSyntaxException {
        String text = reader.readUnquotedString();
        if (text.startsWith("#")) {
            text = text.substring(1);
        }
        if (text.length() != 6) {
            throw ERROR_INVALID_HEX.createWithContext(reader, text);
        }
        try {
            return Integer.parseInt(text, 16);
        } catch (NumberFormatException e) {
            throw ERROR_INVALID_HEX.createWithContext(reader, text);
        }
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> contextBuilder, SuggestionsBuilder builder) {
        return Suggestions.empty();
    }
    public Collection<String> getExamples() {
        return java.util.List.of("F00", "FF0000");
    }
}
