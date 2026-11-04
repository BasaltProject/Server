// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.coordinates;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import net.minecraft.network.chat.Component;

public record WorldCoordinate(boolean relative, double value) {
    public static final SimpleCommandExceptionType ERROR_EXPECTED_DOUBLE =
        new SimpleCommandExceptionType(Component.translatable("argument.pos.missing.double"));
    public static final SimpleCommandExceptionType ERROR_EXPECTED_INT =
        new SimpleCommandExceptionType(Component.translatable("argument.pos.missing.int"));

    public double get(double original) {
        return relative ? value + original : value;
    }

    private static void requireCoordinate(StringReader reader, com.mojang.brigadier.exceptions.SimpleCommandExceptionType missing)
            throws CommandSyntaxException {
        boolean canRead = reader.canRead();
        if (canRead && reader.peek() == '^') {
            throw Vec3Argument.ERROR_MIXED_TYPE.createWithContext(reader);
        }
        if (!canRead) {
            throw missing.createWithContext(reader);
        }
    }

    public static WorldCoordinate parseDouble(StringReader reader, boolean center) throws CommandSyntaxException {
        requireCoordinate(reader, ERROR_EXPECTED_DOUBLE);
        boolean tilde = isRelative(reader);
        int from = reader.getCursor();
        boolean hasNumber = reader.canRead() && reader.peek() != ' ';
        double number = hasNumber ? reader.readDouble() : 0.0;
        String text = reader.getString().substring(from, reader.getCursor());
        if (tilde && text.isEmpty()) {
            return new WorldCoordinate(true, 0.0);
        }
        boolean snapToCentre = center && !tilde && !text.contains(".");
        return new WorldCoordinate(tilde, snapToCentre ? number + 0.5 : number);
    }

    public static WorldCoordinate parseInt(StringReader reader) throws CommandSyntaxException {
        requireCoordinate(reader, ERROR_EXPECTED_INT);
        boolean tilde = isRelative(reader);
        if (!reader.canRead() || reader.peek() == ' ') {
            return new WorldCoordinate(tilde, 0.0);
        }
        return new WorldCoordinate(tilde, tilde ? reader.readDouble() : reader.readInt());
    }

    public static boolean isRelative(StringReader reader) {
        boolean relative = reader.peek() == '~';
        if (relative) {
            reader.skip();
        }
        return relative;
    }

    public boolean isRelative() {
        return relative;
    }
}
