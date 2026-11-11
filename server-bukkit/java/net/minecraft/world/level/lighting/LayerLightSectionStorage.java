// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
import net.minecraft.core.SectionPos;
import net.minecraft.core.BlockPos;
import it.unimi.dsi.fastutil.longs.Long2ByteMap;
import it.unimi.dsi.fastutil.longs.Long2ObjectMap;
import it.unimi.dsi.fastutil.longs.LongSet;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.world.level.chunk.LightChunkGetter;
public abstract class LayerLightSectionStorage<M extends DataLayerStorageMap<M>> {
    protected final LightChunkGetter chunkSource;
    private final LightLayer layer;
    private final it.unimi.dsi.fastutil.longs.LongSet lightOnColumns = new it.unimi.dsi.fastutil.longs.LongOpenHashSet();
    protected final Long2ByteMap sectionStates = new it.unimi.dsi.fastutil.longs.Long2ByteOpenHashMap();
    protected M visibleSectionData;
    protected final M updatingSectionData;
    protected final LongSet changedSections = new it.unimi.dsi.fastutil.longs.LongOpenHashSet();
    protected final LongSet sectionsAffectedByLightUpdates = new it.unimi.dsi.fastutil.longs.LongOpenHashSet();
    protected final Long2ObjectMap<DataLayer> queuedSections = new it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap<>();
    protected boolean hasInconsistencies;
    protected LayerLightSectionStorage(LightLayer layer, LightChunkGetter chunkSource, M initialMap) {
        this.layer = layer;
        this.chunkSource = chunkSource;
        this.updatingSectionData = initialMap;
        this.visibleSectionData = initialMap.copy();
        this.sectionStates.defaultReturnValue((byte) 0);
    }
    protected boolean storingLightForSection(long sectionNode) {
        return updatingSectionData.hasLayer(sectionNode);
    }
    protected DataLayer getDataLayer(long sectionNode, boolean updating) {
        return getDataLayer(updating ? updatingSectionData : visibleSectionData, sectionNode);
    }
    protected DataLayer getDataLayer(M sections, long sectionNode) {
        return sections.getLayer(sectionNode);
    }
    protected DataLayer getDataLayerToWrite(long sectionNode) {
        DataLayer current = updatingSectionData.getLayer(sectionNode);
        if (current == null) {
            return null;
        }
        if (changedSections.add(sectionNode)) {
            current = updatingSectionData.copyDataLayer(sectionNode);
        }
        markSectionAndNeighborsAsAffected(sectionNode);
        return current;
    }
    public DataLayer getDataLayerData(long sectionNode) {
        DataLayer queued = queuedSections.get(sectionNode);
        return queued != null ? queued : visibleSectionData.getLayer(sectionNode);
    }
    protected abstract int getLightValue(long blockNode);
    protected int getStoredLevel(long blockNode) {
        DataLayer data = getDataLayer(SectionPos.blockToSection(blockNode), true);
        return data == null ? 0 : data.get(SectionPos.sectionRelative(BlockPos.getX(blockNode)),
            SectionPos.sectionRelative(BlockPos.getY(blockNode)), SectionPos.sectionRelative(BlockPos.getZ(blockNode)));
    }
    protected void setStoredLevel(long blockNode, int level) {
        DataLayer data = getDataLayerToWrite(SectionPos.blockToSection(blockNode));
        if (data != null) {
            data.set(SectionPos.sectionRelative(BlockPos.getX(blockNode)),
                SectionPos.sectionRelative(BlockPos.getY(blockNode)), SectionPos.sectionRelative(BlockPos.getZ(blockNode)), level);
        }
    }
    protected void markSectionAndNeighborsAsAffected(long sectionNode) {
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    sectionsAffectedByLightUpdates.add(SectionPos.asLong(SectionPos.x(sectionNode) + dx,
                        SectionPos.y(sectionNode) + dy, SectionPos.z(sectionNode) + dz));
                }
            }
        }
    }
    protected DataLayer createDataLayer(long sectionNode) {
        DataLayer queued = queuedSections.get(sectionNode);
        return queued != null ? queued : new DataLayer();
    }
    protected boolean hasInconsistencies() {
        return hasInconsistencies;
    }
    protected void markNewInconsistencies(LightEngine<M, ?> engine) {
        this.hasInconsistencies = false;
    }
    protected void onNodeAdded(long sectionNode) {
    }
    protected void onNodeRemoved(long sectionNode) {
    }
    protected void setLightEnabled(long zeroNode, boolean enable) {
        if (enable) {
            lightOnColumns.add(zeroNode);
        } else {
            lightOnColumns.remove(zeroNode);
        }
    }
    protected boolean lightOnInSection(long sectionNode) {
        return lightOnColumns.contains(SectionPos.getZeroNode(sectionNode));
    }
    protected boolean lightOnInColumn(long sectionZeroNode) {
        return lightOnColumns.contains(sectionZeroNode);
    }
    public void retainData(long zeroNode, boolean retain) {
    }
    protected void queueSectionData(long sectionNode, DataLayer data) {
        queuedSections.put(sectionNode, data);
    }
    protected void updateSectionStatus(long sectionNode, boolean sectionEmpty) {
        byte state = sectionStates.get(sectionNode);
        putSectionState(sectionNode, SectionState.hasData(state, !sectionEmpty));
        if (sectionEmpty) {
            updatingSectionData.removeLayer(sectionNode);
            onNodeRemoved(sectionNode);
        } else if (!updatingSectionData.hasLayer(sectionNode)) {
            updatingSectionData.setLayer(sectionNode, createDataLayer(sectionNode));
            onNodeAdded(sectionNode);
        }
    }
    protected void putSectionState(long sectionNode, byte state) {
        if (state == SectionState.EMPTY) {
            sectionStates.remove(sectionNode);
        } else {
            sectionStates.put(sectionNode, state);
        }
    }
    protected void swapSectionMap() {
        if (!changedSections.isEmpty()) {
            this.visibleSectionData = updatingSectionData.copy();
            changedSections.clear();
        }
        queuedSections.clear();
    }
    public SectionType getDebugSectionType(long sectionNode) {
        return SectionState.type(sectionStates.get(sectionNode));
    }
    protected static class SectionState {
        public static final byte EMPTY = 0;
        public static byte hasData(byte state, boolean hasData) {
            return (byte) (hasData ? (state | 1) : (state & ~1));
        }
        public static byte neighborCount(byte state, int neighborCount) {
            return (byte) ((state & 1) | (neighborCount << 1));
        }
        public static boolean hasData(byte state) {
            return (state & 1) != 0;
        }
        public static int neighborCount(byte state) {
            return (state >> 1) & 31;
        }
        public static SectionType type(byte state) {
            if (hasData(state)) {
                return SectionType.LIGHT_AND_DATA;
            }
            return neighborCount(state) > 0 ? SectionType.LIGHT_ONLY : SectionType.EMPTY;
        }
    }
    public static enum SectionType {
        EMPTY("2"), LIGHT_ONLY("1"), LIGHT_AND_DATA("0");
        private final String display;
        private SectionType(String display) {
            this.display = display;
        }
        public String display() {
            return display;
        }
    }
}
