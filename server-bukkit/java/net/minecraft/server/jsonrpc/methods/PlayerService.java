// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.server.jsonrpc.api.PlayerDto;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
public class PlayerService {
    public static List<PlayerDto> get(MinecraftApi minecraftApi) {
        List<PlayerDto> result = new java.util.ArrayList<>();
        for (org.bukkit.entity.Player player : org.bukkit.Bukkit.getOnlinePlayers()) {
            result.add(new PlayerDto(Optional.of(player.getUniqueId()), Optional.of(player.getName())));
        }
        return result;
    }
    public static List<PlayerDto> kick(MinecraftApi minecraftApi, List<KickDto> kick, ClientInfo clientInfo) {
        List<PlayerDto> kicked = new java.util.ArrayList<>();
        for (KickDto entry : kick) {
            org.bukkit.OfflinePlayer offline = entry.player().resolve();
            org.bukkit.entity.Player online = offline == null ? null : offline.getPlayer();
            if (online == null) {
                continue;
            }
            net.minecraft.network.chat.Component reason = entry.message().flatMap(Message::asComponent)
                .orElse(net.minecraft.network.chat.Component.translatable("multiplayer.disconnect.kicked"));
            online.kick(reason.asAdventure());
            kicked.add(entry.player());
        }
        return kicked;
    }
    public static record KickDto(PlayerDto player, Optional<Message> message) {
        public static final MapCodec<KickDto> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.server.jsonrpc.methods.PlayerService.KickDto.class, "CODEC");
    }
}
