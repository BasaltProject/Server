// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.notifications;

import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import net.minecraft.server.dedicated.DedicatedServer;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.players.IpBanListEntry;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.ServerOpListEntry;
import net.minecraft.server.players.UserBanListEntry;
import net.minecraft.world.level.gamerules.GameRule;

public class NotificationManager implements NotificationService {
    private final List<NotificationService> services = new CopyOnWriteArrayList<>();
    private DedicatedServer server;

    public void registerService(NotificationService notificationService) {
        if (notificationService != this) {
            services.add(notificationService);
        }
    }

    public void setServer(DedicatedServer server) {
        this.server = server;
    }

    public DedicatedServer server() {
        return server;
    }

    @Override
    public void playerJoined(ServerPlayer player) {
        for (NotificationService service : services) {
            service.playerJoined(player);
        }
    }

    @Override
    public void playerLeft(ServerPlayer player) {
        for (NotificationService service : services) {
            service.playerLeft(player);
        }
    }

    @Override
    public void serverStarted() {
        for (NotificationService service : services) {
            service.serverStarted();
        }
    }

    @Override
    public void serverShuttingDown() {
        for (NotificationService service : services) {
            service.serverShuttingDown();
        }
    }

    @Override
    public void serverSaveStarted() {
        for (NotificationService service : services) {
            service.serverSaveStarted();
        }
    }

    @Override
    public void serverSaveCompleted() {
        for (NotificationService service : services) {
            service.serverSaveCompleted();
        }
    }

    @Override
    public void serverActivityOccured() {
        for (NotificationService service : services) {
            service.serverActivityOccured();
        }
    }

    @Override
    public void worldUpgradeStarted() {
        for (NotificationService service : services) {
            service.worldUpgradeStarted();
        }
    }

    @Override
    public void worldUpgradeProgress(float progressPercentage) {
        for (NotificationService service : services) {
            service.worldUpgradeProgress(progressPercentage);
        }
    }

    @Override
    public void worldUpgradeFinished() {
        for (NotificationService service : services) {
            service.worldUpgradeFinished();
        }
    }

    @Override
    public void worldUpgradeFailed(String reason) {
        for (NotificationService service : services) {
            service.worldUpgradeFailed(reason);
        }
    }

    @Override
    public void playerOped(ServerOpListEntry operator) {
        for (NotificationService service : services) {
            service.playerOped(operator);
        }
    }

    @Override
    public void playerDeoped(ServerOpListEntry operator) {
        for (NotificationService service : services) {
            service.playerDeoped(operator);
        }
    }

    @Override
    public void playerAddedToAllowlist(NameAndId player) {
        for (NotificationService service : services) {
            service.playerAddedToAllowlist(player);
        }
    }

    @Override
    public void playerRemovedFromAllowlist(NameAndId player) {
        for (NotificationService service : services) {
            service.playerRemovedFromAllowlist(player);
        }
    }

    @Override
    public void ipBanned(IpBanListEntry ban) {
        for (NotificationService service : services) {
            service.ipBanned(ban);
        }
    }

    @Override
    public void ipUnbanned(String ip) {
        for (NotificationService service : services) {
            service.ipUnbanned(ip);
        }
    }

    @Override
    public void playerBanned(UserBanListEntry ban) {
        for (NotificationService service : services) {
            service.playerBanned(ban);
        }
    }

    @Override
    public void playerUnbanned(NameAndId player) {
        for (NotificationService service : services) {
            service.playerUnbanned(player);
        }
    }

    @Override
    public <T> void onGameRuleChanged(GameRule<T> gameRule, T value) {
        for (NotificationService service : services) {
            service.onGameRuleChanged(gameRule, value);
        }
    }

    @Override
    public void statusHeartbeat() {
        for (NotificationService service : services) {
            service.statusHeartbeat();
        }
    }
}
