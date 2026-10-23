package org.basaltmc.bukkit;

import io.papermc.paper.plugin.lifecycle.event.LifecycleEventManager;
import java.util.function.BooleanSupplier;
import net.kyori.adventure.text.flattener.ComponentFlattener;
import org.bukkit.plugin.Plugin;
import org.bukkit.plugin.java.JavaPlugin;
import org.basaltmc.bukkit.gen.InternalAPIBridgeBase;
import org.basaltmc.bukkit.gen.LifecycleEventManagerBase;

/** The hooks Paper's API reaches the server through; what is not here reports itself once when used. */
public final class BasaltApiBridge extends InternalAPIBridgeBase {
    @Override
    public LifecycleEventManager<Plugin> createPluginLifecycleEventManager(JavaPlugin plugin,
            BooleanSupplier registrationCheck) {
        return new org.basaltmc.bukkit.brigadier.BasaltLifecycleManager(plugin);
    }

    @Override
    public <MODERN, LEGACY> org.bukkit.GameRule<LEGACY> legacyGameRuleBridge(org.bukkit.GameRule<MODERN> modern,
        java.util.function.Function<LEGACY, MODERN> toModern, java.util.function.Function<MODERN, LEGACY> fromModern,
        Class<LEGACY> type) {
        return new BasaltGameRule.Legacy<>(modern, toModern, fromModern, type);
    }

    @Override
    public io.papermc.paper.entity.poi.PoiType.Occupancy createOccupancy(String name) {
        return new io.papermc.paper.entity.poi.PoiType.Occupancy() {
            @Override
            public String toString() {
                return name;
            }
        };
    }

    @Override
    public org.bukkit.block.Biome constructLegacyCustomBiome() {
        return new BasaltBiome("minecraft:custom", -1);
    }

    @Override
    public org.bukkit.inventory.ItemStack createEmptyStack() {
        return new BasaltItemStack(org.bukkit.Material.AIR, 0);
    }

    @Override
    public org.bukkit.inventory.ItemStack deserializeItem(byte[] bytes) {
        return BasaltItemStack.restore(bytes);
    }

    @Override
    public org.bukkit.damage.DamageSource.Builder createDamageSourceBuilder(org.bukkit.damage.DamageType type) {
        return new BasaltDamageSource.Builder(type);
    }

    @Override
    public java.util.function.Predicate<io.papermc.paper.command.brigadier.CommandSourceStack> restricted(
            java.util.function.Predicate<io.papermc.paper.command.brigadier.CommandSourceStack> predicate,
            boolean restricted) {
        return predicate;
    }

    @Override
    public ComponentFlattener componentFlattener() {
        return ComponentFlattener.basic();
    }
}
