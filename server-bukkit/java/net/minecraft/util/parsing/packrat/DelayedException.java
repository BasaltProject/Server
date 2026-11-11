// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;

public interface DelayedException<T extends Exception> {
    T create(String contents, int position);

    static DelayedException<CommandSyntaxException> create(SimpleCommandExceptionType type) {
        return (contents, position) -> type.createWithContext(reader(contents, position));
    }

    static DelayedException<CommandSyntaxException> create(DynamicCommandExceptionType type, String argument) {
        return (contents, position) -> type.createWithContext(reader(contents, position), argument);
    }

    private static StringReader reader(String contents, int position) {
        StringReader reader = new StringReader(contents);
        reader.setCursor(position);
        return reader;
    }
}
