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
    private static final class Lifecycle extends LifecycleEventManagerBase {
        @Override
        public void registerEventHandler(
                io.papermc.paper.plugin.lifecycle.event.handler.configuration.LifecycleEventHandlerConfiguration config) {
            Unsupported.hit("LifecycleEventManager#registerEventHandler");
        }
    }

    @Override
    public LifecycleEventManager<Plugin> createPluginLifecycleEventManager(JavaPlugin plugin,
            BooleanSupplier registrationCheck) {
        return new Lifecycle();
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
    public ComponentFlattener componentFlattener() {
        return ComponentFlattener.basic();
    }
}
