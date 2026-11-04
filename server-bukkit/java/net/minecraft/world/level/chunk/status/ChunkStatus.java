// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.status;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import net.minecraft.world.level.levelgen.Heightmap;

public class ChunkStatus {
    public static final int MAX_STRUCTURE_DISTANCE = 8;
    private static final List<ChunkStatus> kRegistered = new ArrayList<>();
    private static final EnumSet<Heightmap.Types> kWorldgenHeightmaps = EnumSet.of(Heightmap.Types.OCEAN_FLOOR_WG,
        Heightmap.Types.WORLD_SURFACE_WG);
    public static final EnumSet<Heightmap.Types> FINAL_HEIGHTMAPS = EnumSet.of(Heightmap.Types.OCEAN_FLOOR,
        Heightmap.Types.WORLD_SURFACE, Heightmap.Types.MOTION_BLOCKING, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES);
    public static final ChunkStatus EMPTY = register("empty", null, kWorldgenHeightmaps, ChunkType.PROTOCHUNK);
    public static final ChunkStatus STRUCTURE_STARTS = register("structure_starts", EMPTY, kWorldgenHeightmaps,
        ChunkType.PROTOCHUNK);
    public static final ChunkStatus STRUCTURE_REFERENCES = register("structure_references", STRUCTURE_STARTS,
        kWorldgenHeightmaps, ChunkType.PROTOCHUNK);
    public static final ChunkStatus BIOMES = register("biomes", STRUCTURE_REFERENCES, kWorldgenHeightmaps,
        ChunkType.PROTOCHUNK);
    public static final ChunkStatus TERRAIN = register("terrain", BIOMES, FINAL_HEIGHTMAPS, ChunkType.PROTOCHUNK);
    public static final ChunkStatus FEATURES = register("features", TERRAIN, FINAL_HEIGHTMAPS, ChunkType.PROTOCHUNK);
    public static final ChunkStatus INITIALIZE_LIGHT = register("initialize_light", FEATURES, FINAL_HEIGHTMAPS,
        ChunkType.PROTOCHUNK);
    public static final ChunkStatus LIGHT = register("light", INITIALIZE_LIGHT, FINAL_HEIGHTMAPS, ChunkType.PROTOCHUNK);
    public static final ChunkStatus SPAWN = register("spawn", LIGHT, FINAL_HEIGHTMAPS, ChunkType.PROTOCHUNK);
    public static final ChunkStatus FULL = register("full", SPAWN, FINAL_HEIGHTMAPS, ChunkType.LEVELCHUNK);
    public static final Codec<ChunkStatus> CODEC = Codec.STRING.xmap(ChunkStatus::byName, ChunkStatus::getName);

    private final String name;
    private final int index;
    private final ChunkStatus parent;
    private final ChunkType chunkType;
    private final EnumSet<Heightmap.Types> heightmapsAfter;

    private static ChunkStatus register(String name, ChunkStatus parent, EnumSet<Heightmap.Types> heightmaps,
            ChunkType type) {
        ChunkStatus status = new ChunkStatus(parent, heightmaps, type, name);
        kRegistered.add(status);
        return status;
    }

    protected ChunkStatus() {
        this(null, EnumSet.noneOf(Heightmap.Types.class), ChunkType.PROTOCHUNK, "empty");
    }

    protected ChunkStatus(ChunkStatus parent, EnumSet<Heightmap.Types> heightmapsAfter, ChunkType chunkType) {
        this(parent, heightmapsAfter, chunkType, "custom_" + System.identityHashCode(heightmapsAfter));
    }

    private ChunkStatus(ChunkStatus parent, EnumSet<Heightmap.Types> heightmapsAfter, ChunkType chunkType,
            String name) {
        this.name = name;
        this.parent = parent;
        this.index = parent == null ? 0 : parent.index + 1;
        this.chunkType = chunkType;
        this.heightmapsAfter = heightmapsAfter;
    }

    public static List<ChunkStatus> getStatusList() {
        return List.copyOf(kRegistered);
    }

    public int getIndex() {
        return index;
    }

    public ChunkStatus getParent() {
        return parent == null ? this : parent;
    }

    public ChunkType getChunkType() {
        return chunkType;
    }

    public static ChunkStatus byName(String key) {
        String bare = key.startsWith("minecraft:") ? key.substring(10) : key;
        for (ChunkStatus status : kRegistered) {
            if (status.name.equals(bare)) {
                return status;
            }
        }
        return EMPTY;
    }

    public EnumSet<Heightmap.Types> heightmapsAfter() {
        return heightmapsAfter;
    }

    public boolean isOrAfter(ChunkStatus step) {
        return index >= step.index;
    }

    public boolean isAfter(ChunkStatus step) {
        return index > step.index;
    }

    public boolean isOrBefore(ChunkStatus step) {
        return index <= step.index;
    }

    public boolean isBefore(ChunkStatus step) {
        return index < step.index;
    }

    public static ChunkStatus max(ChunkStatus a, ChunkStatus b) {
        return a.isAfter(b) ? a : b;
    }

    @Override
    public String toString() {
        return name;
    }

    public String getName() {
        return name;
    }
}
