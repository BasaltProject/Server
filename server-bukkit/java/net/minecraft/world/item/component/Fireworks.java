// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;
public record Fireworks(int flightDuration, List<FireworkExplosion> explosions) implements TooltipProvider {
    public static final int MAX_EXPLOSIONS = 256;
    public static final Codec<Fireworks> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.Fireworks.class, "CODEC");
    public static final StreamCodec<ByteBuf, Fireworks> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.Fireworks.class, "STREAM_CODEC");
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        if (flightDuration > 0) {
            consumer.accept(Component.translatable("item.minecraft.firework_rocket.flight_duration", flightDuration).withStyle(net.minecraft.ChatFormatting.GRAY));
        }
        FireworkExplosion current = null;
        int count = 0;
        for (FireworkExplosion explosion : explosions) {
            if (current != null && current.equals(explosion)) {
                count++;
                continue;
            }
            if (current != null) {
                explosionTooltip(consumer, current, count);
            }
            current = explosion;
            count = 1;
        }
        if (current != null) {
            explosionTooltip(consumer, current, count);
        }
    }
    private static void explosionTooltip(Consumer<Component> consumer, FireworkExplosion explosion, int count) {
        Component shape = explosion.shape().getName();
        if (count == 1) {
            consumer.accept(Component.translatable("item.minecraft.firework_rocket.single_star", shape).withStyle(net.minecraft.ChatFormatting.GRAY));
        } else {
            consumer.accept(Component.translatable("item.minecraft.firework_rocket.multiple_stars", count, shape).withStyle(net.minecraft.ChatFormatting.GRAY));
        }
        explosion.addAdditionalTooltip(line -> consumer.accept(Component.literal(" ").append(line)));
    }
}
