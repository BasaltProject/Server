// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import com.mojang.serialization.Codec;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.item.component.TooltipProvider;
public record PotDecorations(Optional<ItemStackTemplate> back, Optional<ItemStackTemplate> left, Optional<ItemStackTemplate> right, Optional<ItemStackTemplate> front) implements TooltipProvider {
    public static final PotDecorations EMPTY = new PotDecorations(Optional.empty(), Optional.empty(), Optional.empty(), Optional.empty());
    public static final Codec<PotDecorations> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.PotDecorations.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, PotDecorations> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.PotDecorations.class, "STREAM_CODEC");
    public static PotDecorations allBrick(HolderGetter<Item> items) {
        Optional<ItemStackTemplate> brick = Optional.of(new ItemStackTemplate(net.minecraft.world.item.Items.BRICK));
        return new PotDecorations(brick, brick, brick, brick);
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        if (equals(EMPTY) || equals(allBrick(null))) {
            return;
        }
        for (Optional<ItemStackTemplate> side : java.util.List.of(front, left, right, back)) {
            consumer.accept(side.map(t -> Component.empty().append(t.create().getHoverName()).withStyle(net.minecraft.ChatFormatting.GRAY))
                .orElse(Component.translatable("item.minecraft.brick").withStyle(net.minecraft.ChatFormatting.GRAY)));
        }
    }
}
