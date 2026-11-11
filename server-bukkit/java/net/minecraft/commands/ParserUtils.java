// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import com.mojang.brigadier.StringReader;
import net.minecraft.CharPredicate;
public class ParserUtils {
    public static String readWhile(StringReader reader, CharPredicate predicate) {
        int from = reader.getCursor();
        while (reader.canRead() && predicate.test(reader.peek())) {
            reader.skip();
        }
        return reader.getString().substring(from, reader.getCursor());
    }
}
