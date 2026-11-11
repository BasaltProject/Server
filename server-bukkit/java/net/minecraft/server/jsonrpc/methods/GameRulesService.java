// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.world.level.gamerules.GameRule;
import org.bukkit.Bukkit;
import org.bukkit.World;

public class GameRulesService {
    private static World world() {
        List<World> worlds = Bukkit.getWorlds();
        return worlds.isEmpty() ? null : worlds.get(0);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static List<GameRuleUpdate<?>> get(MinecraftApi minecraftApi) {
        List<GameRuleUpdate<?>> result = new ArrayList<>();
        World world = world();
        for (GameRule<?> rule : (Iterable<GameRule<?>>) BuiltInRegistries.GAME_RULE) {
            Object value = value(world, rule);
            if (value != null) {
                result.add(new GameRuleUpdate(rule, value));
            }
        }
        return result;
    }

    @SuppressWarnings("deprecation")
    private static Object value(World world, GameRule<?> rule) {
        org.bukkit.GameRule<?> bukkit = org.bukkit.GameRule.getByName(rule.id());
        return world == null || bukkit == null ? null : world.getGameRuleValue(bukkit);
    }

    public static <T> GameRuleUpdate<T> getTypedRule(MinecraftApi minecraftApi, GameRule<T> gameRule, T value) {
        return new GameRuleUpdate<>(gameRule, value);
    }

    @SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
    public static <T> GameRuleUpdate<T> update(MinecraftApi minecraftApi, GameRuleUpdate<T> update, ClientInfo clientInfo) {
        org.bukkit.GameRule bukkit = org.bukkit.GameRule.getByName(update.gameRule().id());
        if (bukkit != null) {
            for (World world : Bukkit.getWorlds()) {
                world.setGameRule(bukkit, update.value());
            }
        }
        return update;
    }

    public static record GameRuleUpdate<T>(GameRule<T> gameRule, T value) {
        public static final Codec<GameRuleUpdate<?>> TYPED_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(GameRuleUpdate.class, "TYPED_CODEC");
        public static final Codec<GameRuleUpdate<?>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(GameRuleUpdate.class, "CODEC");
    }
}
