// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.blockscan;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.LevelChunkSection;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.core.SectionPos;
public class FilteredSectionCache {
    private final LevelReader level;
    private final Predicate<BlockState> statePredicate;
    private final CacheEntry[] slots = new CacheEntry[8];
    public FilteredSectionCache(LevelReader level, Predicate<BlockState> statePredicate) {
        this.level = level;
        this.statePredicate = statePredicate;
    }
    public BlockState getBlockState(BlockPos pos) {
        int sx = SectionPos.blockToSectionCoord(pos.getX());
        int sy = SectionPos.blockToSectionCoord(pos.getY());
        int sz = SectionPos.blockToSectionCoord(pos.getZ());
        if (sy < level.getMinSectionY() || sy > level.getMaxSectionY()) {
            BlockState air = Blocks.AIR.defaultBlockState();
            return statePredicate.test(air) ? air : null;
        }
        LevelChunkSection section = load(sx, sy, sz);
        if (section == null) {
            return null;
        }
        BlockState state = section.getBlockState(SectionPos.sectionRelative(pos.getX()), SectionPos.sectionRelative(pos.getY()), SectionPos.sectionRelative(pos.getZ()));
        return statePredicate.test(state) ? state : null;
    }
    private LevelChunkSection load(int sx, int sy, int sz) {
        int slot = (sx & 1) | (sy & 1) << 1 | (sz & 1) << 2;
        CacheEntry cached = slots[slot];
        if (cached == null || cached.sectionX != sx || cached.sectionY != sy || cached.sectionZ != sz) {
            ChunkAccess chunk = level.getChunk(sx, sz);
            LevelChunkSection section = chunk.getSection(chunk.getSectionIndexFromSectionY(sy));
            cached = new CacheEntry(section.maybeHas(statePredicate) ? section : null, sx, sy, sz);
            slots[slot] = cached;
        }
        return cached.chunkSection;
    }
    static record CacheEntry(LevelChunkSection chunkSection, int sectionX, int sectionY, int sectionZ) {
    }
}
