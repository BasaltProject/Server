// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import net.minecraft.commands.SharedSuggestionProvider;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import com.mojang.serialization.Codec;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;
import net.minecraft.util.StringRepresentable;
public class StringRepresentableArgument<T extends Enum<T> & StringRepresentable> implements ArgumentType<T> {
    private static final DynamicCommandExceptionType kInvalid = new DynamicCommandExceptionType(value -> Component.translatableEscape("argument.enum.invalid", value));
    private final Supplier<T[]> values;

    private java.util.stream.Stream<String> names() {
        return java.util.Arrays.stream(values.get()).map(value -> convertId(value.getSerializedName()));
    }

    protected StringRepresentableArgument(Codec<T> codec, Supplier<T[]> values) {
        this.values = values;
        
    }
    public T parse(StringReader reader) throws CommandSyntaxException {
        String text = reader.readUnquotedString();
        String wanted = convertId(text);
        for (T candidate : values.get()) {
            if (candidate.getSerializedName().equals(wanted)) {
                return candidate;
            }
        }
        throw kInvalid.createWithContext(reader, text);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        return SharedSuggestionProvider.suggest(names().toList(), builder);
    }
    public Collection<String> getExamples() {
        return names().limit(2).toList();
    }
    protected String convertId(String id) {
        return id;
    }
}
