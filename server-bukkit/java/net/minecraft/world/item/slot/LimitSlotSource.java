// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.slot;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
public class LimitSlotSource extends TransformedSlotSource {
    private final int limit;
    public static final MapCodec<LimitSlotSource> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.slot.LimitSlotSource.class, "MAP_CODEC");
    private LimitSlotSource(Holder<SlotSource> slotSource, int limit) {
        super(slotSource);
        this.limit = limit;
    }
    public MapCodec<LimitSlotSource> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    protected SlotCollection transform(SlotCollection slots) {
        return slots.limit(limit);
    }
}
