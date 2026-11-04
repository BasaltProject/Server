// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.status;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Base64;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.server.players.NameAndId;

public record ServerStatus(Component description, Optional<Players> players, Optional<Version> version, Optional<Favicon> favicon, boolean enforcesSecureChat) {
    public static final Codec<ServerStatus> CODEC = RecordCodecBuilder.create(instance -> instance.group(
        ComponentSerialization.CODEC.lenientOptionalFieldOf("description", Component.empty()).forGetter(ServerStatus::description),
        Players.CODEC.lenientOptionalFieldOf("players").forGetter(ServerStatus::players),
        Version.CODEC.lenientOptionalFieldOf("version").forGetter(ServerStatus::version),
        Favicon.CODEC.lenientOptionalFieldOf("favicon").forGetter(ServerStatus::favicon),
        Codec.BOOL.lenientOptionalFieldOf("enforcesSecureChat", false).forGetter(ServerStatus::enforcesSecureChat)
    ).apply(instance, ServerStatus::new));

    public static record Players(int max, int online, List<NameAndId> sample) {
        private static final Codec<NameAndId> SAMPLE_ENTRY = RecordCodecBuilder.create(instance -> instance.group(
            Codec.STRING.fieldOf("id").forGetter(entry -> entry.id().toString()),
            Codec.STRING.fieldOf("name").forGetter(NameAndId::name)
        ).apply(instance, (id, name) -> new NameAndId(UUID.fromString(id), name)));
        public static final Codec<Players> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            Codec.INT.fieldOf("max").forGetter(Players::max),
            Codec.INT.fieldOf("online").forGetter(Players::online),
            SAMPLE_ENTRY.listOf().lenientOptionalFieldOf("sample", List.of()).forGetter(Players::sample)
        ).apply(instance, Players::new));
    }

    public static record Version(String name, int protocol) {
        public static final Codec<Version> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            Codec.STRING.fieldOf("name").forGetter(Version::name),
            Codec.INT.fieldOf("protocol").forGetter(Version::protocol)
        ).apply(instance, Version::new));

        public static Version current() {
            return new Version("26.3", 777);
        }
    }

    public static record Favicon(byte[] iconBytes) {
        private static final String kPrefix = "data:image/png;base64,";
        public static final Codec<Favicon> CODEC = Codec.STRING.comapFlatMap(text -> {
            if (!text.startsWith(kPrefix)) {
                return com.mojang.serialization.DataResult.error(() -> "Unknown host");
            }
            try {
                return com.mojang.serialization.DataResult.success(new Favicon(Base64.getDecoder().decode(text.substring(kPrefix.length()))));
            } catch (IllegalArgumentException bad) {
                return com.mojang.serialization.DataResult.error(() -> "Malformed base64 server icon");
            }
        }, favicon -> kPrefix + Base64.getEncoder().encodeToString(favicon.iconBytes));
    }
}
