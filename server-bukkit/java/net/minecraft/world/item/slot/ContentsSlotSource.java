// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.slot;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.ContainerComponentManipulator;
public class ContentsSlotSource extends TransformedSlotSource {
    private final ContainerComponentManipulator<?> component;
    public static final MapCodec<ContentsSlotSource> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.slot.ContentsSlotSource.class, "MAP_CODEC");
    private ContentsSlotSource(Holder<SlotSource> slotSource, ContainerComponentManipulator<?> component) {
        super(slotSource);
        this.component = component;
    }
    public MapCodec<ContentsSlotSource> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    protected SlotCollection transform(SlotCollection slots) {
        return slots.flatMap(component::getSlots);
    }
}
