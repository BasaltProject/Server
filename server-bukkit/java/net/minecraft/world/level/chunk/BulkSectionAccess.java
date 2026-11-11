// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.block.Blocks;
import it.unimi.dsi.fastutil.longs.Long2ObjectMap;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
public class BulkSectionAccess implements AutoCloseable {
    private final LevelAccessor level;
    private final Long2ObjectMap<LevelChunkSection> held = new Long2ObjectOpenHashMap<>();
    public BulkSectionAccess(LevelAccessor level) {
        this.level = level;
    }
    public LevelChunkSection getSection(BlockPos pos) {
        int index = level.getSectionIndex(pos.getY());
        if (index < 0 || index >= level.getSectionsCount()) {
            return null;
        }
        long key = SectionPos.asLong(pos);
        LevelChunkSection cached = held.get(key);
        if (cached == null) {
            cached = level.getChunk(SectionPos.blockToSectionCoord(pos.getX()), SectionPos.blockToSectionCoord(pos.getZ())).getSection(index);
            cached.acquire();
            held.put(key, cached);
        }
        return cached;
    }
    public BlockState getBlockState(BlockPos pos) {
        LevelChunkSection section = getSection(pos);
        return section == null ? Blocks.AIR.defaultBlockState()
            : section.getBlockState(SectionPos.sectionRelative(pos.getX()), SectionPos.sectionRelative(pos.getY()), SectionPos.sectionRelative(pos.getZ()));
    }
    public void close() {
        for (LevelChunkSection section : held.values()) {
            section.release();
        }


    }
}
