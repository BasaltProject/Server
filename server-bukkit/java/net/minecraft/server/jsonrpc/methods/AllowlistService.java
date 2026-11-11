// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.server.jsonrpc.api.PlayerDto;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import org.bukkit.Bukkit;
import org.bukkit.OfflinePlayer;

public class AllowlistService {
    public static List<PlayerDto> get(MinecraftApi minecraftApi) {
        List<PlayerDto> result = new ArrayList<>();
        for (OfflinePlayer player : Bukkit.getWhitelistedPlayers()) {
            result.add(new PlayerDto(java.util.Optional.of(player.getUniqueId()), java.util.Optional.ofNullable(player.getName())));
        }
        return result;
    }

    public static List<PlayerDto> add(MinecraftApi minecraftApi, List<PlayerDto> playerDtos, ClientInfo clientInfo) {
        for (PlayerDto dto : playerDtos) {
            OfflinePlayer player = dto.resolve();
            if (player != null) {
                player.setWhitelisted(true);
            }
        }
        return get(minecraftApi);
    }

    public static List<PlayerDto> clear(MinecraftApi minecraftApi, ClientInfo clientInfo) {
        for (OfflinePlayer player : Bukkit.getWhitelistedPlayers()) {
            player.setWhitelisted(false);
        }
        return get(minecraftApi);
    }

    public static List<PlayerDto> remove(MinecraftApi minecraftApi, List<PlayerDto> playerDtos, ClientInfo clientInfo) {
        for (PlayerDto dto : playerDtos) {
            OfflinePlayer player = dto.resolve();
            if (player != null) {
                player.setWhitelisted(false);
            }
        }
        return get(minecraftApi);
    }

    public static List<PlayerDto> set(MinecraftApi minecraftApi, List<PlayerDto> playerDtos, ClientInfo clientInfo) {
        clear(minecraftApi, clientInfo);
        return add(minecraftApi, playerDtos, clientInfo);
    }
}
