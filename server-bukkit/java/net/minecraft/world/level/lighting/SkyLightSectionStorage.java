// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
import net.minecraft.core.SectionPos;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LightLayer;
import it.unimi.dsi.fastutil.longs.Long2IntOpenHashMap;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.world.level.chunk.LightChunkGetter;
public class SkyLightSectionStorage extends LayerLightSectionStorage<SkyLightSectionStorage.SkyDataLayerStorageMap> {
    protected SkyLightSectionStorage(LightChunkGetter chunkSource) {
        super(LightLayer.SKY, chunkSource, new SkyDataLayerStorageMap(new Long2ObjectOpenHashMap<>(), new Long2IntOpenHashMap(), Integer.MAX_VALUE));
    }
    protected int getLightValue(long blockNode) {
        return getLightValue(blockNode, false);
    }
    protected int getLightValue(long blockNode, boolean updating) {
        long sectionNode = SectionPos.blockToSection(blockNode);
        int sectionY = SectionPos.y(sectionNode);
        SkyDataLayerStorageMap sections = updating ? updatingSectionData : visibleSectionData;
        int top = sections.topSections.get(SectionPos.getZeroNode(sectionNode));
        if (top == sections.currentLowestY || sectionY >= top) {
            return updating && !lightOnInSection(sectionNode) ? 0 : 15;
        }
        DataLayer data = getDataLayer(sections, sectionNode);
        while (data == null) {
            sectionY++;
            if (sectionY >= top) {
                return 15;
            }
            sectionNode = SectionPos.offset(sectionNode, Direction.UP);
            data = getDataLayer(sections, sectionNode);
        }
        long flat = BlockPos.getFlatIndex(blockNode);
        return data.get(SectionPos.sectionRelative(BlockPos.getX(flat)), SectionPos.sectionRelative(BlockPos.getY(blockNode)),
            SectionPos.sectionRelative(BlockPos.getZ(flat)));
    }
    protected void onNodeAdded(long sectionNode) {
        int y = SectionPos.y(sectionNode);
        if (updatingSectionData.currentLowestY > y) {
            updatingSectionData.currentLowestY = y;
            updatingSectionData.topSections.defaultReturnValue(y);
        }
        long zeroNode = SectionPos.getZeroNode(sectionNode);
        if (updatingSectionData.topSections.get(zeroNode) < y + 1) {
            updatingSectionData.topSections.put(zeroNode, y + 1);
        }
    }
    protected void onNodeRemoved(long sectionNode) {
        long zeroNode = SectionPos.getZeroNode(sectionNode);
        int y = SectionPos.y(sectionNode);
        if (updatingSectionData.topSections.get(zeroNode) != y + 1) {
            return;
        }
        long probe = sectionNode;
        while (!storingLightForSection(probe) && hasLightDataAtOrBelow(y)) {
            y--;
            probe = SectionPos.offset(probe, Direction.DOWN);
        }
        if (storingLightForSection(probe)) {
            updatingSectionData.topSections.put(zeroNode, y + 1);
        } else {
            updatingSectionData.topSections.remove(zeroNode);
        }
    }
    protected DataLayer createDataLayer(long sectionNode) {
        DataLayer queued = queuedSections.get(sectionNode);
        if (queued != null) {
            return queued;
        }
        int top = updatingSectionData.topSections.get(SectionPos.getZeroNode(sectionNode));
        if (top == updatingSectionData.currentLowestY || SectionPos.y(sectionNode) >= top) {
            return lightOnInSection(sectionNode) ? new DataLayer(15) : new DataLayer();
        }
        long above = SectionPos.offset(sectionNode, Direction.UP);
        DataLayer found = getDataLayer(above, true);
        while (found == null) {
            above = SectionPos.offset(above, Direction.UP);
            found = getDataLayer(above, true);
        }
        byte[] source = found.getData();
        byte[] tiled = new byte[2048];
        for (int row = 0; row < 16; row++) {
            System.arraycopy(source, 0, tiled, row * 128, 128);
        }
        return new DataLayer(tiled);
    }
    protected boolean hasLightDataAtOrBelow(int sectionY) {
        return sectionY >= updatingSectionData.currentLowestY;
    }
    protected boolean isAboveData(long sectionNode) {
        int top = updatingSectionData.topSections.get(SectionPos.getZeroNode(sectionNode));
        return top == updatingSectionData.currentLowestY || SectionPos.y(sectionNode) >= top;
    }
    protected int getTopSectionY(long zeroNode) {
        return updatingSectionData.topSections.get(zeroNode);
    }
    protected int getBottomSectionY() {
        return updatingSectionData.currentLowestY;
    }
    protected static final class SkyDataLayerStorageMap extends DataLayerStorageMap<SkyDataLayerStorageMap> {
        private int currentLowestY;
        private final Long2IntOpenHashMap topSections;
        public SkyDataLayerStorageMap(Long2ObjectOpenHashMap<DataLayer> map, Long2IntOpenHashMap topSections, int currentLowestY) {
            super(map);
            this.topSections = topSections;
            topSections.defaultReturnValue(currentLowestY);
            this.currentLowestY = currentLowestY;
        }
        public SkyDataLayerStorageMap copy() {
            return new SkyDataLayerStorageMap(map.clone(), topSections.clone(), currentLowestY);
        }
    }
}
