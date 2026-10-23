package org.basaltmc.bukkit.plugin;

import io.papermc.paper.plugin.bootstrap.PluginProviderContext;
import io.papermc.paper.plugin.configuration.PluginMeta;
import io.papermc.paper.plugin.loader.PluginClasspathBuilder;
import io.papermc.paper.plugin.loader.PluginLoader;
import io.papermc.paper.plugin.loader.library.ClassPathLibrary;
import io.papermc.paper.plugin.loader.library.LibraryLoadingException;
import io.papermc.paper.plugin.loader.library.LibraryStore;
import io.papermc.paper.plugin.loader.library.impl.MavenLibraryResolver;
import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import net.kyori.adventure.text.logger.slf4j.ComponentLogger;
import org.basaltmc.bukkit.Natives;
import org.eclipse.aether.artifact.DefaultArtifact;
import org.eclipse.aether.graph.Dependency;
import org.eclipse.aether.repository.RemoteRepository;
import org.yaml.snakeyaml.Yaml;

/** The libraries a plugin asks for, in plugin.yml or through the loader class of paper-plugin.yml. */
public final class PluginLibraries {
    private PluginLibraries() {}

    /** Downloads (once, into libraries/) the plugin.yml libraries and returns the jars to put on its class path. */
    public static List<URL> forDescription(PluginMeta meta) {
        List<String> coordinates = meta instanceof org.bukkit.plugin.PluginDescriptionFile description
            ? description.getLibraries() : List.of();
        List<URL> urls = new ArrayList<>();
        if (coordinates.isEmpty()) {
            return urls;
        }
        MavenLibraryResolver resolver = new MavenLibraryResolver();
        resolver.addRepository(new RemoteRepository.Builder("central", "default",
            MavenLibraryResolver.MAVEN_CENTRAL_DEFAULT_MIRROR).build());
        for (String coordinate : coordinates) {
            resolver.addDependency(new Dependency(new DefaultArtifact(coordinate), null));
        }
        register(resolver, urls, meta.getName());
        return urls;
    }

    private static void register(ClassPathLibrary library, List<URL> into, String plugin) {
        LibraryStore store = path -> {
            try {
                into.add(path.toUri().toURL());
            } catch (MalformedURLException e) {
                throw new IllegalStateException(e);
            }
        };
        try {
            library.register(store);
        } catch (LibraryLoadingException e) {
            Natives.log(3, "Bukkit", "Could not load the libraries of " + plugin + ": " + e.getMessage());
        }
    }

    /** The loader class paper-plugin.yml names, or null. */
    public static String loaderClass(File jar) {
        try (JarFile file = new JarFile(jar)) {
            JarEntry entry = file.getJarEntry("paper-plugin.yml");
            if (entry == null) {
                return null;
            }
            try (var stream = file.getInputStream(entry)) {
                Object parsed = new Yaml().load(stream);
                return parsed instanceof java.util.Map<?, ?> map && map.get("loader") != null
                    ? String.valueOf(map.get("loader")) : null;
            }
        } catch (IOException | RuntimeException e) {
            return null;
        }
    }

    /** Runs the plugin's own PluginLoader against a builder that collects the libraries it adds. */
    public static List<URL> runLoader(String className, ClassLoader loader, PluginMeta meta, Path dataDirectory,
            Path source) {
        List<URL> urls = new ArrayList<>();
        try {
            PluginLoader pluginLoader = (PluginLoader) Class.forName(className, true, loader)
                .getDeclaredConstructor().newInstance();
            ComponentLogger logger = ComponentLogger.logger(meta.getName());
            PluginProviderContext context = new PluginProviderContext() {
                @Override
                public PluginMeta getConfiguration() {
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
            pluginLoader.classloader(new PluginClasspathBuilder() {
                @Override
                public PluginClasspathBuilder addLibrary(ClassPathLibrary library) {
                    register(library, urls, meta.getName());
                    return this;
                }

                @Override
                public PluginProviderContext getContext() {
                    return context;
                }
            });
        } catch (ReflectiveOperationException | RuntimeException | LinkageError e) {
            Natives.log(3, "Bukkit", "Could not run the plugin loader " + className + ": " + e);
        }
        return urls;
    }
}
