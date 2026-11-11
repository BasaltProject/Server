// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import java.util.concurrent.CompletableFuture;
import java.util.function.Function;

public interface CommandArgumentParser<T> {
    T parseForCommands(StringReader reader) throws CommandSyntaxException;

    CompletableFuture<Suggestions> parseForSuggestions(SuggestionsBuilder suggestionsBuilder);

    default <S> CommandArgumentParser<S> mapResult(Function<T, S> mapper) {
        CommandArgumentParser<T> source = this;
        return new CommandArgumentParser<>() {
            @Override
            public S parseForCommands(StringReader reader) throws CommandSyntaxException {
                return mapper.apply(source.parseForCommands(reader));
            }

            @Override
            public CompletableFuture<Suggestions> parseForSuggestions(SuggestionsBuilder builder) {
                return source.parseForSuggestions(builder);
            }
        };
    }

    default <T, O> CommandArgumentParser<T> withCodec(DynamicOps<O> ops, CommandArgumentParser<O> valueParser,
            Codec<T> codec, DynamicCommandExceptionType exceptionType) {
        CommandArgumentParser<?> source = this;
        return new CommandArgumentParser<>() {
            @Override
            public T parseForCommands(StringReader reader) throws CommandSyntaxException {
                int start = reader.getCursor();
                O raw = valueParser.parseForCommands(reader);
                return codec.parse(ops, raw).getOrThrow(message -> {
                    reader.setCursor(start);
                    return exceptionType.createWithContext(reader, message);
                });
            }

            @Override
            public CompletableFuture<Suggestions> parseForSuggestions(SuggestionsBuilder builder) {
                return source.parseForSuggestions(builder);
            }
        };
    }
}
