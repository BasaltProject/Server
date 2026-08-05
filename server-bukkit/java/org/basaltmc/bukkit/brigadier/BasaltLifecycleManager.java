package org.basaltmc.bukkit.brigadier;

import io.papermc.paper.command.brigadier.Commands;
import io.papermc.paper.plugin.lifecycle.event.LifecycleEventManager;
import io.papermc.paper.plugin.lifecycle.event.handler.configuration.LifecycleEventHandlerConfiguration;
import io.papermc.paper.plugin.lifecycle.event.registrar.ReloadableRegistrarEvent;
import io.papermc.paper.plugin.lifecycle.event.types.BasaltLifecycleEventTypeProvider;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.Natives;
import org.bukkit.plugin.Plugin;

/** Runs the handlers a plugin registers for the events this server has: commands are registered as soon as asked. */
public final class BasaltLifecycleManager implements LifecycleEventManager<Plugin> {
    private static final Set<String> kReported = ConcurrentHashMap.newKeySet();

    private final Plugin plugin;

    public BasaltLifecycleManager(Plugin plugin) {
        this.plugin = plugin;
    }

    @Override
    public void registerEventHandler(LifecycleEventHandlerConfiguration<? super Plugin> configuration) {
        if ((Object) configuration instanceof io.papermc.paper.registry.event.BasaltRegistryEventTypeProvider.Recorded<?> unused) {
            reportUnused(unused.event());
            return;
        }
        if (!(configuration instanceof BasaltLifecycleEventTypeProvider.Handler<?> handler)) {
            throw new IllegalArgumentException("Not a handler this server made: " + configuration);
        }
        if (!handler.event().equals("commands")) {
            reportUnused(handler.event());
            return;
        }
        runCommands(plugin, handler);
    }

    public static void reportUnused(String event) {
        if (kReported.add(event)) {
            Natives.log(2, "Bukkit", "The lifecycle event '" + event + "' is not run by Basalt");
        }
    }

    public static void runCommands(Plugin plugin, BasaltLifecycleEventTypeProvider.Handler<?> handler) {
        Commands registrar = new BasaltCommands(plugin);
        handler.<ReloadableRegistrarEvent<Commands>>handler().run(new ReloadableRegistrarEvent<>() {
            @Override
            public Commands registrar() {
                return registrar;
            }

            @Override
            public Cause cause() {
                return Cause.INITIAL;
            }
        });
    }
}
