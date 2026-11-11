// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.resources.Identifier;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Rule;

public class IdentifierParseRule implements Rule<StringReader, Identifier> {
    public static final Rule<StringReader, Identifier> INSTANCE = new IdentifierParseRule();

    private IdentifierParseRule() {}

    @Override
    public Identifier parse(ParseState<StringReader> state) {
        state.input().skipWhitespace();
        try {
            return Identifier.readNonEmpty(state.input());
        } catch (CommandSyntaxException invalid) {
            return null;
        }
    }
}
