// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
public record UseRemainder(ItemStackTemplate convertInto) {
    public static final Codec<UseRemainder> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.UseRemainder.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, UseRemainder> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.UseRemainder.class, "STREAM_CODEC");
    public ItemStack convertIntoRemainder(ItemStack usedStack, int stackCountBeforeUsing, boolean hasInfiniteMaterials, OnExtraCreatedRemainder onExtraCreatedRemainder) {
        if (hasInfiniteMaterials || usedStack.getCount() >= stackCountBeforeUsing) {
            return usedStack;
        }
        ItemStack remainder = convertInto.create();
        if (usedStack.isEmpty()) {
            return remainder;
        }
        onExtraCreatedRemainder.apply(remainder);
        return usedStack;
    }
    public interface OnExtraCreatedRemainder {
        void apply(ItemStack extraCreatedRemainder);
    }
}
