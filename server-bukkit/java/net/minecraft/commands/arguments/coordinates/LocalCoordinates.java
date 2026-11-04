// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.coordinates;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.Objects;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.util.Mth;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;

public record LocalCoordinates(double left, double up, double forwards) implements Coordinates {
    public static final char PREFIX_LOCAL_COORDINATE = '^';

    public Vec3 getPosition(CommandSourceStack sender) {
        return apply(sender.getAnchor().apply(sender), sender.getRotation());
    }

    public Vec3 apply(Vec3 source, Vec2 rotation) {
        float yawCos = Mth.cos((rotation.y + 90.0F) * (float) (Math.PI / 180.0));
        float yawSin = Mth.sin((rotation.y + 90.0F) * (float) (Math.PI / 180.0));
        float pitchCos = Mth.cos(-rotation.x * (float) (Math.PI / 180.0));
        float pitchSin = Mth.sin(-rotation.x * (float) (Math.PI / 180.0));
        float pitchCosUp = Mth.cos((-rotation.x + 90.0F) * (float) (Math.PI / 180.0));
        float pitchSinUp = Mth.sin((-rotation.x + 90.0F) * (float) (Math.PI / 180.0));
        Vec3 forwardAxis = new Vec3(yawCos * pitchCos, pitchSin, yawSin * pitchCos);
        Vec3 upAxis = new Vec3(yawCos * pitchCosUp, pitchSinUp, yawSin * pitchCosUp);
        Vec3 leftAxis = forwardAxis.cross(upAxis).scale(-1.0);
        double x = forwardAxis.x * forwards + upAxis.x * up + leftAxis.x * left;
        double y = forwardAxis.y * forwards + upAxis.y * up + leftAxis.y * left;
        double z = forwardAxis.z * forwards + upAxis.z * up + leftAxis.z * left;
        return new Vec3(source.x + x, source.y + y, source.z + z);
    }

    public Vec2 getRotation(CommandSourceStack sender) {
        return Vec2.ZERO;
    }

    public boolean isXRelative() {
        return true;
    }

    public boolean isYRelative() {
        return true;
    }

    public boolean isZRelative() {
        return true;
    }

    public static LocalCoordinates parse(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        double left = readDouble(reader, start);
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.setCursor(start);
            throw Vec3Argument.ERROR_NOT_COMPLETE.createWithContext(reader);
        }
        reader.skip();
        double up = readDouble(reader, start);
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.setCursor(start);
            throw Vec3Argument.ERROR_NOT_COMPLETE.createWithContext(reader);
        }
        reader.skip();
        return new LocalCoordinates(left, up, readDouble(reader, start));
    }

    private static double readDouble(StringReader reader, int start) throws CommandSyntaxException {
        if (!reader.canRead()) {
            throw WorldCoordinate.ERROR_EXPECTED_DOUBLE.createWithContext(reader);
        }
        if (reader.peek() != PREFIX_LOCAL_COORDINATE) {
            reader.setCursor(start);
            throw Vec3Argument.ERROR_MIXED_TYPE.createWithContext(reader);
        }
        reader.skip();
        return reader.canRead() && reader.peek() != ' ' ? reader.readDouble() : 0.0;
    }

    public boolean equals(Object other) {
        return other instanceof LocalCoordinates local && left == local.left && up == local.up && forwards == local.forwards;
    }

    public int hashCode() {
        return Objects.hash(left, up, forwards);
    }
}
