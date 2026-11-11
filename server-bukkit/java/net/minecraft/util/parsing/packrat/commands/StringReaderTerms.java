// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat.commands;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import it.unimi.dsi.fastutil.chars.CharList;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import net.minecraft.util.parsing.packrat.Control;
import net.minecraft.util.parsing.packrat.DelayedException;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Scope;
import net.minecraft.util.parsing.packrat.SuggestionSupplier;
import net.minecraft.util.parsing.packrat.Term;

public interface StringReaderTerms {
    static Term<StringReader> word(String value) {
        return new TerminalWord(value);
    }

    static Term<StringReader> character(char value) {
        return new TerminalCharacters(CharList.of(value)) {
            @Override
            protected boolean isAccepted(char candidate) {
                return value == candidate;
            }
        };
    }

    static Term<StringReader> characters(char v1, char v2) {
        return new TerminalCharacters(CharList.of(v1, v2)) {
            @Override
            protected boolean isAccepted(char candidate) {
                return candidate == v1 || candidate == v2;
            }
        };
    }

    static StringReader createReader(String contents, int cursor) {
        StringReader reader = new StringReader(contents);
        reader.setCursor(cursor);
        return reader;
    }

    public static final class TerminalWord implements Term<StringReader> {
        private final String value;
        private final DelayedException<CommandSyntaxException> error;
        private final SuggestionSupplier<StringReader> suggestions;

        public TerminalWord(String value) {
            this.value = value;
            this.error = DelayedException.create(CommandSyntaxException.BUILT_IN_EXCEPTIONS.literalIncorrect(), value);
            this.suggestions = state -> Stream.of(value);
        }

        @Override
        public boolean parse(ParseState<StringReader> state, Scope scope, Control control) {
            state.input().skipWhitespace();
            int start = state.mark();
            String word = state.input().readUnquotedString();
            if (!word.equals(value)) {
                state.errorCollector().store(start, suggestions, error);
                return false;
            }
            return true;
        }

        @Override
        public String toString() {
            return "terminal[" + value + "]";
        }
    }

    public static abstract class TerminalCharacters implements Term<StringReader> {
        private final DelayedException<CommandSyntaxException> error;
        private final SuggestionSupplier<StringReader> suggestions;

        public TerminalCharacters(CharList values) {
            String joined = values.intStream().mapToObj(Character::toString).collect(Collectors.joining("|"));
            this.error = DelayedException.create(CommandSyntaxException.BUILT_IN_EXCEPTIONS.literalIncorrect(), joined);
            this.suggestions = state -> values.intStream().mapToObj(Character::toString);
        }

        @Override
        public boolean parse(ParseState<StringReader> state, Scope scope, Control control) {
            state.input().skipWhitespace();
            int start = state.mark();
            if (state.input().canRead() && isAccepted(state.input().read())) {
                return true;
            }
            state.errorCollector().store(start, suggestions, error);
            return false;
        }

        protected abstract boolean isAccepted(char value);
    }
}
