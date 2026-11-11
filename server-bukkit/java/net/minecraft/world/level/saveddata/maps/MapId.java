// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata.maps;
import net.minecraft.ChatFormatting;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.MapPostProcessing;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.item.component.TooltipProvider;
public record MapId(int id) implements TooltipProvider {
    public static final Codec<MapId> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.saveddata.maps.MapId.class, "CODEC");
    public static final StreamCodec<ByteBuf, MapId> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.saveddata.maps.MapId.class, "STREAM_CODEC");
    public String key() {
        return "maps/" + id;

    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        MapItemSavedData data = context.mapData(this);
        if (data == null) {
            consumer.accept(Component.translatable("filled_map.unknown").withStyle(ChatFormatting.GRAY));
            return;
        }
        MapPostProcessing post = components.get(DataComponents.MAP_POST_PROCESSING);
        if (post == null && components.get(DataComponents.CUSTOM_NAME) == null) {
            consumer.accept(Component.translatable("filled_map.id", id).withStyle(ChatFormatting.GRAY));
        }
        if (data.locked || post == MapPostProcessing.LOCK) {
            consumer.accept(Component.translatable("filled_map.locked", id).withStyle(ChatFormatting.GRAY));
        }
        if (flag.isAdvanced()) {
            int shown = Math.min(data.scale + (post == MapPostProcessing.SCALE ? 1 : 0), 4);
            consumer.accept(Component.translatable("filled_map.scale", 1 << shown).withStyle(ChatFormatting.GRAY));
            consumer.accept(Component.translatable("filled_map.level", shown, 4).withStyle(ChatFormatting.GRAY));
        }
    }
}
