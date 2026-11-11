// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.world.inventory.SlotRange;
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
public class SlotArgument implements ArgumentType<Integer> {
    public static SlotArgument slot() {
        return new SlotArgument();
    }
    public static int getSlot(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Integer.class);
    }
    public Integer parse(StringReader reader) throws CommandSyntaxException {
        String text = reader.readUnquotedString();
        SlotRange range = SlotRanges.nameToIds(text);
        if (range == null || range.size() != 1) {
            throw new DynamicCommandExceptionType(value -> Component.translatableEscape("slot.unknown", value)).createWithContext(reader, text);
        }
        return range.slots().getInt(0);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> contextBuilder, SuggestionsBuilder builder) {
        return SharedSuggestionProvider.suggest(SlotRanges.singleSlotNames(), builder);
    }
    public Collection<String> getExamples() {
        return java.util.List.of("container.5", "weapon");
    }
}
