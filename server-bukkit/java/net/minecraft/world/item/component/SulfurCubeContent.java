// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.TooltipFlag;
public record SulfurCubeContent(ItemStackTemplate absorbedBlockItemStack) implements TooltipProvider {
    public static final Codec<SulfurCubeContent> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.SulfurCubeContent.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, SulfurCubeContent> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.SulfurCubeContent.class, "STREAM_CODEC");
    public static SulfurCubeContent ofNonEmpty(ItemStack itemStack) {
        return new SulfurCubeContent(net.minecraft.world.item.ItemStackTemplate.fromNonEmptyStack(itemStack));
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        ItemStack current = absorbedBlockItemStack.create();
        consumer.accept(Component.translatable("entity.minecraft.sulfur_cube.content", current.getHoverName()).withStyle(net.minecraft.ChatFormatting.ITALIC, net.minecraft.ChatFormatting.GRAY));
    }
}
