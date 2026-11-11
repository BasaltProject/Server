// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.protocol.status.ServerStatus;
import net.minecraft.server.jsonrpc.api.PlayerDto;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
public class ServerStateService {
    public static ServerState status(MinecraftApi minecraftApi) {
        return new ServerState(true, PlayerService.get(minecraftApi), net.minecraft.network.protocol.status.ServerStatus.Version.current());
    }
    public static boolean save(MinecraftApi minecraftApi, boolean flush, ClientInfo clientInfo) {
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            world.save();
        }
        return true;
    }
    public static boolean stop(MinecraftApi minecraftApi, ClientInfo clientInfo) {
        org.bukkit.Bukkit.shutdown();
        return true;
    }
    public static boolean systemMessage(MinecraftApi minecraftApi, SystemMessage systemMessage, ClientInfo clientInfo) {
        net.minecraft.network.chat.Component text = systemMessage.message().asComponent().orElse(null);
        if (text == null) {
            return false;
        }
        java.util.Collection<? extends org.bukkit.entity.Player> targets = org.bukkit.Bukkit.getOnlinePlayers();
        if (systemMessage.receivingPlayers().isPresent()) {
            java.util.List<org.bukkit.entity.Player> chosen = new java.util.ArrayList<>();
            for (PlayerDto dto : systemMessage.receivingPlayers().get()) {
                org.bukkit.OfflinePlayer offline = dto.resolve();
                if (offline != null && offline.getPlayer() != null) {
                    chosen.add(offline.getPlayer());
                }
            }
            targets = chosen;
        }
        for (org.bukkit.entity.Player player : targets) {
            if (systemMessage.overlay()) {
                player.sendActionBar(text.asAdventure());
            } else {
                player.sendMessage(text.asAdventure());
            }
        }
        return true;
    }
    public static record ServerState(boolean started, List<PlayerDto> players, ServerStatus.Version version) {
        public static final Codec<ServerState> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.server.jsonrpc.methods.ServerStateService.ServerState.class, "CODEC");
        public static final ServerState NOT_STARTED = new ServerState(false, List.of(), net.minecraft.network.protocol.status.ServerStatus.Version.current());
    }
    public static record SystemMessage(Message message, boolean overlay, Optional<List<PlayerDto>> receivingPlayers) {
        public static final Codec<SystemMessage> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.server.jsonrpc.methods.ServerStateService.SystemMessage.class, "CODEC");
    }
}
