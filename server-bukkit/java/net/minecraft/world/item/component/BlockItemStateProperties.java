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
    public static final BlockItemStateProperties EMPTY = null;
    public static final Codec<BlockItemStateProperties> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.BlockItemStateProperties.class, "CODEC");
    public static final StreamCodec<ByteBuf, BlockItemStateProperties> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.<ByteBuf, String, String, Map<String, String>>map(it.unimi.dsi.fastutil.objects.Object2ObjectOpenHashMap::new, net.minecraft.network.codec.ByteBufCodecs.STRING_UTF8, net.minecraft.network.codec.ByteBufCodecs.STRING_UTF8).map(BlockItemStateProperties::new, BlockItemStateProperties::properties);
    public <T extends Comparable<T>> BlockItemStateProperties with(Property<T> property, T value) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.BlockItemStateProperties#with");
    }
    public <T extends Comparable<T>> BlockItemStateProperties with(Property<T> property, BlockState state) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.BlockItemStateProperties#with");
    }
    public <T extends Comparable<T>> T get(Property<T> property) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.BlockItemStateProperties#get");
    }
    public BlockState apply(BlockState state) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.BlockItemStateProperties#apply");
    }
    public boolean isEmpty() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.BlockItemStateProperties#isEmpty");
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.BlockItemStateProperties#addToTooltip");
    }
}
