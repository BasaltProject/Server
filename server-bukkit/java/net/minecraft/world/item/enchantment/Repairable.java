// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import com.mojang.serialization.Codec;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
public record Repairable(HolderSet<Item> items) {
    public static final Codec<Repairable> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.Repairable.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Repairable> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.enchantment.Repairable.class, "STREAM_CODEC");
    public boolean isValidRepairItem(ItemStack repairItemStack) {
        return repairItemStack.is(items);
    }
}
