// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.SequencedSet;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public record TooltipDisplay(boolean hideTooltip, SequencedSet<DataComponentType<?>> hiddenComponents) {
    public static final Codec<TooltipDisplay> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.TooltipDisplay.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, TooltipDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.TooltipDisplay.class, "STREAM_CODEC");
    public static final TooltipDisplay DEFAULT = new TooltipDisplay(false, new java.util.LinkedHashSet<>());
    public TooltipDisplay withHidden(DataComponentType<?> component, boolean hidden) {
        if (hiddenComponents.contains(component) == hidden) {
            return this;
        }
        java.util.LinkedHashSet<DataComponentType<?>> next = new java.util.LinkedHashSet<>(hiddenComponents);
        if (hidden) {
            next.add(component);
        } else {
            next.remove(component);
        }
        return new TooltipDisplay(hideTooltip, next);
    }
    public boolean shows(DataComponentType<?> component) {
        return !hideTooltip && !hiddenComponents.contains(component);
    }
}
