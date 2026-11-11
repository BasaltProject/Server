// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.inventory.SlotRange;
import net.minecraft.world.inventory.SlotRanges;
import net.minecraft.world.item.slot.RangeSlotSource;
import net.minecraft.world.item.slot.SlotSources;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.Holder;
import net.minecraft.world.inventory.SlotRange;
import net.minecraft.world.item.slot.SlotSource;
public class SlotSourceArgument implements ArgumentType<SlotSourceArgument.Result> {
    private static final Collection<String> EXAMPLES = java.util.stream.Stream.concat(ResourceOrIdArgument.EXAMPLES.stream(), SlotsArgument.EXAMPLES.stream()).toList();
    private final ArgumentType<Holder<SlotSource>> holderArgument;

    private SlotSourceArgument(CommandBuildContext context) {
        this.holderArgument = new ResourceOrIdArgument<SlotSource>(context, (ResourceKey) Registries.SLOT_SOURCE, SlotSources.DIRECT_CODEC) {};
        
    }
    public static SlotSourceArgument slotSource(CommandBuildContext context) {
        return new SlotSourceArgument(context);
    }
    public static Result getSlotSource(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Result.class);
    }
    public Result parse(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        SlotRange range = SlotRanges.tryRead(reader);
        if (range != null) {
            return new LiteralResult(range);
        }
        reader.setCursor(start);
        return new HolderResult(holderArgument.parse(reader));
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> contextBuilder, SuggestionsBuilder builder) {
        SuggestionsBuilder names = builder.restart();
        SharedSuggestionProvider.suggest(SlotRanges.allNames(), names);
        builder.add(names);
        return holderArgument.listSuggestions(contextBuilder, builder);
    }
    public Collection<String> getExamples() {
        return EXAMPLES;
    }
    public static record HolderResult(Holder<SlotSource> holder) implements Result {
        public SlotSource value() {
            return holder.value();
        }
        public Optional<String> name() {
            return holder.getRegisteredNameIfPresent();
        }
    }
    public static record LiteralResult(SlotRange slotRange) implements Result {
        public SlotSource value() {
            return RangeSlotSource.slotRange(slotRange);
        }
        public Optional<String> name() {
            return Optional.of(slotRange.getSerializedName());
        }
    }
    public interface Result {
        SlotSource value();
        Optional<String> name();
    }
}
