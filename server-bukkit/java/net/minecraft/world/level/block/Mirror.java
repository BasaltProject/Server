// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import com.mojang.math.OctahedralGroup;
import com.mojang.serialization.Codec;
import net.minecraft.core.Direction;
import net.minecraft.network.chat.Component;
import net.minecraft.util.StringRepresentable;
public enum Mirror implements StringRepresentable {
    NONE, LEFT_RIGHT, FRONT_BACK;
    public static final Codec<Mirror> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.Mirror.class, "CODEC");
    public static final Codec<Mirror> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.Mirror.class, "LEGACY_CODEC");
    public int mirror(int rotation, int steps) {
        int half = steps / 2;
        int signed = rotation > half ? rotation - steps : rotation;
        return switch (this) {
            case LEFT_RIGHT -> (half - signed + steps) % steps;
            case FRONT_BACK -> (steps - signed) % steps;
            default -> rotation;
        };
    }
    public Rotation getRotation(Direction value) {
        Direction.Axis axis = value.getAxis();
        boolean flips = this == LEFT_RIGHT && axis == Direction.Axis.Z || this == FRONT_BACK && axis == Direction.Axis.X;
        return flips ? Rotation.CLOCKWISE_180 : Rotation.NONE;
    }
    public Direction mirror(Direction direction) {
        boolean flips = this == FRONT_BACK && direction.getAxis() == Direction.Axis.X
            || this == LEFT_RIGHT && direction.getAxis() == Direction.Axis.Z;
        return flips ? direction.getOpposite() : direction;
    }
    public OctahedralGroup rotation() {
        return switch (this) {
            case NONE -> OctahedralGroup.IDENTITY;
            case LEFT_RIGHT -> OctahedralGroup.INVERT_Z;
            case FRONT_BACK -> OctahedralGroup.INVERT_X;
        };
    }
    public Component symbol() {
        return Component.translatable("mirror." + getSerializedName());


    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
