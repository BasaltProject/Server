// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.util.parsing.packrat.DelayedException;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Rule;

public abstract class GreedyPredicateParseRule implements Rule<StringReader, String> {
    private final int minSize;
    private final int maxSize;
    private final DelayedException<CommandSyntaxException> error;

    public GreedyPredicateParseRule(int minSize, DelayedException<CommandSyntaxException> error) {
        this(minSize, Integer.MAX_VALUE, error);
    }

    public GreedyPredicateParseRule(int minSize, int maxSize, DelayedException<CommandSyntaxException> error) {
        this.minSize = minSize;
        this.maxSize = maxSize;
        this.error = error;
    }

    @Override
    public String parse(ParseState<StringReader> state) {
        StringReader input = state.input();
        String text = input.getString();
        int start = input.getCursor();
        int end = start;
        while (end < text.length() && end - start < maxSize && isAccepted(text.charAt(end))) {
            end++;
        }
        if (end - start < minSize) {
            state.errorCollector().store(state.mark(), error);
            return null;
        }
        input.setCursor(end);
        return text.substring(start, end);
    }

    protected abstract boolean isAccepted(char c);
}
