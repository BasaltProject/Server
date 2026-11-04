// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import it.unimi.dsi.fastutil.ints.IntList;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;
public record FireworkExplosion(Shape shape, IntList colors, IntList fadeColors, boolean hasTrail, boolean hasTwinkle) implements TooltipProvider {
    public static final FireworkExplosion DEFAULT = new FireworkExplosion(Shape.SMALL_BALL, it.unimi.dsi.fastutil.ints.IntList.of(), it.unimi.dsi.fastutil.ints.IntList.of(), false, false);
    public static final Codec<IntList> COLOR_LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.FireworkExplosion.class, "COLOR_LIST_CODEC");
    public static final Codec<FireworkExplosion> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.FireworkExplosion.class, "CODEC");
    public static final StreamCodec<ByteBuf, FireworkExplosion> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.FireworkExplosion.class, "STREAM_CODEC");
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        addAdditionalTooltip(consumer);
    }
    public void addAdditionalTooltip(Consumer<Component> consumer) {
        consumer.accept(shape.getName().withStyle(net.minecraft.ChatFormatting.GRAY));
        if (hasTrail) {
            consumer.accept(Component.translatable("item.minecraft.firework_star.trail").withStyle(net.minecraft.ChatFormatting.GRAY));
        }
        if (hasTwinkle) {
            consumer.accept(Component.translatable("item.minecraft.firework_star.flicker").withStyle(net.minecraft.ChatFormatting.GRAY));
        }
    }
    public FireworkExplosion withFadeColors(IntList fadeColors) {
        return new FireworkExplosion(shape, colors, fadeColors, hasTrail, hasTwinkle);
    }
    public static enum Shape implements StringRepresentable {
        SMALL_BALL, LARGE_BALL, STAR, CREEPER, BURST;
        public static final StreamCodec<ByteBuf, Shape> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.FireworkExplosion.Shape.class, "STREAM_CODEC");
        public static final Codec<Shape> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.FireworkExplosion.Shape.class, "CODEC");
        public MutableComponent getName() {
            return Component.translatable("item.minecraft.firework_star.shape." + name().toLowerCase(java.util.Locale.ROOT));
        }
        public int getId() {
            return ordinal();
        }
        public static Shape byId(int id) {
            return values()[Math.floorMod(id, values().length)];
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
