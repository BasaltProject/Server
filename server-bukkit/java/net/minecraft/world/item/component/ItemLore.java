// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.Style;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;

public record ItemLore(List<Component> lines, List<Component> styledLines) implements TooltipProvider {
    public static final ItemLore EMPTY = new ItemLore(List.of());
    public static final int MAX_LINES = 256;
    private static final Style kLoreStyle = Style.EMPTY.withColor(net.minecraft.ChatFormatting.DARK_PURPLE).withItalic(true);
    public static final Codec<ItemLore> CODEC = ComponentSerialization.CODEC.listOf(0, MAX_LINES).xmap(ItemLore::new, ItemLore::lines);
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemLore> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.ItemLore.class, "STREAM_CODEC");

    public ItemLore(List<Component> lines) {
        this(lines, styled(lines));
    }

    public ItemLore {
        if (lines.size() > MAX_LINES) {
            throw new IllegalArgumentException("Got " + lines.size() + " lines, but maximum is 256");
        }
    }

    private static List<Component> styled(List<Component> lines) {
        List<Component> out = new ArrayList<>(lines.size());
        for (Component line : lines) {
            out.add(net.minecraft.network.chat.ComponentUtils.mergeStyles(line, kLoreStyle));
        }
        return List.copyOf(out);
    }

    public ItemLore withLineAdded(Component component) {
        List<Component> next = new ArrayList<>(lines);
        next.add(component);
        return new ItemLore(next);
    }

    @Override
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        styledLines.forEach(consumer);
    }
}
