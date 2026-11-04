// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.util.Objects;
import java.util.function.Function;

public class CompilableString<T> {
    private final String source;
    private final T compiled;

    private CompilableString(String source, T compiled) {
        this.source = source;
        this.compiled = compiled;
    }

    public static <T> Codec<CompilableString<T>> codec(Function<String, DataResult<T>> compiler) {
        return Codec.STRING.comapFlatMap(text -> compiler.apply(text).map(value -> new CompilableString<>(text, value)),
            CompilableString::source);
    }

    public String source() {
        return source;
    }

    public T compiled() {
        return compiled;
    }

    @Override
    public boolean equals(Object o) {
        return this == o || o instanceof CompilableString<?> other && source.equals(other.source);
    }

    @Override
    public int hashCode() {
        return Objects.hashCode(source);
    }

    @Override
    public String toString() {
        return source;
    }

    public abstract static class CommandParserHelper<T> implements Function<String, DataResult<T>> {
        @Override
        public final DataResult<T> apply(String contents) {
            StringReader reader = new StringReader(contents);
            try {
                T result = parse(reader);
                if (reader.canRead()) {
                    throw new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(
                        new com.mojang.brigadier.LiteralMessage("Unexpected trailing data")).createWithContext(reader);
                }
                return DataResult.success(result);
            } catch (CommandSyntaxException e) {
                return DataResult.error(() -> errorMessage(contents, e));
            }
        }

        protected abstract T parse(StringReader reader) throws CommandSyntaxException;

        protected abstract String errorMessage(String original, CommandSyntaxException exception);
    }
}
