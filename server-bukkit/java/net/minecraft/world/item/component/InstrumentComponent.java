// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Instrument;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;
public record InstrumentComponent(Holder<Instrument> instrument) implements TooltipProvider {
    public static final Codec<InstrumentComponent> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.InstrumentComponent.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, InstrumentComponent> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.InstrumentComponent.class, "STREAM_CODEC");
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        consumer.accept(net.minecraft.network.chat.ComponentUtils.mergeStyles(instrument.value().description(), net.minecraft.network.chat.Style.EMPTY.withColor(net.minecraft.ChatFormatting.GRAY)));
    }
}
