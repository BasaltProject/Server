// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.StringReader;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
import net.minecraft.nbt.TagParser;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Rule;

public class TagParseRule<T> implements Rule<StringReader, Dynamic<?>> {
    private final TagParser<T> parser;

    public TagParseRule(DynamicOps<T> ops) {
        this.parser = TagParser.create(ops);
    }

    @Override
    public Dynamic<T> parse(ParseState<StringReader> state) {
        state.input().skipWhitespace();
        int start = state.mark();
        try {
            return new Dynamic<>(parser.getOps(), parser.parseAsArgument(state.input()));
        } catch (Exception failure) {
            state.errorCollector().store(start, failure);
            return null;
        }
    }
}
