// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.item.component.TooltipProvider;
public record BannerPatternLayers(List<Layer> layers) implements TooltipProvider {
    public static final BannerPatternLayers EMPTY = new BannerPatternLayers(List.of());
    public static final Codec<BannerPatternLayers> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.BannerPatternLayers.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, BannerPatternLayers> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.BannerPatternLayers.class, "STREAM_CODEC");
    public BannerPatternLayers removeLast() {
        return layers.isEmpty() ? this : new BannerPatternLayers(List.copyOf(layers.subList(0, layers.size() - 1)));
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        for (int i = 0; i < Math.min(layers.size(), 6); i++) {
            consumer.accept(layers.get(i).description().withStyle(net.minecraft.ChatFormatting.GRAY));
        }
    }
    public static record Layer(Holder<BannerPattern> pattern, DyeColor color) {
        public static final Codec<Layer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.BannerPatternLayers.Layer.class, "CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, Layer> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.BannerPatternLayers.Layer.class, "STREAM_CODEC");
        public MutableComponent description() {
            String key = pattern.unwrapKey().map(k -> k.identifier().getPath()).orElse("unknown");
            return Component.translatable("block.minecraft.banner." + key + "." + color.getName());
        }
    }
    public static class Builder {
        private final java.util.ArrayList<Layer> layers = new java.util.ArrayList<>();

        public Builder addIfRegistered(HolderGetter<BannerPattern> patternGetter, ResourceKey<BannerPattern> patternKey, DyeColor color) {
            patternGetter.get(patternKey).ifPresent(pattern -> add(pattern, color));
            return this;
        }

        public Builder add(Holder<BannerPattern> pattern, DyeColor color) {
            return add(new Layer(pattern, color));
        }

        public Builder add(Layer layer) {
            layers.add(layer);
            return this;
        }

        public Builder addAll(BannerPatternLayers layers) {
            this.layers.addAll(layers.layers());
            return this;
        }

        public BannerPatternLayers build() {
            return new BannerPatternLayers(List.copyOf(layers));
        }
    }
}
