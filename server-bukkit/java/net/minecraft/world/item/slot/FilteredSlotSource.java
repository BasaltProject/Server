// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.slot;
import com.mojang.serialization.MapCodec;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.core.Holder;
public class FilteredSlotSource extends TransformedSlotSource {
    private final ItemPredicate filter;
    public static final MapCodec<FilteredSlotSource> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.slot.FilteredSlotSource.class, "MAP_CODEC");
    private FilteredSlotSource(Holder<SlotSource> slotSource, ItemPredicate filter) {
        super(slotSource);
        this.filter = filter;
    }
    public MapCodec<FilteredSlotSource> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    protected SlotCollection transform(SlotCollection slots) {
        return slots.filter(filter);
    }
}
