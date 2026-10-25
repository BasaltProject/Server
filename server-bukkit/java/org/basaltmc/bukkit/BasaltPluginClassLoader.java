package org.basaltmc.bukkit;

import io.papermc.paper.plugin.configuration.PluginMeta;
import io.papermc.paper.plugin.provider.classloader.ConfiguredPluginClassLoader;
import io.papermc.paper.plugin.provider.classloader.PluginClassLoaderGroup;
import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.concurrent.ConcurrentHashMap;
import java.util.logging.Logger;
import org.bukkit.plugin.PluginDescriptionFile;
import org.bukkit.plugin.java.JavaPlugin;

/** One loader per plugin jar: its own classes first, then the plugins it depends on, then every other plugin. */
public final class BasaltPluginClassLoader extends URLClassLoader implements ConfiguredPluginClassLoader {
    static {
        registerAsParallelCapable();
    }

    private static final ConcurrentHashMap<String, String> classOwners = new ConcurrentHashMap<>();

    private final BasaltPluginManager manager;
    private final PluginDescriptionFile description;
    private final File dataFolder;
    private final File jar;
    private final ConcurrentHashMap<String, Class<?>> classes = new ConcurrentHashMap<>();
    private JavaPlugin plugin;

    public BasaltPluginClassLoader(ClassLoader parent, BasaltPluginManager manager, PluginDescriptionFile description,
            File dataFolder, File jar) throws IOException {
        super(jar.getName(), new URL[] {jar.toURI().toURL()}, parent);
        this.manager = manager;
        this.description = description;
        this.dataFolder = dataFolder;
        this.jar = jar;
    }

    public static String pluginOfClass(String className) {
        int lambda = className.indexOf("$$Lambda");
        return classOwners.get(lambda < 0 ? className : className.substring(0, lambda));
    }

    public void addLibraries(java.util.List<URL> libraries) {
        for (URL library : libraries) {
            addURL(library);
        }
    }

    public String pluginName() {
        return description.getName();
    }

    @Override
    protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
        return loadClass(name, resolve, true, true);
    }

    @Override
    public Class<?> loadClass(String name, boolean resolve, boolean checkGlobal, boolean checkLibraries)
            throws ClassNotFoundException {
        try {
            return super.loadClass(name, resolve);
        } catch (ClassNotFoundException ignored) {
            // Falls through to the other plugins.
        }
        if (checkGlobal) {
            Class<?> found = manager.findClass(name, this);
            if (found != null) {
                return found;
            }
        }
        throw new ClassNotFoundException(name);
    }

    @Override
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        Class<?> cached = classes.get(name);
        if (cached != null) {
            return cached;
        }
        Class<?> found = super.findClass(name);
        classes.put(name, found);
        classOwners.put(name, description.getName());
        return found;
    }

    Class<?> ownClass(String name) {
        Class<?> cached = classes.get(name);
        if (cached != null) {
            return cached;
        }
        try {
            return findClass(name);
        } catch (ClassNotFoundException e) {
            return null;
        }
    }

    public PluginDescriptionFile description() {
        return description;
    }

    public File jar() {
        return jar;
    }

    @Override
    public PluginMeta getConfiguration() {
        return description;
    }

    @Override
    public void init(JavaPlugin plugin) {
        if (this.plugin != null) {
            throw new IllegalStateException("Plugin already initialized: " + this.plugin);
        }
        this.plugin = plugin;
        Logger logger = Logger.getLogger(description.getName());
        plugin.init(BasaltServer.instance(), description, dataFolder, jar, this, description, logger);
    }

    @Override
    public JavaPlugin getPlugin() {
        return plugin;
    }

    @Override
    public PluginClassLoaderGroup getGroup() {
        return null;
    }
}
