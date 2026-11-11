// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Collectors;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.util.parsing.packrat.DelayedException;
import net.minecraft.util.parsing.packrat.Dictionary;
import net.minecraft.util.parsing.packrat.ErrorCollector;
import net.minecraft.util.parsing.packrat.ErrorEntry;
import net.minecraft.util.parsing.packrat.NamedRule;
import net.minecraft.util.parsing.packrat.ParseState;

public record Grammar<T>(Dictionary<StringReader> rules, NamedRule<StringReader, T> top)
        implements CommandArgumentParser<T> {
    public Grammar {
        rules.checkAllBound();
    }

    public Optional<T> parse(ParseState<StringReader> state) {
        return state.parseTopRule(top);
    }

    @Override
    public T parseForCommands(StringReader reader) throws CommandSyntaxException {
        ErrorCollector.LongestOnly<StringReader> errors = new ErrorCollector.LongestOnly<>();
        Optional<T> result = parse(new StringReaderParserState(errors, reader));
        if (result.isPresent()) {
            return result.get();
        }
        List<Exception> causes = new ArrayList<>();
        for (ErrorEntry<StringReader> entry : errors.entries()) {
            if (entry.reason() instanceof DelayedException<?> delayed) {
                causes.add(delayed.create(reader.getString(), entry.cursor()));
            } else if (entry.reason() instanceof Exception exception) {
                causes.add(exception);
            }
        }
        for (Exception cause : causes) {
            if (cause instanceof CommandSyntaxException syntax) {
                throw syntax;
            }
        }
        if (causes.size() == 1 && causes.get(0) instanceof RuntimeException runtime) {
            throw runtime;
        }
        throw new IllegalStateException("Failed to parse: "
            + errors.entries().stream().map(ErrorEntry::toString).collect(Collectors.joining(", ")));
    }

    @Override
    public CompletableFuture<Suggestions> parseForSuggestions(SuggestionsBuilder builder) {
        StringReader reader = new StringReader(builder.getInput());
        reader.setCursor(builder.getStart());
        ErrorCollector.LongestOnly<StringReader> errors = new ErrorCollector.LongestOnly<>();
        StringReaderParserState state = new StringReaderParserState(errors, reader);
        parse(state);
        List<ErrorEntry<StringReader>> entries = errors.entries();
        if (entries.isEmpty()) {
            return builder.buildFuture();
        }
        SuggestionsBuilder shifted = builder.createOffset(errors.cursor());
        for (ErrorEntry<StringReader> entry : entries) {
            if (entry.suggestions() instanceof ResourceSuggestion resources) {
                SharedSuggestionProvider.suggestResource(resources.possibleResources(), shifted);
            } else {
                SharedSuggestionProvider.suggest(entry.suggestions().possibleValues(state), shifted);
            }
        }
        return shifted.buildFuture();
    }
}
