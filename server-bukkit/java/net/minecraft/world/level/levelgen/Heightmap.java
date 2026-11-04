// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.SimpleBitStorage;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.LevelChunk;
import org.basaltmc.bukkit.WorldNatives;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.state.BlockState;

/** Heights read from the world on first use; a plugin that writes raw data only replaces this copy. */
public class Heightmap {
    private final LevelChunk chunk;
    private final ServerLevel level;
    private final Types type;
    private SimpleBitStorage data;

    protected Heightmap() {
        this.chunk = null;
        this.level = null;
        this.type = null;
    }

    public Heightmap(LevelChunk chunk, ServerLevel level, Types type) {
        this.chunk = chunk;
        this.level = level;
        this.type = type;
    }

    private synchronized SimpleBitStorage storage() {
        if (data == null) {
            int bits = net.minecraft.util.Mth.ceillog2(level.getHeight() + 1);
            SimpleBitStorage made = new SimpleBitStorage(bits, 256);
            String kind = switch (type) {
                case WORLD_SURFACE_WG, WORLD_SURFACE -> "surface";
                case OCEAN_FLOOR_WG, OCEAN_FLOOR -> "ocean_floor";
                default -> "motion_blocking";
            };
            String world = level.getWorld().getName();
            int minY = level.getMinY();
            for (int z = 0; z < 16; z++) {
                for (int x = 0; x < 16; x++) {
                    int top = WorldNatives.highest(world, (chunk.locX << 4) + x, (chunk.locZ << 4) + z, kind, false);
                    made.set(z << 4 | x, Math.max(0, top + 1 - minY));
                }
            }
            data = made;
        }
        return data;
    }

    public long[] getRawData() {
        return storage().getRaw();
    }

    public void setRawData(ChunkAccess chunk, Heightmap.Types type, long[] raw) {
        int bits = net.minecraft.util.Mth.ceillog2(level.getHeight() + 1);
        synchronized (this) {
            data = new SimpleBitStorage(bits, 256, raw.clone());
        }
    }

    public enum Types implements StringRepresentable {
        WORLD_SURFACE_WG, WORLD_SURFACE, OCEAN_FLOOR_WG, OCEAN_FLOOR, MOTION_BLOCKING, MOTION_BLOCKING_NO_LEAVES;
    ;
        public static final Codec<Types> CODEC = StringRepresentable.fromEnum(Types::values);
        public static final StreamCodec<ByteBuf, Types> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.idMapper(i -> values()[i], Types::ordinal);
        public String getSerializationKey() {
            return name();
        }
        public boolean sendToClient() {
            return this == WORLD_SURFACE || this == MOTION_BLOCKING || this == MOTION_BLOCKING_NO_LEAVES;
        }
        public boolean keepAfterWorldgen() {
            return this != WORLD_SURFACE_WG && this != OCEAN_FLOOR_WG;
        }
        public Predicate<BlockState> isOpaque() {
            return switch (this) {
                case WORLD_SURFACE_WG, WORLD_SURFACE -> state -> !state.isAir();
                case OCEAN_FLOOR_WG, OCEAN_FLOOR -> BlockState::isSolid;
                case MOTION_BLOCKING -> state -> state.isSolid() || !state.getFluidState().isEmpty();
                case MOTION_BLOCKING_NO_LEAVES -> state -> state.isSolid() && !state.getBlock().blockKey().getPath().endsWith("_leaves")
                    || !state.getFluidState().isEmpty();
            };
        }
        public String getSerializedName() {
            return name();
        }
}
    public Heightmap(ChunkAccess chunk, Types heightmapType) {
        throw new UnsupportedOperationException("Heightmaps are bound to a level chunk");
    }
    public static void primeHeightmaps(ChunkAccess chunk, Set<Types> types) {

    }
    public boolean update(int localX, int localY, int localZ, BlockState state) {
        SimpleBitStorage held = storage();
        int index = localX | localZ << 4;
        int first = held.get(index) + level.getMinY();
        if (localY <= first - 2) {
            return false;
        }
        if (type.isOpaque().test(state)) {
            if (localY >= first) {
                held.set(index, localY + 1 - level.getMinY());
                return true;
            }
        } else if (first - 1 == localY) {
            BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
            for (int y = localY - 1; y >= level.getMinY(); y--) {
                pos.set((chunk.locX << 4) + localX, y, (chunk.locZ << 4) + localZ);
                if (type.isOpaque().test(chunk.getBlockState(pos))) {
                    held.set(index, y + 1 - level.getMinY());
                    return true;
                }
            }
            held.set(index, 0);
            return true;
        }
        return false;
    }
    public int getFirstAvailable(int x, int z) {
        return storage().get(x | z << 4) + level.getMinY();
    }
    public int getHighestTaken(int x, int z) {
        return getFirstAvailable(x, z) - 1;
    }
    public static enum Usage {
        WORLDGEN, LIVE_WORLD, CLIENT;
    }
}
