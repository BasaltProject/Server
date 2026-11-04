// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.core.ClientAsset;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.world.item.ItemStackTemplate;
public record DisplayInfo(ItemStackTemplate icon, Component title, Component description, Optional<ClientAsset.ResourceTexture> background, AdvancementType type, boolean showToast, boolean announceToChat, boolean hidden) {
    public static final Codec<DisplayInfo> CODEC = RecordCodecBuilder.create(i -> i.group(
        ItemStackTemplate.CODEC.fieldOf("icon").forGetter(DisplayInfo::icon),
        ComponentSerialization.CODEC.fieldOf("title").forGetter(DisplayInfo::title),
        ComponentSerialization.CODEC.fieldOf("description").forGetter(DisplayInfo::description),
        ClientAsset.ResourceTexture.CODEC.optionalFieldOf("background").forGetter(DisplayInfo::background),
        AdvancementType.CODEC.optionalFieldOf("frame", AdvancementType.TASK).forGetter(DisplayInfo::type),
        Codec.BOOL.optionalFieldOf("show_toast", true).forGetter(DisplayInfo::showToast),
        Codec.BOOL.optionalFieldOf("announce_to_chat", true).forGetter(DisplayInfo::announceToChat),
        Codec.BOOL.optionalFieldOf("hidden", false).forGetter(DisplayInfo::hidden)).apply(i, DisplayInfo::new));
}
