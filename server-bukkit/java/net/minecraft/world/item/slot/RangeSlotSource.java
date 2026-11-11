// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.slot;
import net.minecraft.world.entity.SlotProvider;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.inventory.SlotRange;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextArg;
public class RangeSlotSource implements SlotSource {
    private static final LootContextArg<Object> CONTAINER_ARG = LootContextArg.of(LootContextParams.CONTAINER);
    private final LootContextArg<Object> source;
    private final SlotRange slotRange;
    public static final MapCodec<RangeSlotSource> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.slot.RangeSlotSource.class, "MAP_CODEC");
    private RangeSlotSource(LootContextArg<Object> source, SlotRange slotRange) {
        this.source = source;
        this.slotRange = slotRange;
    }
    public MapCodec<RangeSlotSource> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(source.contextParam());
    }
    public final SlotCollection provide(LootContext context) {
        return source.get(context) instanceof SlotProvider provider ? provider.getSlotsFromRange(slotRange.slots()) : SlotCollection.EMPTY;
    }
    public static RangeSlotSource slotRange(SlotRange slotRange) {
        return new RangeSlotSource(CONTAINER_ARG, slotRange);


    }
}
