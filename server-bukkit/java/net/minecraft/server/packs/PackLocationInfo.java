// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;

import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.server.packs.repository.KnownPack;
import net.minecraft.server.packs.repository.PackSource;

public record PackLocationInfo(String id, Component title, PackSource source, Optional<KnownPack> knownPackInfo) {
    public Component createChatLink(boolean enabled, Component description) {
        return net.minecraft.network.chat.ComponentUtils.wrapInSquareBrackets(source.decorate(Component.literal(id)))
            .withStyle(style -> style.withColor(enabled ? net.minecraft.ChatFormatting.GREEN : net.minecraft.ChatFormatting.RED).withInsertion(id));
    }
}
