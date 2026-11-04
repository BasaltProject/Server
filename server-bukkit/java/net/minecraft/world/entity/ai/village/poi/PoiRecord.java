// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.village.poi;

import com.mojang.serialization.Codec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;

public class PoiRecord {
    private final BlockPos pos;
    private final Holder<PoiType> poiType;
    private int freeTickets;
    private final Runnable setDirty;

    private PoiRecord(BlockPos pos, Holder<PoiType> poiType, int freeTickets, Runnable setDirty) {
        this.pos = pos.immutable();
        this.poiType = poiType;
        this.freeTickets = freeTickets;
        this.setDirty = setDirty;
    }

    public PoiRecord(BlockPos pos, Holder<PoiType> poiType, Runnable setDirty) {
        this(pos, poiType, poiType.value().maxTickets(), setDirty);
    }

    public Packed pack() {
        return new Packed(pos, poiType, freeTickets);
    }

    public int getFreeTickets() {
        return freeTickets;
    }

    protected boolean acquireTicket() {
        if (freeTickets <= 0) {
            return false;
        }
        freeTickets--;
        setDirty.run();
        return true;
    }

    protected boolean releaseTicket() {
        if (freeTickets >= poiType.value().maxTickets()) {
            return false;
        }
        freeTickets++;
        setDirty.run();
        return true;
    }

    public boolean hasSpace() {
        return freeTickets > 0;
    }

    public boolean isOccupied() {
        return freeTickets != poiType.value().maxTickets();
    }

    public BlockPos getPos() {
        return pos;
    }

    public Holder<PoiType> getPoiType() {
        return poiType;
    }

    @Override
    public boolean equals(Object o) {
        return this == o || (o instanceof PoiRecord other && pos.equals(other.pos));
    }

    @Override
    public int hashCode() {
        return pos.hashCode();
    }

    public static record Packed(BlockPos pos, Holder<PoiType> poiType, int freeTickets) {
        public static final Codec<Packed> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.ai.village.poi.PoiRecord.Packed.class, "CODEC");

        public PoiRecord unpack(Runnable setDirty) {
            return new PoiRecord(pos, poiType, freeTickets, setDirty);
        }
    }
}
