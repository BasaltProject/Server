// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.world.inventory.SlotRanges;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.inventory.SlotRange;
public class SlotsArgument implements ArgumentType<SlotRange> {
    static final Collection<String> EXAMPLES = java.util.List.of("container.*", "container.5", "weapon");
    public static SlotsArgument slots() {
        return new SlotsArgument();
    }
    public static SlotRange getSlots(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, SlotRange.class);
    }
    public SlotRange parse(StringReader reader) throws CommandSyntaxException {
        return SlotRanges.read(reader);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> contextBuilder, SuggestionsBuilder builder) {
        return SharedSuggestionProvider.suggest(SlotRanges.allNames(), builder);
    }
    public Collection<String> getExamples() {
        return EXAMPLES;
    }
}
