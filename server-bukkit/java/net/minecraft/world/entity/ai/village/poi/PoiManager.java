// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.village.poi;
import java.util.stream.Collectors;
import java.util.List;
import java.util.Comparator;
import com.mojang.datafixers.DataFixer;
import com.mojang.datafixers.util.Pair;
import java.nio.file.Path;
import java.util.Optional;
import java.util.function.BiPredicate;
import java.util.function.BooleanSupplier;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.SectionPos;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.SectionTracker;
import net.minecraft.util.RandomSource;
import net.minecraft.util.debug.DebugPoiInfo;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.chunk.LevelChunkSection;
import net.minecraft.world.level.chunk.storage.ChunkIOErrorReporter;
import net.minecraft.world.level.chunk.storage.RegionStorageInfo;
import net.minecraft.world.level.chunk.storage.SectionStorage;
public class PoiManager extends SectionStorage<PoiSection, PoiSection.Packed> {
    private final it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap<PoiSection> sections = new it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap<>();

    private PoiSection section(BlockPos pos) {
        return sections.computeIfAbsent(SectionPos.asLong(pos), key -> new PoiSection(() -> {
        }));
    }

    private Stream<PoiRecord> recordsAround(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        int minX = (center.getX() - radius) >> 4;
        int maxX = (center.getX() + radius) >> 4;
        int minY = (center.getY() - radius) >> 4;
        int maxY = (center.getY() + radius) >> 4;
        int minZ = (center.getZ() - radius) >> 4;
        int maxZ = (center.getZ() + radius) >> 4;
        return sections.long2ObjectEntrySet().stream().filter(entry -> {
            SectionPos pos = SectionPos.of(entry.getLongKey());
            return pos.x() >= minX && pos.x() <= maxX && pos.y() >= minY && pos.y() <= maxY && pos.z() >= minZ && pos.z() <= maxZ;
        }).flatMap(entry -> entry.getValue().getRecords(predicate, occupancy));
    }

    public static final int MAX_VILLAGE_DISTANCE = 6;
    public static final int VILLAGE_SECTION_SIZE = 1;
    public PoiManager(RegionStorageInfo info, Path folder, DataFixer fixerUpper, boolean sync, RegistryAccess registryAccess, ChunkIOErrorReporter errorReporter, LevelHeightAccessor levelHeightAccessor) {
        super((net.minecraft.world.level.chunk.storage.SimpleRegionStorage) null, (com.mojang.serialization.Codec) null, (java.util.function.Function) null, (java.util.function.BiFunction) null, (java.util.function.Function) null, (net.minecraft.core.RegistryAccess) null, (net.minecraft.world.level.chunk.storage.ChunkIOErrorReporter) null, (net.minecraft.world.level.LevelHeightAccessor) null);
    }
    public PoiRecord add(BlockPos pos, Holder<PoiType> type) {
        return section(pos).add(pos, type);
    }
    public void remove(BlockPos pos) {
        PoiSection section = sections.get(SectionPos.asLong(pos));
        if (section != null) {
            section.remove(pos);
        }
    }
    public long getCountInRange(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        return getInRange(predicate, center, radius, occupancy).count();
    }
    public boolean existsAtPosition(ResourceKey<PoiType> poiType, BlockPos blockPos) {
        return exists(blockPos, holder -> holder.is(poiType));
    }
    public Stream<PoiRecord> getInSquare(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        return recordsAround(predicate, center, radius, occupancy).filter(record -> Math.abs(record.getPos().getX() - center.getX()) <= radius
            && Math.abs(record.getPos().getZ() - center.getZ()) <= radius);
    }
    public Stream<PoiRecord> getInRange(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        long radiusSqr = (long) radius * radius;
        return recordsAround(predicate, center, radius, occupancy).filter(record -> record.getPos().distSqr(center) <= radiusSqr);
    }
    public Stream<PoiRecord> getInChunk(Predicate<Holder<PoiType>> predicate, ChunkPos chunkPos, Occupancy occupancy) {
        return sections.long2ObjectEntrySet().stream().filter(entry -> {
            SectionPos pos = SectionPos.of(entry.getLongKey());
            return pos.x() == chunkPos.x() && pos.z() == chunkPos.z();
        }).flatMap(entry -> entry.getValue().getRecords(predicate, occupancy));
    }
    public Stream<BlockPos> findAll(Predicate<Holder<PoiType>> predicate, Predicate<BlockPos> filter, BlockPos center, int radius, Occupancy occupancy) {
        return getInRange(predicate, center, radius, occupancy).map(PoiRecord::getPos).filter(filter);
    }
    public Stream<Pair<Holder<PoiType>, BlockPos>> findAllWithType(Predicate<Holder<PoiType>> predicate, Predicate<BlockPos> filter, BlockPos center, int radius, Occupancy occupancy) {
        return getInRange(predicate, center, radius, occupancy).filter(record -> filter.test(record.getPos()))
            .map(record -> Pair.of(record.getPoiType(), record.getPos()));
    }
    public Stream<Pair<Holder<PoiType>, BlockPos>> findAllClosestFirstWithType(Predicate<Holder<PoiType>> predicate, Predicate<BlockPos> filter, BlockPos center, int radius, Occupancy occupancy) {
        return findAllWithType(predicate, filter, center, radius, occupancy).sorted(Comparator.comparingDouble(pair -> pair.getSecond().distSqr(center)));
    }
    public Optional<BlockPos> find(Predicate<Holder<PoiType>> predicate, Predicate<BlockPos> filter, BlockPos center, int radius, Occupancy occupancy) {
        return findAll(predicate, filter, center, radius, occupancy).findFirst();
    }
    public Optional<BlockPos> findClosest(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        return findClosest(predicate, pos -> true, center, radius, occupancy);
    }
    public Optional<Pair<Holder<PoiType>, BlockPos>> findClosestWithType(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        return findAllClosestFirstWithType(predicate, pos -> true, center, radius, occupancy).findFirst();
    }
    public Optional<BlockPos> findClosest(Predicate<Holder<PoiType>> predicate, Predicate<BlockPos> filter, BlockPos center, int radius, Occupancy occupancy) {
        return findAll(predicate, filter, center, radius, occupancy).min(Comparator.comparingDouble(pos -> pos.distSqr(center)));
    }
    public Optional<BlockPos> take(Predicate<Holder<PoiType>> predicate, BiPredicate<Holder<PoiType>, BlockPos> filter, BlockPos center, int radius) {
        return getInRange(predicate, center, radius, Occupancy.HAS_SPACE).filter(record -> filter.test(record.getPoiType(), record.getPos()))
            .sorted(Comparator.comparingDouble(record -> record.getPos().distSqr(center))).filter(PoiRecord::acquireTicket).map(PoiRecord::getPos)
            .findFirst();
    }
    public Optional<BlockPos> getRandom(Predicate<Holder<PoiType>> predicate, Predicate<BlockPos> filter, Occupancy occupancy, BlockPos center, int radius, RandomSource random) {
        List<PoiRecord> found = getInRange(predicate, center, radius, occupancy).filter(record -> filter.test(record.getPos())).collect(Collectors.toList());
        java.util.Collections.shuffle(found, new java.util.Random(random.nextLong()));
        return found.stream().map(PoiRecord::getPos).findFirst();
    }
    public boolean release(BlockPos pos) {
        PoiSection section = sections.get(SectionPos.asLong(pos));
        return section != null && section.release(pos);
    }
    public boolean exists(BlockPos pos, Predicate<Holder<PoiType>> predicate) {
        PoiSection section = sections.get(SectionPos.asLong(pos));
        return section != null && section.exists(pos, predicate);
    }
    public Optional<Holder<PoiType>> getType(BlockPos pos) {
        PoiSection section = sections.get(SectionPos.asLong(pos));
        return section == null ? Optional.empty() : section.getType(pos);
    }
    public DebugPoiInfo getDebugPoiInfo(BlockPos pos) {
        return getInRange(type -> true, pos, 1, Occupancy.ANY).filter(record -> record.getPos().equals(pos)).findFirst()
            .map(DebugPoiInfo::new).orElse(null);
    }
    public int sectionsToVillage(SectionPos sectionPos) {
        return 7;
    }
    public void tick(BooleanSupplier haveTime) {
        return;
    }
    protected void setDirty(long sectionPos) {
        return;
    }
    protected void onSectionLoad(long sectionPos) {
        return;
    }
    public void checkConsistencyWithBlocks(SectionPos sectionPos, LevelChunkSection blockSection) {
        return;
    }
    public void ensureLoadedAndValid(LevelReader reader, BlockPos center, int radius) {
        return;
    }
    public static enum Occupancy {
        HAS_SPACE, IS_OCCUPIED, ANY;
        public Predicate<? super PoiRecord> getTest() {
        return switch (this) {
            case HAS_SPACE -> PoiRecord::hasSpace;
            case IS_OCCUPIED -> PoiRecord::isOccupied;
            case ANY -> record -> true;
        };
        }
    }
    final class DistanceTracker extends SectionTracker {
        DistanceTracker() {
            super((int) 0, (int) 0, (int) 0);
        }
        protected int getLevelFromSource(long to) {
            return 7;
        }
        protected int getLevel(long node) {
            return 7;
        }
        protected void setLevel(long node, int level) {
            return;
        }
        public void runAllUpdates() {
            return;
        }
    }
}
