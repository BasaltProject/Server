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
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.UserBanListEntry;
import org.bukkit.BanEntry;
import org.bukkit.BanList;
import org.bukkit.Bukkit;
import org.bukkit.OfflinePlayer;

public class BanlistService {
    @SuppressWarnings("deprecation")
    private static BanList<?> list() {
        return Bukkit.getBanList(BanList.Type.PROFILE);
    }

    @SuppressWarnings({"deprecation", "rawtypes"})
    public static List<UserBanDto> get(MinecraftApi minecraftApi) {
        List<UserBanDto> result = new ArrayList<>();
        for (BanEntry entry : list().getBanEntries()) {
            OfflinePlayer player = Bukkit.getOfflinePlayer(String.valueOf(entry.getTarget()));
            result.add(new UserBanDto(new PlayerDto(Optional.of(player.getUniqueId()), Optional.ofNullable(player.getName())),
                Optional.ofNullable(entry.getReason()), Optional.ofNullable(entry.getSource()), instant(entry.getExpiration())));
        }
        return result;
    }

    static Optional<Instant> instant(Date date) {
        return Optional.ofNullable(date).map(Date::toInstant);
    }

    @SuppressWarnings("deprecation")
    public static List<UserBanDto> add(MinecraftApi minecraftApi, List<UserBanDto> bans, ClientInfo clientInfo) {
        for (UserBanDto ban : bans) {
            OfflinePlayer player = ban.player().resolve();
            if (player != null && player.getName() != null) {
                list().addBan(player.getName(), ban.reason().orElse(null), ban.expires().map(Date::from).orElse(null), ban.source().orElse(null));
            }
        }
        return get(minecraftApi);
    }

    @SuppressWarnings({"deprecation", "rawtypes"})
    public static List<UserBanDto> clear(MinecraftApi minecraftApi, ClientInfo clientInfo) {
        for (BanEntry entry : new ArrayList<>(list().getBanEntries())) {
            list().pardon(String.valueOf(entry.getTarget()));
        }
        return get(minecraftApi);
    }

    @SuppressWarnings("deprecation")
    public static List<UserBanDto> remove(MinecraftApi minecraftApi, List<PlayerDto> remove, ClientInfo clientInfo) {
        for (PlayerDto dto : remove) {
            OfflinePlayer player = dto.resolve();
            if (player != null && player.getName() != null) {
                list().pardon(player.getName());
            }
        }
        return get(minecraftApi);
    }

    public static List<UserBanDto> set(MinecraftApi minecraftApi, List<UserBanDto> bans, ClientInfo clientInfo) {
        clear(minecraftApi, clientInfo);
        return add(minecraftApi, bans, clientInfo);
    }

    public static record UserBanDto(PlayerDto player, Optional<String> reason, Optional<String> source, Optional<Instant> expires) {
        public static final MapCodec<UserBanDto> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(UserBanDto.class, "CODEC");

        public static UserBanDto from(UserBanListEntry entry) {
            NameAndId user = entry.getUser();
            return new UserBanDto(PlayerDto.from(user), Optional.ofNullable(entry.getReason()), Optional.ofNullable(entry.getSource()), instant(entry.getExpires()));
        }
    }

    static record UserBan(NameAndId player, String reason, String source, Optional<Instant> expires) {}
}
