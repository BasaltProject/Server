// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Map;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;
public record BlockItemStateProperties(Map<String, String> properties) implements TooltipProvider {
    public static final BlockItemStateProperties EMPTY = new BlockItemStateProperties(Map.of());
    private static <T extends Comparable<T>> BlockState withValue(BlockState state, Property<T> property, String name) {
        return property.getValue(name).map(value -> state.setValue(property, value)).orElse(state);
    }
    public static final Codec<BlockItemStateProperties> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.BlockItemStateProperties.class, "CODEC");
    public static final StreamCodec<ByteBuf, BlockItemStateProperties> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.<ByteBuf, String, String, Map<String, String>>map(it.unimi.dsi.fastutil.objects.Object2ObjectOpenHashMap::new, net.minecraft.network.codec.ByteBufCodecs.STRING_UTF8, net.minecraft.network.codec.ByteBufCodecs.STRING_UTF8).map(BlockItemStateProperties::new, BlockItemStateProperties::properties);
    public <T extends Comparable<T>> BlockItemStateProperties with(Property<T> property, T value) {
        Map<String, String> next = new java.util.LinkedHashMap<>(properties);
        next.put(property.getName(), property.getName(value));
        return new BlockItemStateProperties(Map.copyOf(next));
    }
    public <T extends Comparable<T>> BlockItemStateProperties with(Property<T> property, BlockState state) {
        return with(property, state.getValue(property));
    }
    public <T extends Comparable<T>> T get(Property<T> property) {
        String text = properties.get(property.getName());
        return text == null ? null : property.getValue(text).orElse(null);
    }
    public BlockState apply(BlockState state) {
        BlockState result = state;
        for (Map.Entry<String, String> entry : properties.entrySet()) {
            Property<?> property = state.getBlock().getStateDefinition().getProperty(entry.getKey());
            if (property != null) {
                result = withValue(result, property, entry.getValue());
            }
        }
        return result;
    }
    public boolean isEmpty() {
        return properties.isEmpty();
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        return;
    }
}
