// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import java.util.Collection;
import java.util.UUID;
import net.minecraft.commands.CommandSourceStack;
public class UuidArgument implements ArgumentType<UUID> {
    public static final SimpleCommandExceptionType ERROR_INVALID_UUID = new SimpleCommandExceptionType(new com.mojang.brigadier.LiteralMessage("Invalid UUID"));
    public static UUID getUuid(CommandContext<CommandSourceStack> source, String name) {
        return source.getArgument(name, UUID.class);
    }
    public static UuidArgument uuid() {
        return new UuidArgument();
    }
    public UUID parse(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        while (reader.canRead() && (Character.digit(reader.peek(), 16) >= 0 || reader.peek() == '-')) {
            reader.skip();
        }
        String text = reader.getString().substring(start, reader.getCursor());
        try {
            return UUID.fromString(text);
        } catch (IllegalArgumentException e) {
            reader.setCursor(start);
            throw ERROR_INVALID_UUID.createWithContext(reader);
        }
    }
    public Collection<String> getExamples() {
        return java.util.List.of("dd12be42-52a9-4a91-a8a1-11c01849e498");
    }
}
