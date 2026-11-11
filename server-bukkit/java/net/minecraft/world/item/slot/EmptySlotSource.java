// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.slot;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.storage.loot.LootContext;
public record EmptySlotSource() implements SlotSource {
    public static final MapCodec<EmptySlotSource> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.slot.EmptySlotSource.class, "MAP_CODEC");
    public MapCodec<EmptySlotSource> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public SlotCollection provide(LootContext context) {
        return SlotCollection.EMPTY;
    }
}
