package org.basaltmc.bukkit.player;

import com.destroystokyo.paper.ClientOption;
import java.net.InetSocketAddress;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.resource.ResourcePackInfo;
import net.kyori.adventure.resource.ResourcePackRequest;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.BasaltServer;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.Text;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Statistic;
import org.bukkit.World;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.Player;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.MainHand;
import org.bukkit.plugin.Plugin;
public abstract class PlayerStats extends PlayerView {
    private UUID key() {
        return getUniqueId();
    }

    private static String itemKey(Material material) {
        return material.getKey().toString();
    }

    @Override
    public boolean hasCooldown(Material material) {
        return getCooldown(material) > 0;
    }

    @Override
    public boolean hasCooldown(ItemStack item) {
        return getCooldown(item) > 0;
    }

    @Override
    public int getCooldown(Material material) {
        return (int) PlayerCalls.count(key(), "cooldown", itemKey(material));
    }

    @Override
    public int getCooldown(ItemStack item) {
        return (int) PlayerCalls.count(key(), "cooldown", itemKey(item.getType()));
    }

    @Override
    public int getCooldown(Key key) {
        return (int) PlayerCalls.count(key(), "cooldown", key.asString());
    }

    @Override
    public void setCooldown(Material material, int ticks) {
        PlayerCalls.call(key(), "set_cooldown", itemKey(material), (long) ticks);
    }

    @Override
    public void setCooldown(ItemStack item, int ticks) {
        PlayerCalls.call(key(), "set_cooldown", itemKey(item.getType()), (long) ticks);
    }

    @Override
    public void setCooldown(Key key, int ticks) {
        PlayerCalls.call(key(), "set_cooldown", key.asString(), (long) ticks);
    }

    private int stat(String key) {
        return (int) PlayerCalls.count(key(), "statistic", key);
    }

    private void addStat(String key, int by) {
        PlayerCalls.call(key(), "inc_statistic", key, (long) by);
    }

    private void putStat(String key, int value) {
        PlayerCalls.call(key(), "set_statistic", key, (long) value);
    }

    private static void positive(int amount) {
        if (amount <= 0) {
            throw new IllegalArgumentException("Amount must be greater than 0");
        }
    }

    @Override
    public void incrementStatistic(Statistic statistic) {
        addStat(PlayerStatistics.untyped(statistic), 1);
    }

    @Override
    public void incrementStatistic(Statistic statistic, int amount) {
        positive(amount);
        addStat(PlayerStatistics.untyped(statistic), amount);
    }

    @Override
    public void incrementStatistic(Statistic statistic, Material material) {
        addStat(PlayerStatistics.of(statistic, material), 1);
    }

    @Override
    public void incrementStatistic(Statistic statistic, Material material, int amount) {
        positive(amount);
        addStat(PlayerStatistics.of(statistic, material), amount);
    }

    @Override
    public void incrementStatistic(Statistic statistic, EntityType entity) {
        addStat(PlayerStatistics.of(statistic, entity), 1);
    }

    @Override
    public void incrementStatistic(Statistic statistic, EntityType entity, int amount) {
        positive(amount);
        addStat(PlayerStatistics.of(statistic, entity), amount);
    }

    @Override
    public void decrementStatistic(Statistic statistic) {
        addStat(PlayerStatistics.untyped(statistic), -1);
    }

    @Override
    public void decrementStatistic(Statistic statistic, int amount) {
        positive(amount);
        addStat(PlayerStatistics.untyped(statistic), -amount);
    }

    @Override
    public void decrementStatistic(Statistic statistic, Material material) {
        addStat(PlayerStatistics.of(statistic, material), -1);
    }

    @Override
    public void decrementStatistic(Statistic statistic, Material material, int amount) {
        positive(amount);
        addStat(PlayerStatistics.of(statistic, material), -amount);
    }

    @Override
    public void decrementStatistic(Statistic statistic, EntityType entity) {
        addStat(PlayerStatistics.of(statistic, entity), -1);
    }

    @Override
    public void decrementStatistic(Statistic statistic, EntityType entity, int amount) {
        positive(amount);
        addStat(PlayerStatistics.of(statistic, entity), -amount);
    }

    @Override
    public int getStatistic(Statistic statistic) {
        return stat(PlayerStatistics.untyped(statistic));
    }

    @Override
    public int getStatistic(Statistic statistic, Material material) {
        return stat(PlayerStatistics.of(statistic, material));
    }

    @Override
    public int getStatistic(Statistic statistic, EntityType entity) {
        return stat(PlayerStatistics.of(statistic, entity));
    }

    @Override
    public void setStatistic(Statistic statistic, int value) {
        nonNegative(value);
        putStat(PlayerStatistics.untyped(statistic), value);
    }

    @Override
    public void setStatistic(Statistic statistic, Material material, int value) {
        nonNegative(value);
        putStat(PlayerStatistics.of(statistic, material), value);
    }

    @Override
    public void setStatistic(Statistic statistic, EntityType entity, int value) {
        nonNegative(value);
        putStat(PlayerStatistics.of(statistic, entity), value);
    }

    private static void nonNegative(int value) {
        if (value < 0) {
            throw new IllegalArgumentException("Value must be greater than or equal to 0");
        }
    }

    private static String[] keys(Collection<NamespacedKey> recipes) {
        List<String> out = new ArrayList<>();
        for (NamespacedKey recipe : recipes) {
            out.add(recipe.toString());
        }
        return out.toArray(new String[0]);
    }

    @Override
    public int discoverRecipes(Collection<NamespacedKey> recipes) {
        return (int) PlayerCalls.count(key(), "discover_recipes", (Object) keys(recipes));
    }

    @Override
    public int undiscoverRecipes(Collection<NamespacedKey> recipes) {
        return (int) PlayerCalls.count(key(), "undiscover_recipes", (Object) keys(recipes));
    }

    @Override
    public boolean hasDiscoveredRecipe(NamespacedKey recipe) {
        return PlayerCalls.done(key(), "has_recipe", recipe.toString());
    }

    @Override
    public Set<NamespacedKey> getDiscoveredRecipes() {
        Set<NamespacedKey> out = new HashSet<>();
        if (PlayerCalls.call(key(), "known_recipes") instanceof String[] known) {
            for (String text : known) {
                NamespacedKey parsed = NamespacedKey.fromString(text);
                if (parsed != null) {
                    out.add(parsed);
                }
            }
        }
        return out;
    }
}
