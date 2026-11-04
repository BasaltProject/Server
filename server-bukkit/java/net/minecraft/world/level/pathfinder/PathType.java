// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
public enum PathType {
    BLOCKED(-1.0F), OPEN(0.0F), WALKABLE(0.0F), WALKABLE_DOOR(0.0F), TRAPDOOR(0.0F), POWDER_SNOW(-1.0F), ON_TOP_OF_POWDER_SNOW(0.0F),
    FENCE(-1.0F), LAVA(-1.0F), WATER(8.0F), WATER_BORDER(8.0F), RAIL(0.0F), UNPASSABLE_RAIL(-1.0F), FIRE_IN_NEIGHBOR(8.0F),
    FIRE(16.0F), DAMAGING_IN_NEIGHBOR(8.0F), DAMAGING(-1.0F), DOOR_OPEN(0.0F), DOOR_WOOD_CLOSED(-1.0F), DOOR_IRON_CLOSED(-1.0F),
    BREACH(4.0F), LEAVES(-1.0F), STICKY_HONEY(8.0F), COCOA(0.0F), DAMAGE_CAUTIOUS(0.0F), ON_TOP_OF_TRAPDOOR(0.0F),
    BIG_MOBS_CLOSE_TO_DANGER(4.0F);
    public static final StreamCodec<ByteBuf, PathType> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.pathfinder.PathType.class, "STREAM_CODEC");
    private final float malus;
    PathType(float malus) {
        this.malus = malus;
    }
    public float getMalus() {
        return malus;
    }
}
