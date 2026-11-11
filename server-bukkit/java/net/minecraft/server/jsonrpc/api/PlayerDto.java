// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.api;

import com.mojang.authlib.GameProfile;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.players.NameAndId;

public record PlayerDto(Optional<UUID> id, Optional<String> name) {
    public static final MapCodec<PlayerDto> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(PlayerDto.class, "CODEC");

    public static PlayerDto from(GameProfile gameProfile) {
        return new PlayerDto(Optional.ofNullable(gameProfile.id()), Optional.ofNullable(gameProfile.name()));
    }

    public static PlayerDto from(NameAndId nameAndId) {
        return new PlayerDto(Optional.of(nameAndId.id()), Optional.of(nameAndId.name()));
    }

    public static PlayerDto from(ServerPlayer player) {
        return new PlayerDto(Optional.of(player.getUUID()), Optional.of(player.getName().getString()));
    }

    public org.bukkit.OfflinePlayer resolve() {
        if (id.isPresent()) {
            return org.bukkit.Bukkit.getOfflinePlayer(id.get());
        }
        return name.map(org.bukkit.Bukkit::getOfflinePlayer).orElse(null);
    }
}
