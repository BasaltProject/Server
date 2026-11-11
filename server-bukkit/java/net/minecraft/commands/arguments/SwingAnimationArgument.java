// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import java.util.Arrays;
import java.util.List;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import net.minecraft.commands.SharedSuggestionProvider;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.item.SwingAnimationType;
public class SwingAnimationArgument implements ArgumentType<SwingAnimationType> {
    private static final Collection<String> kExamples = List.of(SwingAnimationType.WHACK.getSerializedName(), SwingAnimationType.STAB.getSerializedName());
    private static final DynamicCommandExceptionType kInvalid = new DynamicCommandExceptionType(value -> Component.translatableEscape("argument.swing_animation.invalid", value));

    public SwingAnimationType parse(StringReader reader) throws CommandSyntaxException {
        String text = reader.readUnquotedString();
        SwingAnimationType found = SwingAnimationType.byName(text, null);
        if (found == null) {
            throw kInvalid.createWithContext(reader, text);
        }
        return found;
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        return context.getSource() instanceof SharedSuggestionProvider
            ? SharedSuggestionProvider.suggest(Arrays.stream(SwingAnimationType.values()).map(SwingAnimationType::getSerializedName), builder) : Suggestions.empty();
    }
    public Collection<String> getExamples() {
        return kExamples;
    }
    public static SwingAnimationArgument swingAnimationType() {
        return new SwingAnimationArgument();
    }
    public static SwingAnimationType getSwingAnimationType(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, SwingAnimationType.class);
    }
}
