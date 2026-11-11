// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import com.mojang.serialization.Codec;
import net.minecraft.core.Direction;
import net.minecraft.util.StringRepresentable;
public record CardinalLighting(float down, float up, float north, float south, float west, float east) {
    public static final CardinalLighting DEFAULT = new CardinalLighting(0.5F, 1.0F, 0.8F, 0.8F, 0.6F, 0.6F);
    public static final CardinalLighting NETHER = new CardinalLighting(0.9F, 0.9F, 0.8F, 0.8F, 0.6F, 0.6F);
    public float byFace(Direction direction) {
        return switch (direction) {
            case DOWN -> down;
            case UP -> up;
            case NORTH -> north;
            case SOUTH -> south;
            case WEST -> west;
            case EAST -> east;
        };
    }
    public static enum Type implements StringRepresentable {
        DEFAULT("default", CardinalLighting.DEFAULT), NETHER("nether", CardinalLighting.NETHER);
        public static final Codec<Type> CODEC = StringRepresentable.fromEnum(Type::values);
        private final String serialized;
        private final CardinalLighting lighting;
        Type(String serialized, CardinalLighting lighting) {
            this.serialized = serialized;
            this.lighting = lighting;
        }
        public CardinalLighting get() {
            return lighting;
        }
        @Override
        public String getSerializedName() {
            return serialized;
        }
    }
}
