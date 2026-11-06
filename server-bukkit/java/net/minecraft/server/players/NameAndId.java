// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import com.mojang.authlib.GameProfile;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.UUID;
import net.minecraft.core.UUIDUtil;

public record NameAndId(UUID id, String name) {
    public static final Codec<NameAndId> CODEC = RecordCodecBuilder.create(i -> i.group(
        UUIDUtil.CODEC.fieldOf("id").forGetter(NameAndId::id),
        Codec.STRING.fieldOf("name").forGetter(NameAndId::name)).apply(i, NameAndId::new));

    public NameAndId(GameProfile profile) {
        this(profile.id(), profile.name());
    }

    public NameAndId(com.mojang.authlib.services.response.NameAndId profile) {
        this(profile.id(), profile.name());
    }

    public static NameAndId fromJson(JsonObject object) {
        if (!object.has("uuid") || !object.has("name")) {
            return null;
        }
        try {
            return new NameAndId(UUID.fromString(object.get("uuid").getAsString()), object.get("name").getAsString());
        } catch (IllegalArgumentException invalid) {
            return null;
        }
    }

    public void appendTo(JsonObject output) {
        output.addProperty("uuid", id.toString());
        output.addProperty("name", name);
    }

    public static NameAndId createOffline(String name) {
        return new NameAndId(UUID.nameUUIDFromBytes(("OfflinePlayer:" + name).getBytes(java.nio.charset.StandardCharsets.UTF_8)), name);
    }
}
