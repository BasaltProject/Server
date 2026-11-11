// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;

import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.chat.Component;

public record Message(Optional<String> literal, Optional<String> translatable, Optional<List<String>> translatableParams) {
    public static final Codec<Message> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(Message.class, "CODEC");

    public Optional<Component> asComponent() {
        if (literal.isPresent()) {
            return Optional.of(Component.literal(literal.get()));
        }
        return translatable.map(key -> Component.translatable(key, translatableParams.orElse(List.of()).toArray()));
    }
}
