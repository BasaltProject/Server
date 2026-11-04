// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.village.poi;

import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.shorts.Short2ObjectMap;
import it.unimi.dsi.fastutil.shorts.Short2ObjectOpenHashMap;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.SectionPos;

public class PoiSection {
    private final Short2ObjectMap<PoiRecord> records = new Short2ObjectOpenHashMap<>();
    private final Map<Holder<PoiType>, Set<PoiRecord>> byType = new HashMap<>();
    private final Runnable setDirty;
    private boolean isValid;

    public PoiSection(Runnable setDirty) {
        this(setDirty, true, List.of());
    }

    private PoiSection(Runnable setDirty, boolean isValid, List<PoiRecord> records) {
        this.setDirty = setDirty;
        this.isValid = isValid;
        records.forEach(this::add);
    }

    public Packed pack() {
        return new Packed(isValid, records.values().stream().map(PoiRecord::pack).toList());
    }

    public Stream<PoiRecord> getRecords(Predicate<Holder<PoiType>> predicate, PoiManager.Occupancy occupancy) {
        return byType.entrySet().stream().filter(entry -> predicate.test(entry.getKey())).flatMap(entry -> entry.getValue().stream())
            .filter(occupancy.getTest());
    }

    public PoiRecord add(BlockPos blockPos, Holder<PoiType> type) {
        PoiRecord record = new PoiRecord(blockPos, type, setDirty);
        add(record);
        return record;
    }

    private boolean add(PoiRecord record) {
        BlockPos pos = record.getPos();
        Holder<PoiType> type = record.getPoiType();
        short key = SectionPos.sectionRelativePos(pos);
        PoiRecord existing = records.get(key);
        if (existing != null) {
            if (type.equals(existing.getPoiType())) {
                return false;
            }
            throw new IllegalStateException("POI data mismatch: already registered at " + pos);
        }
        records.put(key, record);
        byType.computeIfAbsent(type, ignored -> new HashSet<>()).add(record);
        return true;
    }

    public void remove(BlockPos pos) {
        PoiRecord removed = records.remove(SectionPos.sectionRelativePos(pos));
        if (removed != null) {
            byType.get(removed.getPoiType()).remove(removed);
            setDirty.run();
        }
    }

    public int getFreeTickets(BlockPos pos) {
        return getPoiRecord(pos).map(PoiRecord::getFreeTickets).orElse(0);
    }

    public boolean release(BlockPos pos) {
        PoiRecord record = records.get(SectionPos.sectionRelativePos(pos));
        if (record == null) {
            throw new IllegalStateException("POI never registered at " + pos);
        }
        return record.releaseTicket();
    }

    public boolean exists(BlockPos pos, Predicate<Holder<PoiType>> predicate) {
        return getType(pos).filter(predicate).isPresent();
    }

    public Optional<Holder<PoiType>> getType(BlockPos pos) {
        return getPoiRecord(pos).map(PoiRecord::getPoiType);
    }

    private Optional<PoiRecord> getPoiRecord(BlockPos pos) {
        return Optional.ofNullable(records.get(SectionPos.sectionRelativePos(pos)));
    }

    public void refresh(Consumer<BiConsumer<BlockPos, Holder<PoiType>>> updater) {
        isValid = true;
    }

    public boolean isValid() {
        return isValid;
    }

    public static record Packed(boolean isValid, List<PoiRecord.Packed> records) {
        public static final Codec<Packed> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.ai.village.poi.PoiSection.Packed.class, "CODEC");

        public PoiSection unpack(Runnable setDirty) {
            return new PoiSection(setDirty, isValid, records.stream().map(packed -> packed.unpack(setDirty)).toList());
        }
    }
}
