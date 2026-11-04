// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.TooltipFlag;
public record DyedItemColor(int rgb) implements TooltipProvider {
    public static final Codec<DyedItemColor> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.DyedItemColor.class, "CODEC");
    public static final StreamCodec<ByteBuf, DyedItemColor> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.DyedItemColor.class, "STREAM_CODEC");
    public static final int LEATHER_COLOR = -6265536;
    public static int getOrDefault(ItemStack itemStack, int defaultColor) {
        DyedItemColor color = itemStack.get(net.minecraft.core.component.DataComponents.DYED_COLOR);
        return color != null ? color.rgb() : defaultColor;
    }
    public static ItemStack applyDyes(ItemStack itemStack, List<DyeColor> dyes) {
        DyedItemColor current = itemStack.get(net.minecraft.core.component.DataComponents.DYED_COLOR);
        DyedItemColor next = applyDyes(current, dyes);
        if (next == null) {
            return ItemStack.EMPTY;
        }
        ItemStack copy = itemStack.copyWithCount(1);
        copy.set(net.minecraft.core.component.DataComponents.DYED_COLOR, next);
        return copy;
    }
    public static DyedItemColor applyDyes(DyedItemColor currentDye, List<DyeColor> dyes) {
        int[] sums = new int[3];
        int maxSum = 0;
        int count = 0;
        if (currentDye != null) {
            int rgb = currentDye.rgb();
            int r = rgb >> 16 & 255;
            int g = rgb >> 8 & 255;
            int b = rgb & 255;
            maxSum += Math.max(r, Math.max(g, b));
            sums[0] += r;
            sums[1] += g;
            sums[2] += b;
            count++;
        }
        for (DyeColor dye : dyes) {
            int rgb = dye.getTextureDiffuseColor();
            int r = rgb >> 16 & 255;
            int g = rgb >> 8 & 255;
            int b = rgb & 255;
            maxSum += Math.max(r, Math.max(g, b));
            sums[0] += r;
            sums[1] += g;
            sums[2] += b;
            count++;
        }
        if (count == 0) {
            return null;
        }
        int r = sums[0] / count;
        int g = sums[1] / count;
        int b = sums[2] / count;
        float scale = (float) maxSum / count;
        float peak = Math.max(r, Math.max(g, b));
        r = (int) (r * scale / peak);
        g = (int) (g * scale / peak);
        b = (int) (b * scale / peak);
        return new DyedItemColor((r << 8 | g) << 8 | b);
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        if (flag.isAdvanced()) {
            consumer.accept(Component.translatable("item.color", String.format(java.util.Locale.ROOT, "#%06X", rgb)).withStyle(net.minecraft.ChatFormatting.GRAY));
        } else {
            consumer.accept(Component.translatable("item.dyed").withStyle(net.minecraft.ChatFormatting.GRAY, net.minecraft.ChatFormatting.ITALIC));
        }
    }
}
