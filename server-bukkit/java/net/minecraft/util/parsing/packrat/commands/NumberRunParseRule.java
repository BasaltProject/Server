// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.util.parsing.packrat.DelayedException;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Rule;

public abstract class NumberRunParseRule implements Rule<StringReader, String> {
    private final DelayedException<CommandSyntaxException> noValueError;
    private final DelayedException<CommandSyntaxException> underscoreNotAllowedError;

    public NumberRunParseRule(DelayedException<CommandSyntaxException> noValueError, DelayedException<CommandSyntaxException> underscoreNotAllowedError) {
        this.noValueError = noValueError;
        this.underscoreNotAllowedError = underscoreNotAllowedError;
    }

    @Override
    public String parse(ParseState<StringReader> state) {
        StringReader input = state.input();
        input.skipWhitespace();
        String text = input.getString();
        int start = input.getCursor();
        int end = start;
        while (end < text.length() && isAccepted(text.charAt(end))) {
            end++;
        }
        if (end == start) {
            state.errorCollector().store(state.mark(), noValueError);
            return null;
        }
        if (text.charAt(start) == '_' || text.charAt(end - 1) == '_') {
            state.errorCollector().store(state.mark(), underscoreNotAllowedError);
            return null;
        }
        input.setCursor(end);
        return text.substring(start, end);
    }

    protected abstract boolean isAccepted(char c);
}
