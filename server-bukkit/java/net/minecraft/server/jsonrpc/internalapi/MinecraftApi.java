// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.server.jsonrpc.JsonRpcLogger;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;
import net.minecraft.server.notifications.NotificationManager;
public class MinecraftApi {
    private final NotificationManager notifications;
    private final MinecraftAllowListService allowList;
    private final MinecraftBanListService banList;
    private final MinecraftPlayerListService playerList;
    private final MinecraftGameRuleService gameRules;
    private final MinecraftOperatorListService operators;
    private final MinecraftServerSettingsService settings;
    private final MinecraftServerStateService state;
    private final MinecraftExecutorService executor;
    public MinecraftApi(NotificationManager notificationManager, MinecraftAllowListService allowListService, MinecraftBanListService banListService, MinecraftPlayerListService minecraftPlayerListService, MinecraftGameRuleService gameRuleService, MinecraftOperatorListService minecraftOperatorListService, MinecraftServerSettingsService minecraftServerSettingsService, MinecraftServerStateService minecraftServerStateService, MinecraftExecutorService executorService) {
        this.notifications = notificationManager;
        this.allowList = allowListService;
        this.banList = banListService;
        this.playerList = minecraftPlayerListService;
        this.gameRules = gameRuleService;
        this.operators = minecraftOperatorListService;
        this.settings = minecraftServerSettingsService;
        this.state = minecraftServerStateService;
        this.executor = executorService;
    }
    public <V> CompletableFuture<V> submit(Supplier<V> supplier) {
        return executor.submit(supplier);
    }
    public CompletableFuture<Void> submit(Runnable runnable) {
        return executor.submit(runnable);
    }
    public MinecraftAllowListService allowListService() {
        return allowList;
    }
    public MinecraftBanListService banListService() {
        return banList;
    }
    public MinecraftPlayerListService playerListService() {
        return playerList;
    }
    public MinecraftGameRuleService gameRuleService() {
        return gameRules;
    }
    public MinecraftOperatorListService operatorListService() {
        return operators;
    }
    public MinecraftServerSettingsService serverSettingsService() {
        return settings;
    }
    public MinecraftServerStateService serverStateService() {
        return state;
    }
    public NotificationManager notificationManager() {
        return notifications;
    }
    public static MinecraftApi of(NotificationManager notificationManager) {
        JsonRpcLogger logger = new JsonRpcLogger();
        return new MinecraftApi(notificationManager, new MinecraftAllowListServiceImpl(notificationManager, logger),
            new MinecraftBanListServiceImpl(notificationManager, logger), new MinecraftPlayerListServiceImpl(notificationManager, logger),
            new MinecraftGameRuleServiceImpl(notificationManager, logger), new MinecraftOperatorListServiceImpl(notificationManager, logger),
            new MinecraftServerSettingsServiceImpl(notificationManager, logger), new MinecraftServerStateServiceImpl(notificationManager, logger),
            new MinecraftExecutorServiceImpl(notificationManager));
    }
}
