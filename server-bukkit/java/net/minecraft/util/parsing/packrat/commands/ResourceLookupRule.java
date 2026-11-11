// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.ImmutableStringReader;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.resources.Identifier;
import net.minecraft.util.parsing.packrat.DelayedException;
import net.minecraft.util.parsing.packrat.NamedRule;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Rule;

public abstract class ResourceLookupRule<C, V> implements Rule<StringReader, V>, ResourceSuggestion {
    private final NamedRule<StringReader, Identifier> idParser;
    protected final C context;
    private final DelayedException<CommandSyntaxException> invalid = DelayedException.create(Identifier.ERROR_INVALID);

    protected ResourceLookupRule(NamedRule<StringReader, Identifier> idParser, C context) {
        this.idParser = idParser;
        this.context = context;
    }

    @Override
    public V parse(ParseState<StringReader> state) {
        state.input().skipWhitespace();
        int start = state.mark();
        Identifier id = state.parse(idParser);
        if (id == null) {
            state.errorCollector().store(start, this, invalid);
            return null;
        }
        try {
            return validateElement(state.input(), id);
        } catch (Exception failure) {
            state.errorCollector().store(start, this, failure);
            return null;
        }
    }

    protected abstract V validateElement(ImmutableStringReader reader, Identifier id) throws Exception;
}
