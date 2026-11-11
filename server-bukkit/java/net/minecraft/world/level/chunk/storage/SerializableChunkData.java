// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import net.minecraft.nbt.ListTag;
import it.unimi.dsi.fastutil.shorts.ShortList;
import java.util.List;
import java.util.Map;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtException;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.world.level.chunk.LevelChunkSection;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import net.minecraft.world.level.chunk.ProtoChunk;
import net.minecraft.world.level.chunk.UpgradeData;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.levelgen.BelowZeroRetrogen;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.blending.BlendingData;
public record SerializableChunkData(PalettedContainerFactory containerFactory, ChunkPos chunkPos, int minSectionY, long lastUpdateTime, long inhabitedTime, ChunkStatus chunkStatus, BlendingData. Packed blendingData, BelowZeroRetrogen belowZeroRetrogen, UpgradeData upgradeData, Map<Heightmap.Types, long[]> heightmaps, ChunkAccess.PackedTicks packedTicks, ShortList[] postProcessingSections, boolean lightCorrect, List<SectionData> sectionData, List<CompoundTag> entities, List<CompoundTag> blockEntities, CompoundTag structureData) {
    public static final String X_POS_TAG = "xPos";
    public static final String Z_POS_TAG = "zPos";
    public static final String HEIGHTMAPS_TAG = "Heightmaps";
    public static final String IS_LIGHT_ON_TAG = "isLightOn";
    public static final String SECTIONS_TAG = "sections";
    public static final String BLOCK_LIGHT_TAG = "BlockLight";
    public static final String SKY_LIGHT_TAG = "SkyLight";
    public static SerializableChunkData parse(LevelHeightAccessor levelHeight, PalettedContainerFactory containerFactory, CompoundTag chunkData) {
        ChunkPos pos = new ChunkPos(chunkData.getIntOr("xPos", 0), chunkData.getIntOr("zPos", 0));
        return new SerializableChunkData(containerFactory, pos, levelHeight.getMinSectionY(), chunkData.getLongOr("LastUpdate", 0L),
            chunkData.getLongOr("InhabitedTime", 0L), getChunkStatusFromTag(chunkData), null, null, UpgradeData.EMPTY, new java.util.EnumMap<>(Heightmap.Types.class),
            new ChunkAccess.PackedTicks(List.of(), List.of()), new ShortList[levelHeight.getSectionsCount()], chunkData.getBooleanOr("isLightOn", false),
            List.of(), List.of(), chunkData.getListOrEmpty("block_entities").stream().filter(CompoundTag.class::isInstance).map(CompoundTag.class::cast).toList(),
            chunkData.getCompoundOrEmpty("structures"));
    }
    public ProtoChunk read(ServerLevel level, PoiManager poiManager, RegionStorageInfo regionInfo, ChunkPos pos) {
        if (!pos.equals(chunkPos)) {
            throw new ChunkReadException("Chunk file at " + pos + " is in the wrong location. (Expected " + pos + ", got " + chunkPos + ")");
        }
        ProtoChunk proto = new ProtoChunk(pos, upgradeData, level, containerFactory, null);
        proto.setPersistedStatus(chunkStatus);
        proto.setLightCorrect(lightCorrect);
        for (CompoundTag tile : blockEntities) {
            proto.setBlockEntityNbt(tile);
        }
        return proto;
    }
    public static SerializableChunkData copyOf(ServerLevel level, ChunkAccess chunk) {
        ChunkPos pos = chunk.getPos();
        return new SerializableChunkData(level.palettedContainerFactory(), pos, chunk.getMinSectionY(), level.getGameTime(), chunk.getInhabitedTime(),
            chunk.getPersistedStatus(), null, null, UpgradeData.EMPTY, new java.util.EnumMap<>(Heightmap.Types.class), chunk.getTicksForSerialization(level.getGameTime()),
            new ShortList[chunk.getSectionsCount()], chunk.isLightCorrect(), List.of(), List.of(), List.of(), new CompoundTag());
    }
    public CompoundTag write() {
        CompoundTag out = new CompoundTag();
        out.putInt("xPos", chunkPos.x());
        out.putInt("zPos", chunkPos.z());
        out.putInt("yPos", minSectionY);
        out.putLong("LastUpdate", lastUpdateTime);
        out.putLong("InhabitedTime", inhabitedTime);
        out.putString("Status", chunkStatus.getName());
        out.putBoolean("isLightOn", lightCorrect);
        ListTag tiles = new ListTag();
        tiles.addAll(blockEntities);
        out.put("block_entities", tiles);
        out.put("structures", structureData);
        return out;
    }
    public static ChunkStatus getChunkStatusFromTag(CompoundTag tag) {
        String name = tag.getStringOr("Status", "minecraft:empty");
        return ChunkStatus.byName(name);


    }
    public static record SectionData(int y, LevelChunkSection chunkSection, DataLayer blockLight, DataLayer skyLight) {
    }
    public static class ChunkReadException extends NbtException {
        public ChunkReadException(String message) {
            super(message);
        }
    }
}
