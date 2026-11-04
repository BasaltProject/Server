// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.level.block.entity.BeehiveBlockEntity;
public record Bees(List<BeehiveBlockEntity.Occupant> bees) implements TooltipProvider {
    public static final Codec<Bees> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.Bees.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Bees> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.Bees.class, "STREAM_CODEC");
    public static final Bees EMPTY = new Bees(java.util.List.of());
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        consumer.accept(Component.translatable("container.beehive.bees", bees.size(), 3).withStyle(net.minecraft.ChatFormatting.GRAY));
    }
}
