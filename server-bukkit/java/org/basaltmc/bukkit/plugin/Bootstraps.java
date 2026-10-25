package org.basaltmc.bukkit.plugin;

import io.papermc.paper.plugin.bootstrap.BootstrapContext;
import io.papermc.paper.plugin.bootstrap.PluginBootstrap;
import io.papermc.paper.plugin.configuration.PluginMeta;
import io.papermc.paper.plugin.lifecycle.event.LifecycleEventManager;
import io.papermc.paper.plugin.lifecycle.event.handler.configuration.LifecycleEventHandlerConfiguration;
import io.papermc.paper.plugin.lifecycle.event.types.BasaltLifecycleEventTypeProvider;
import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import net.kyori.adventure.text.logger.slf4j.ComponentLogger;
import org.basaltmc.bukkit.brigadier.BasaltLifecycleManager;
import org.bukkit.plugin.Plugin;
import org.bukkit.plugin.java.JavaPlugin;
import org.yaml.snakeyaml.Yaml;

/** Runs the bootstrapper class paper-plugin.yml names: it sees the plugin before the plugin exists. */
public final class Bootstraps {
    private Bootstraps() {}

    /** A bootstrapper that has run, with the command handlers it queued for when the plugin is made. */
    public static final class Started {
        private final PluginBootstrap bootstrap;
        private final BootstrapContext context;
        private final List<BasaltLifecycleEventTypeProvider.Handler<?>> commands;

        Started(PluginBootstrap bootstrap, BootstrapContext context,
                List<BasaltLifecycleEventTypeProvider.Handler<?>> commands) {
            this.bootstrap = bootstrap;
            this.context = context;
            this.commands = commands;
        }

        /** The plugin the bootstrapper makes, or null to let the caller build the main class itself. */
        public JavaPlugin createPlugin() {
            return bootstrap.createPlugin(context);
        }

        public void registerCommands(Plugin plugin) {
            for (BasaltLifecycleEventTypeProvider.Handler<?> handler : commands) {
                BasaltLifecycleManager.runCommands(plugin, handler);
            }
        }
    }

    public static String bootstrapperClass(File jar) {
        try (JarFile file = new JarFile(jar)) {
            JarEntry entry = file.getJarEntry("paper-plugin.yml");
            if (entry == null) {
                return null;
            }
            try (var stream = file.getInputStream(entry)) {
                Object parsed = new Yaml().load(stream);
                return parsed instanceof java.util.Map<?, ?> map && map.get("bootstrapper") != null
                    ? String.valueOf(map.get("bootstrapper")) : null;
            }
        } catch (IOException | RuntimeException e) {
            return null;
        }
    }

    public static Started start(String className, ClassLoader loader, PluginMeta meta, Path dataDirectory,
            Path source) throws ReflectiveOperationException {
        PluginBootstrap bootstrap = (PluginBootstrap) Class.forName(className, true, loader)
            .getDeclaredConstructor().newInstance();
        ComponentLogger logger = ComponentLogger.logger(meta.getName());
        List<BasaltLifecycleEventTypeProvider.Handler<?>> commands = new ArrayList<>();
        List<io.papermc.paper.registry.event.BasaltRegistryEventTypeProvider.Recorded<?>> registry = new ArrayList<>();
        LifecycleEventManager<BootstrapContext> manager = new LifecycleEventManager<>() {
            @Override
            public void registerEventHandler(LifecycleEventHandlerConfiguration<? super BootstrapContext> config) {
                if ((Object) config instanceof io.papermc.paper.registry.event.BasaltRegistryEventTypeProvider.Recorded<?> recorded) {
                    registry.add(recorded);
                    return;
                }
                if (!(config instanceof BasaltLifecycleEventTypeProvider.Handler<?> handler)) {
                    throw new IllegalArgumentException("Not a handler this server made: " + config);
                }
                if (handler.event().equals("commands")) {
                    commands.add(handler);
                } else {
                    BasaltLifecycleManager.reportUnused(handler.event());
                }
            }
        };
        BootstrapContext context = new BootstrapContext() {
            @Override
            public LifecycleEventManager<BootstrapContext> getLifecycleManager() {
                return manager;
            }

            @Override
            public PluginMeta getConfiguration() {
                return meta;
            }

            @Override
            public PluginMeta getPluginMeta() {
                return meta;
            }

            @Override
            public Path getDataDirectory() {
                return dataDirectory;
            }

            @Override
            public ComponentLogger getLogger() {
                return logger;
            }

            @Override
            public Path getPluginSource() {
                return source;
            }
        };
        bootstrap.bootstrap(context);
        org.basaltmc.bukkit.RegistryLifecycle.run(registry, meta.getName());
        return new Started(bootstrap, context, commands);
    }
}
