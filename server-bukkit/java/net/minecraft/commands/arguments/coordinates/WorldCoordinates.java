// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.coordinates;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;

public record WorldCoordinates(WorldCoordinate x, WorldCoordinate y, WorldCoordinate z) implements Coordinates {
    public static final WorldCoordinates ZERO_ROTATION = new WorldCoordinates(new WorldCoordinate(true, 0.0),
        new WorldCoordinate(true, 0.0), new WorldCoordinate(true, 0.0));

    public Vec3 getPosition(CommandSourceStack sender) {
        Vec3 pos = sender.getPosition();
        return new Vec3(x.get(pos.x), y.get(pos.y), z.get(pos.z));
    }

    public Vec2 getRotation(CommandSourceStack sender) {
        Vec2 rotation = sender.getRotation();
        return new Vec2((float) x.get(rotation.x), (float) y.get(rotation.y));
    }

    public boolean isXRelative() {
        return x.isRelative();
    }

    public boolean isYRelative() {
        return y.isRelative();
    }

    public boolean isZRelative() {
        return z.isRelative();
    }

    public static WorldCoordinates parseInt(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        WorldCoordinate x = WorldCoordinate.parseInt(reader);
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.setCursor(start);
            throw Vec3Argument.ERROR_NOT_COMPLETE.createWithContext(reader);
        }
        reader.skip();
        WorldCoordinate y = WorldCoordinate.parseInt(reader);
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.setCursor(start);
            throw Vec3Argument.ERROR_NOT_COMPLETE.createWithContext(reader);
        }
        reader.skip();
        return new WorldCoordinates(x, y, WorldCoordinate.parseInt(reader));
    }

    public static WorldCoordinates parseDouble(StringReader reader, boolean centerCorrect) throws CommandSyntaxException {
        int start = reader.getCursor();
        WorldCoordinate x = WorldCoordinate.parseDouble(reader, centerCorrect);
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.setCursor(start);
            throw Vec3Argument.ERROR_NOT_COMPLETE.createWithContext(reader);
        }
        reader.skip();
        WorldCoordinate y = WorldCoordinate.parseDouble(reader, false);
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.setCursor(start);
            throw Vec3Argument.ERROR_NOT_COMPLETE.createWithContext(reader);
        }
        reader.skip();
        return new WorldCoordinates(x, y, WorldCoordinate.parseDouble(reader, centerCorrect));
    }

    public static WorldCoordinates absolute(double x, double y, double z) {
        return new WorldCoordinates(new WorldCoordinate(false, x), new WorldCoordinate(false, y), new WorldCoordinate(false, z));
    }

    public static WorldCoordinates absolute(Vec2 rotation) {
        return new WorldCoordinates(new WorldCoordinate(false, rotation.x), new WorldCoordinate(false, rotation.y),
            new WorldCoordinate(true, 0.0));
    }
}
