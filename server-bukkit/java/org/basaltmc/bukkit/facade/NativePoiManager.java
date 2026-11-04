package org.basaltmc.bukkit.facade;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.entity.ai.village.poi.PoiRecord;
import net.minecraft.world.entity.ai.village.poi.PoiType;
import net.minecraft.world.level.ChunkPos;
import org.basaltmc.bukkit.WorldEnvNatives;

/** A point-of-interest manager whose queries are answered by the native world instead of an in-memory index. */
public final class NativePoiManager extends PoiManager {
    private final String world;

    public NativePoiManager(String world) {
        super(null, null, null, false, null, null, null);
        this.world = world;
    }

    private Stream<PoiRecord> rows(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        int mode = occupancy == Occupancy.HAS_SPACE ? 0 : occupancy == Occupancy.IS_OCCUPIED ? 1 : 2;
        String[] rows = WorldEnvNatives.pointsOfInterest(world, new String[0], center.getX(), center.getY(), center.getZ(), radius, mode);
        List<PoiRecord> out = new ArrayList<>();
        for (String row : rows == null ? new String[0] : rows) {
            String[] part = row.split("\\|");
            PoiType type = BuiltInRegistries.POINT_OF_INTEREST_TYPE.getValue(Identifier.parse(part[0]));
            if (type == null) {
                continue;
            }
            Holder<PoiType> holder = BuiltInRegistries.POINT_OF_INTEREST_TYPE.wrapAsHolder(type);
            if (predicate.test(holder)) {
                out.add(new PoiRecord(new BlockPos(Integer.parseInt(part[1]), Integer.parseInt(part[2]), Integer.parseInt(part[3])), holder, () -> {
                }));
            }
        }
        return out.stream();
    }

    @Override
    public Stream<PoiRecord> getInRange(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        return rows(predicate, center, radius, occupancy);
    }

    @Override
    public Stream<PoiRecord> getInSquare(Predicate<Holder<PoiType>> predicate, BlockPos center, int radius, Occupancy occupancy) {
        return rows(predicate, center, (int) Math.ceil(radius * Math.sqrt(2.0)), occupancy).filter(record ->
            Math.abs(record.getPos().getX() - center.getX()) <= radius && Math.abs(record.getPos().getZ() - center.getZ()) <= radius);
    }

    @Override
    public Stream<PoiRecord> getInChunk(Predicate<Holder<PoiType>> predicate, ChunkPos chunkPos, Occupancy occupancy) {
        BlockPos middle = new BlockPos(chunkPos.x() * 16 + 8, 64, chunkPos.z() * 16 + 8);
        return rows(predicate, middle, 400, occupancy).filter(record ->
            (record.getPos().getX() >> 4) == chunkPos.x() && (record.getPos().getZ() >> 4) == chunkPos.z());
    }

    @Override
    public boolean exists(BlockPos pos, Predicate<Holder<PoiType>> predicate) {
        return rows(predicate, pos, 1, Occupancy.ANY).anyMatch(record -> record.getPos().equals(pos));
    }

    @Override
    public java.util.Optional<Holder<PoiType>> getType(BlockPos pos) {
        return rows(type -> true, pos, 1, Occupancy.ANY).filter(record -> record.getPos().equals(pos)).map(PoiRecord::getPoiType).findFirst();
    }
}
