// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.resources.ResourceKey;
import net.minecraft.core.registries.Registries;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
public class DimensionArgument implements ArgumentType<Identifier> {
    public Identifier parse(StringReader reader) throws CommandSyntaxException {
        return Identifier.read(reader);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        return context.getSource() instanceof SharedSuggestionProvider provider
            ? SharedSuggestionProvider.suggestResource(provider.levels().stream().map(ResourceKey::identifier), builder) : Suggestions.empty();
    }
    public Collection<String> getExamples() {
        return java.util.List.of("overworld", "the_nether");
    }
    public static DimensionArgument dimension() {
        return new DimensionArgument();
    }
    public static ServerLevel getDimension(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        Identifier id = context.getArgument(name, Identifier.class);
        ServerLevel level = context.getSource().getServer().getLevel(ResourceKey.create(Registries.DIMENSION, id));
        if (level == null) {
            throw new DynamicCommandExceptionType(value -> Component.translatableEscape("argument.dimension.invalid", value)).create(id);
        }
        return level;
    }
}
