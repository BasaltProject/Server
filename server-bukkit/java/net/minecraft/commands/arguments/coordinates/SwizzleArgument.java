// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.coordinates;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.Collection;
import java.util.EnumSet;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.Direction;
public class SwizzleArgument implements ArgumentType<EnumSet<Direction.Axis>> {
    private static final com.mojang.brigadier.exceptions.SimpleCommandExceptionType kInvalid =
        new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(Component.translatable("arguments.swizzle.invalid"));

    public static SwizzleArgument swizzle() {
        return new SwizzleArgument();
    }
    public static EnumSet<Direction.Axis> getSwizzle(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, EnumSet.class);
    }
    public EnumSet<Direction.Axis> parse(StringReader reader) throws CommandSyntaxException {
        EnumSet<Direction.Axis> axes = EnumSet.noneOf(Direction.Axis.class);
        while (reader.canRead() && reader.peek() != ' ') {
            char c = reader.read();
            Direction.Axis axis = switch (c) {
                case 'x' -> Direction.Axis.X;
                case 'y' -> Direction.Axis.Y;
                case 'z' -> Direction.Axis.Z;
                default -> throw kInvalid.createWithContext(reader);
            };
            if (!axes.add(axis)) {
                throw kInvalid.createWithContext(reader);
            }
        }
        return axes;
    }
    public Collection<String> getExamples() {
        return java.util.List.of("xyz", "x");
    }
}
