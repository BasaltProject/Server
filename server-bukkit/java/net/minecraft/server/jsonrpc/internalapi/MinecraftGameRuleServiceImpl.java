// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.core.registries.BuiltInRegistries;
import java.util.stream.Stream;
import net.minecraft.server.jsonrpc.JsonRpcLogger;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.jsonrpc.methods.GameRulesService;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.world.level.gamerules.GameRule;
public class MinecraftGameRuleServiceImpl implements MinecraftGameRuleService {
    public MinecraftGameRuleServiceImpl(NotificationManager notificationManager, JsonRpcLogger jsonrpcLogger) {
        
    }
    public <T> GameRulesService.GameRuleUpdate<T> updateGameRule(GameRulesService.GameRuleUpdate<T> update, ClientInfo clientInfo) {
        return GameRulesService.update(null, update, clientInfo);
    }
    public <T> GameRulesService.GameRuleUpdate<T> getTypedRule(GameRule<T> gameRule, T value) {
        return new GameRulesService.GameRuleUpdate<>(gameRule, value);
    }
    public Stream<GameRule<?>> getAvailableGameRules() {
        return BuiltInRegistries.GAME_RULE.stream().map(rule -> (GameRule<?>) rule);
    }
    @SuppressWarnings({"unchecked", "deprecation", "removal"})
    public <T> T getRuleValue(GameRule<T> gameRule) {
        org.bukkit.GameRule<?> bukkit = org.bukkit.GameRule.getByName(gameRule.id());
        return bukkit == null || org.bukkit.Bukkit.getWorlds().isEmpty() ? null : (T) org.bukkit.Bukkit.getWorlds().get(0).getGameRuleValue(bukkit);
    }
}
