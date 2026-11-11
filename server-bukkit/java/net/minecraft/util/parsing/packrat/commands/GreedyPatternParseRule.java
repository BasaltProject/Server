// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import net.minecraft.util.parsing.packrat.DelayedException;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Rule;

public final class GreedyPatternParseRule implements Rule<StringReader, String> {
    private final Pattern pattern;
    private final DelayedException<CommandSyntaxException> error;

    public GreedyPatternParseRule(Pattern pattern, DelayedException<CommandSyntaxException> error) {
        this.pattern = pattern;
        this.error = error;
    }

    @Override
    public String parse(ParseState<StringReader> state) {
        StringReader input = state.input();
        String text = input.getString();
        Matcher matcher = pattern.matcher(text).region(input.getCursor(), text.length());
        if (!matcher.lookingAt()) {
            state.errorCollector().store(state.mark(), error);
            return null;
        }
        input.setCursor(matcher.end());
        return matcher.group();
    }
}
