// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.Collection;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.resources.Identifier;
public class IdentifierArgument implements ArgumentType<Identifier> {
    public static IdentifierArgument id() {
        return new IdentifierArgument();
    }
    public static Identifier getId(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Identifier.class);
    }
    public Identifier parse(StringReader reader) throws CommandSyntaxException {
        return Identifier.read(reader);
    }
    public Collection<String> getExamples() {
        return java.util.List.of("foo", "foo:bar", "012");
    }
}
