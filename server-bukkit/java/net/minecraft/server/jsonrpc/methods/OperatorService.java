// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;

import com.mojang.serialization.MapCodec;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.server.jsonrpc.api.PlayerDto;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.server.permissions.PermissionLevel;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.ServerOpListEntry;
import org.bukkit.Bukkit;
import org.bukkit.OfflinePlayer;

public class OperatorService {
    public static List<OperatorDto> get(MinecraftApi minecraftApi) {
        List<OperatorDto> result = new ArrayList<>();
        for (OfflinePlayer player : Bukkit.getOperators()) {
            result.add(new OperatorDto(new PlayerDto(Optional.of(player.getUniqueId()), Optional.ofNullable(player.getName())),
                Optional.of(PermissionLevel.OWNERS), Optional.of(false)));
        }
        return result;
    }

    public static List<OperatorDto> clear(MinecraftApi minecraftApi, ClientInfo clientInfo) {
        for (OfflinePlayer player : Bukkit.getOperators()) {
            player.setOp(false);
        }
        return get(minecraftApi);
    }

    public static List<OperatorDto> remove(MinecraftApi minecraftApi, List<PlayerDto> playerDtos, ClientInfo clientInfo) {
        for (PlayerDto dto : playerDtos) {
            OfflinePlayer player = dto.resolve();
            if (player != null) {
                player.setOp(false);
            }
        }
        return get(minecraftApi);
    }

    public static List<OperatorDto> add(MinecraftApi minecraftApi, List<OperatorDto> operators, ClientInfo clientInfo) {
        for (OperatorDto dto : operators) {
            OfflinePlayer player = dto.player().resolve();
            if (player != null) {
                player.setOp(true);
            }
        }
        return get(minecraftApi);
    }

    public static List<OperatorDto> set(MinecraftApi minecraftApi, List<OperatorDto> operators, ClientInfo clientInfo) {
        clear(minecraftApi, clientInfo);
        return add(minecraftApi, operators, clientInfo);
    }

    public static record OperatorDto(PlayerDto player, Optional<PermissionLevel> permissionLevel, Optional<Boolean> bypassesPlayerLimit) {
        public static final MapCodec<OperatorDto> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(OperatorDto.class, "CODEC");

        public static OperatorDto from(ServerOpListEntry entry) {
            return new OperatorDto(PlayerDto.from(entry.getUser()), Optional.of(PermissionLevel.OWNERS), Optional.of(entry.getBypassesPlayerLimit()));
        }
    }

    static record Op(NameAndId user, Optional<PermissionLevel> permissionLevel, Optional<Boolean> bypassesPlayerLimit) {}
}
