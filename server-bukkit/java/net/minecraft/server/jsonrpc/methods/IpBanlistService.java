// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;

import com.mojang.serialization.MapCodec;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Optional;
import net.minecraft.server.jsonrpc.api.PlayerDto;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.server.players.IpBanListEntry;
import org.bukkit.BanEntry;
import org.bukkit.BanList;
import org.bukkit.Bukkit;

public class IpBanlistService {
    @SuppressWarnings("deprecation")
    private static BanList<?> list() {
        return Bukkit.getBanList(BanList.Type.IP);
    }

    @SuppressWarnings({"deprecation", "rawtypes"})
    public static List<IpBanDto> get(MinecraftApi minecraftApi) {
        List<IpBanDto> result = new ArrayList<>();
        for (BanEntry entry : list().getBanEntries()) {
            result.add(new IpBanDto(String.valueOf(entry.getTarget()), Optional.ofNullable(entry.getReason()),
                Optional.ofNullable(entry.getSource()), BanlistService.instant(entry.getExpiration())));
        }
        return result;
    }

    @SuppressWarnings("deprecation")
    public static List<IpBanDto> add(MinecraftApi minecraftApi, List<IncomingIpBanDto> bans, ClientInfo clientInfo) {
        for (IncomingIpBanDto ban : bans) {
            String ip = ban.ip().orElse(null);
            if (ip == null && ban.player().isPresent()) {
                org.bukkit.OfflinePlayer player = ban.player().get().resolve();
                org.bukkit.entity.Player online = player == null ? null : player.getPlayer();
                if (online != null && online.getAddress() != null) {
                    ip = online.getAddress().getAddress().getHostAddress();
                }
            }
            if (ip != null) {
                list().addBan(ip, ban.reason().orElse(null), ban.expires().map(Date::from).orElse(null), ban.source().orElse(null));
            }
        }
        return get(minecraftApi);
    }

    @SuppressWarnings({"deprecation", "rawtypes"})
    public static List<IpBanDto> clear(MinecraftApi minecraftApi, ClientInfo clientInfo) {
        for (BanEntry entry : new ArrayList<>(list().getBanEntries())) {
            list().pardon(String.valueOf(entry.getTarget()));
        }
        return get(minecraftApi);
    }

    @SuppressWarnings("deprecation")
    public static List<IpBanDto> remove(MinecraftApi minecraftApi, List<String> ban, ClientInfo clientInfo) {
        for (String ip : ban) {
            list().pardon(ip);
        }
        return get(minecraftApi);
    }

    public static List<IpBanDto> set(MinecraftApi minecraftApi, List<IpBanDto> ips, ClientInfo clientInfo) {
        clear(minecraftApi, clientInfo);
        List<IncomingIpBanDto> incoming = new ArrayList<>();
        for (IpBanDto dto : ips) {
            incoming.add(new IncomingIpBanDto(Optional.empty(), Optional.of(dto.ip()), dto.reason(), dto.source(), dto.expires()));
        }
        return add(minecraftApi, incoming, clientInfo);
    }

    public static record IncomingIpBanDto(Optional<PlayerDto> player, Optional<String> ip, Optional<String> reason, Optional<String> source, Optional<Instant> expires) {
        public static final MapCodec<IncomingIpBanDto> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(IncomingIpBanDto.class, "CODEC");
    }

    public static record IpBanDto(String ip, Optional<String> reason, Optional<String> source, Optional<Instant> expires) {
        public static final MapCodec<IpBanDto> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(IpBanDto.class, "CODEC");

        public static IpBanDto from(IpBanListEntry ban) {
            return new IpBanDto(ban.getUser(), Optional.ofNullable(ban.getReason()), Optional.ofNullable(ban.getSource()), BanlistService.instant(ban.getExpires()));
        }
    }

    static record IpBan(String ip, String reason, String source, Optional<Instant> expires) {}
}
