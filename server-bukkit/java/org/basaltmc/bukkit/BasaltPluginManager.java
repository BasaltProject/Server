package org.basaltmc.bukkit;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.logging.Level;
import org.bukkit.event.Event;
import org.bukkit.event.EventHandler;
import org.bukkit.event.EventPriority;
import org.bukkit.event.HandlerList;
import org.bukkit.event.Listener;
import org.bukkit.event.server.PluginDisableEvent;
import org.bukkit.event.server.PluginEnableEvent;
import org.bukkit.permissions.Permissible;
import org.bukkit.permissions.Permission;
import org.bukkit.permissions.PermissionDefault;
import org.bukkit.plugin.EventExecutor;
import org.bukkit.plugin.IllegalPluginAccessException;
import org.bukkit.plugin.InvalidDescriptionException;
import org.bukkit.plugin.InvalidPluginException;
import org.bukkit.plugin.Plugin;
import org.bukkit.plugin.PluginDescriptionFile;
import org.bukkit.plugin.PluginLoadOrder;
import org.bukkit.plugin.RegisteredListener;
import org.bukkit.plugin.UnknownDependencyException;
import org.bukkit.plugin.java.JavaPlugin;
import org.bukkit.plugin.java.PluginSwitch;
import org.basaltmc.bukkit.gen.PluginManagerBase;

public final class BasaltPluginManager extends PluginManagerBase {
    private final BasaltServer server;
    /** Read by name through reflection, as Spigot plugins have always done. */
    private org.bukkit.command.CommandMap commandMap;
    private final File folder;
    private final List<Plugin> plugins = new ArrayList<>();
    private final Map<String, Plugin> byName = new HashMap<>();
    private final List<BasaltPluginClassLoader> loaders = new ArrayList<>();
    private final Map<String, Permission> permissions = new LinkedHashMap<>();
    private final Map<Boolean, Set<Permission>> defaults = new HashMap<>();
    private final Map<String, Set<Permissible>> subscriptions = new HashMap<>();
    private final Map<Boolean, Set<Permissible>> defaultSubscriptions = new HashMap<>();

    BasaltPluginManager(BasaltServer server, File folder) {
        this.server = server;
        this.folder = folder;
        defaults.put(true, new HashSet<>());
        defaults.put(false, new HashSet<>());
    }

    void commandMap(org.bukkit.command.CommandMap map) {
        this.commandMap = map;
    }

    File folder() {
        return folder;
    }

    private record Candidate(File file, PluginDescriptionFile description) {}

    static PluginDescriptionFile describe(File jar) throws InvalidDescriptionException, IOException {
        try (JarFile file = new JarFile(jar)) {
            JarEntry entry = file.getJarEntry("plugin.yml");
            if (entry == null) {
                entry = file.getJarEntry("paper-plugin.yml");
            }
            if (entry == null) {
                throw new InvalidDescriptionException("Jar does not contain plugin.yml");
            }
            try (InputStream stream = file.getInputStream(entry)) {
                return new PluginDescriptionFile(stream);
            }
        }
    }

    @Override
    public Plugin[] loadPlugins(File directory) {
        List<Candidate> pending = new ArrayList<>();
        File[] files = directory.listFiles((dir, name) -> name.endsWith(".jar"));
        if (files == null) {
            return new Plugin[0];
        }
        Arrays.sort(files);
        Set<String> names = new HashSet<>();
        for (File file : files) {
            try {
                PluginDescriptionFile description = describe(file);
                if (!names.add(description.getName().toLowerCase(Locale.ROOT))) {
                    Natives.log(3, "Bukkit", "Ambiguous plugin name '" + description.getName() + "' in " + file.getName());
                    continue;
                }
                pending.add(new Candidate(file, description));
            } catch (InvalidDescriptionException e) {
                if (e.getMessage() == null || !e.getMessage().contains("does not contain")) {
                    Natives.log(3, "Bukkit", "Could not read " + file.getName() + ": " + e.getMessage());
                }
            } catch (Throwable t) {
                Natives.log(3, "Bukkit", "Could not read " + file.getName() + ": " + t.getMessage());
            }
        }
        List<Plugin> loaded = new ArrayList<>();
        Set<String> done = new HashSet<>();
        boolean progressed = true;
        while (!pending.isEmpty() && progressed) {
            progressed = false;
            for (Candidate candidate : new ArrayList<>(pending)) {
                if (!ready(candidate, pending, done)) {
                    continue;
                }
                pending.remove(candidate);
                progressed = true;
                done.add(candidate.description().getName().toLowerCase(Locale.ROOT));
                try {
                    Plugin plugin = loadPlugin(candidate.file());
                    if (plugin != null) {
                        loaded.add(plugin);
                    }
                } catch (Throwable t) {
                    Natives.log(3, "Bukkit", "Could not load " + candidate.file().getName() + ": " + t);
                    t.printStackTrace();
                }
            }
        }
        for (Candidate stuck : pending) {
            Natives.log(3, "Bukkit", "Could not load " + stuck.file().getName() + ": missing or circular dependency");
        }
        return loaded.toArray(new Plugin[0]);
    }

    private boolean ready(Candidate candidate, List<Candidate> pending, Set<String> done) {
        PluginDescriptionFile description = candidate.description();
        for (String name : description.getDepend()) {
            if (!done.contains(name.toLowerCase(Locale.ROOT))) {
                boolean exists = pending.stream().anyMatch(other -> providesName(other.description(), name));
                if (!exists) {
                    Natives.log(3, "Bukkit", "Could not load " + candidate.file().getName() + ": unknown dependency " + name);
                    done.add(description.getName().toLowerCase(Locale.ROOT));
                    pending.remove(candidate);
                    return false;
                }
                return false;
            }
        }
        for (String name : description.getSoftDepend()) {
            boolean waiting = pending.stream().anyMatch(other -> other != candidate && providesName(other.description(), name));
            if (waiting && !done.contains(name.toLowerCase(Locale.ROOT))) {
                return false;
            }
        }
        for (Candidate other : pending) {
            if (other != candidate && other.description().getLoadBefore().stream()
                    .anyMatch(name -> name.equalsIgnoreCase(description.getName()))) {
                return false;
            }
        }
        return true;
    }

    private static boolean providesName(PluginDescriptionFile description, String name) {
        return description.getName().equalsIgnoreCase(name)
            || description.getProvides().stream().anyMatch(name::equalsIgnoreCase);
    }

    @Override
    public Plugin loadPlugin(File file)
            throws InvalidPluginException, InvalidDescriptionException, UnknownDependencyException {
        PluginDescriptionFile description;
        try {
            description = describe(file);
        } catch (IOException e) {
            throw new InvalidPluginException(e);
        }
        for (String dependency : description.getDepend()) {
            if (getPlugin(dependency) == null) {
                throw new UnknownDependencyException(dependency);
            }
        }
        File dataFolder = new File(file.getParentFile(), description.getName());
        BasaltPluginClassLoader loader;
        JavaPlugin plugin;
        try {
            loader = new BasaltPluginClassLoader(getClass().getClassLoader(), this, description, dataFolder, file);
            Addons.attach(loader);
            Class<?> main = Class.forName(description.getMainClass(), true, loader);
            if (!JavaPlugin.class.isAssignableFrom(main)) {
                throw new InvalidPluginException("main class '" + description.getMainClass() + "' does not extend JavaPlugin");
            }
            plugin = (JavaPlugin) Addons.scoped(loader, () -> main.getDeclaredConstructor().newInstance());
        } catch (InvocationTargetException e) {
            throw new InvalidPluginException(e.getCause());
        } catch (ReflectiveOperationException | IOException | LinkageError e) {
            throw new InvalidPluginException(e);
        } catch (Throwable e) {
            throw new InvalidPluginException(e);
        }
        loaders.add(loader);
        plugins.add(plugin);
        byName.put(description.getName().toLowerCase(Locale.ROOT), plugin);
        for (String provided : description.getProvides()) {
            byName.putIfAbsent(provided.toLowerCase(Locale.ROOT), plugin);
        }
        addPermissions(description.getPermissions());
        plugin.getLogger().info("Loading " + description.getFullName());
        try {
            Addons.scoped(plugin.getClass().getClassLoader(), () -> {
                plugin.onLoad();
                return null;
            });
        } catch (Throwable t) {
            plugin.getLogger().log(Level.SEVERE, "Error occurred in onLoad: " + t.getMessage(), t);
        }
        return plugin;
    }

    Class<?> findClass(String name, BasaltPluginClassLoader requester) {
        for (BasaltPluginClassLoader loader : loaders) {
            if (loader == requester) {
                continue;
            }
            Class<?> found = loader.ownClass(name);
            if (found != null) {
                return found;
            }
        }
        return null;
    }

    void enableAll(PluginLoadOrder order) {
        for (Plugin plugin : new ArrayList<>(plugins)) {
            if (!plugin.isEnabled() && plugin.getDescription().getLoad() == order) {
                enablePlugin(plugin);
            }
        }
    }

    @Override
    public Plugin getPlugin(String name) {
        return byName.get(name.replace(' ', '_').toLowerCase(Locale.ROOT));
    }

    @Override
    public Plugin[] getPlugins() {
        return plugins.toArray(new Plugin[0]);
    }

    @Override
    public boolean isPluginEnabled(String name) {
        return isPluginEnabled(getPlugin(name));
    }

    @Override
    public boolean isPluginEnabled(Plugin plugin) {
        return plugin != null && plugins.contains(plugin) && plugin.isEnabled();
    }

    @Override
    public void enablePlugin(Plugin plugin) {
        if (plugin.isEnabled()) {
            return;
        }
        List<org.bukkit.command.Command> commands = org.bukkit.command.PluginCommandYamlParser.parse(plugin);
        if (!commands.isEmpty()) {
            server.commandMapImpl().registerAll(plugin.getDescription().getName(), commands);
        }
        try {
            Addons.scoped(plugin.getClass().getClassLoader(), () -> {
                PluginSwitch.enable((JavaPlugin) plugin);
                return null;
            });
        } catch (Throwable t) {
            plugin.getLogger().log(Level.SEVERE,
                "Error occurred while enabling " + plugin.getDescription().getFullName() + " (Is it up to date?)", t);
            disablePlugin(plugin);
            return;
        }
        if (plugin.isEnabled()) {
            callEvent(new PluginEnableEvent(plugin));
        }
    }

    @Override
    public void disablePlugin(Plugin plugin) {
        if (plugin.isEnabled()) {
            try {
                Natives.log(1, "Bukkit", "Disabling " + plugin.getDescription().getFullName());
                callEvent(new PluginDisableEvent(plugin));
                PluginSwitch.disable((JavaPlugin) plugin);
            } catch (Throwable t) {
                plugin.getLogger().log(Level.SEVERE,
                    "Error occurred while disabling " + plugin.getDescription().getFullName(), t);
            }
        }
        server.schedulerImpl().cancelTasks(plugin);
        server.getServicesManager().unregisterAll(plugin);
        HandlerList.unregisterAll(plugin);
        server.getMessenger().unregisterIncomingPluginChannel(plugin);
        server.getMessenger().unregisterOutgoingPluginChannel(plugin);
        server.commandMapImpl().unregisterPlugin(plugin);
    }

    @Override
    public void disablePlugins() {
        List<Plugin> reversed = new ArrayList<>(plugins);
        Collections.reverse(reversed);
        for (Plugin plugin : reversed) {
            disablePlugin(plugin);
        }
    }

    @Override
    public void clearPlugins() {
        disablePlugins();
        plugins.clear();
        byName.clear();
        loaders.clear();
        HandlerList.unregisterAll();
    }

    @Override
    public void registerEvents(Listener listener, Plugin plugin) {
        if (!plugin.isEnabled()) {
            throw new IllegalPluginAccessException("Plugin attempted to register " + listener + " while not enabled");
        }
        Set<Method> methods = new HashSet<>();
        try {
            methods.addAll(Arrays.asList(listener.getClass().getMethods()));
            methods.addAll(Arrays.asList(listener.getClass().getDeclaredMethods()));
        } catch (NoClassDefFoundError e) {
            plugin.getLogger().severe("Plugin " + plugin.getDescription().getFullName()
                + " has failed to register events for " + listener.getClass() + " because " + e.getMessage()
                + " does not exist.");
            return;
        }
        for (Method method : methods) {
            EventHandler handler = method.getAnnotation(EventHandler.class);
            if (handler == null || method.isBridge() || method.isSynthetic()) {
                continue;
            }
            Class<?> parameter;
            if (method.getParameterCount() != 1 || !Event.class.isAssignableFrom(parameter = method.getParameterTypes()[0])) {
                plugin.getLogger().severe(plugin.getDescription().getFullName() + " attempted to register an invalid EventHandler method signature \""
                    + method.toGenericString() + "\" in " + listener.getClass());
                continue;
            }
            Class<? extends Event> eventClass = parameter.asSubclass(Event.class);
            method.setAccessible(true);
            EventExecutor executor = (target, event) -> {
                try {
                    if (!eventClass.isAssignableFrom(event.getClass())) {
                        return;
                    }
                    method.invoke(target, event);
                } catch (InvocationTargetException e) {
                    throw new org.bukkit.event.EventException(e.getCause());
                } catch (Throwable t) {
                    throw new org.bukkit.event.EventException(t);
                }
            };
            registerEvent(eventClass, listener, handler.priority(), executor, plugin, handler.ignoreCancelled());
        }
    }

    @Override
    public void registerEvent(Class<? extends Event> event, Listener listener, EventPriority priority,
            EventExecutor executor, Plugin plugin) {
        registerEvent(event, listener, priority, executor, plugin, false);
    }

    @Override
    public void registerEvent(Class<? extends Event> event, Listener listener, EventPriority priority,
            EventExecutor executor, Plugin plugin, boolean ignoreCancelled) {
        if (!plugin.isEnabled()) {
            throw new IllegalPluginAccessException("Plugin attempted to register " + event + " while not enabled");
        }
        handlerList(event).register(new RegisteredListener(listener, executor, priority, plugin, ignoreCancelled));
        EventBridge.listening(event, priority);
    }

    private static HandlerList handlerList(Class<? extends Event> type) {
        try {
            Class<?> current = type;
            while (current != null && Event.class.isAssignableFrom(current)) {
                try {
                    Method method = current.getDeclaredMethod("getHandlerList");
                    method.setAccessible(true);
                    return (HandlerList) method.invoke(null);
                } catch (NoSuchMethodException ignored) {
                    current = current.getSuperclass();
                }
            }
        } catch (ReflectiveOperationException e) {
            throw new IllegalPluginAccessException(e.toString());
        }
        throw new IllegalPluginAccessException("Unable to find handler list for event " + type.getName()
            + ". Static getHandlerList method required!");
    }

    @Override
    public void callEvent(Event event) {
        for (RegisteredListener registration : event.getHandlers().getRegisteredListeners()) {
            if (!registration.getPlugin().isEnabled()) {
                continue;
            }
            try {
                registration.callEvent(event);
            } catch (Throwable t) {
                server.getLogger().log(Level.SEVERE, "Could not pass event " + event.getEventName() + " to "
                    + registration.getPlugin().getDescription().getFullName(), t);
            }
        }
    }

    void callAt(Event event, EventPriority priority) {
        for (RegisteredListener registration : event.getHandlers().getRegisteredListeners()) {
            if (registration.getPriority() != priority || !registration.getPlugin().isEnabled()) {
                continue;
            }
            try {
                registration.callEvent(event);
            } catch (Throwable t) {
                server.getLogger().log(Level.SEVERE, "Could not pass event " + event.getEventName() + " to "
                    + registration.getPlugin().getDescription().getFullName(), t);
            }
        }
    }

    @Override
    public void registerInterface(Class<? extends org.bukkit.plugin.PluginLoader> loader) {
        throw new IllegalArgumentException("Basalt loads Java plugins itself");
    }

    @Override
    public Plugin[] loadPlugins(File[] files) {
        throw Unsupported.hit("PluginManager#loadPlugins(File[])");
    }

    @Override
    public Permission getPermission(String name) {
        return permissions.get(name.toLowerCase(Locale.ROOT));
    }

    @Override
    public void addPermission(Permission permission) {
        String key = permission.getName().toLowerCase(Locale.ROOT);
        if (permissions.containsKey(key)) {
            return;
        }
        permissions.put(key, permission);
        calculateDefault(permission);
    }

    private void calculateDefault(Permission permission) {
        PermissionDefault value = permission.getDefault();
        defaults.get(true).remove(permission);
        defaults.get(false).remove(permission);
        if (value == PermissionDefault.TRUE || value == PermissionDefault.OP) {
            defaults.get(true).add(permission);
        }
        if (value == PermissionDefault.TRUE || value == PermissionDefault.NOT_OP) {
            defaults.get(false).add(permission);
        }
    }

    @Override
    public void addPermissions(List<Permission> list) {
        for (Permission permission : list) {
            addPermission(permission);
        }
    }

    @Override
    public void removePermission(Permission permission) {
        removePermission(permission.getName());
    }

    @Override
    public void removePermission(String name) {
        Permission removed = permissions.remove(name.toLowerCase(Locale.ROOT));
        if (removed != null) {
            defaults.get(true).remove(removed);
            defaults.get(false).remove(removed);
        }
    }

    @Override
    public void clearPermissions() {
        permissions.clear();
        defaults.get(true).clear();
        defaults.get(false).clear();
    }

    @Override
    public Set<Permission> getDefaultPermissions(boolean op) {
        return Set.copyOf(defaults.get(op));
    }

    @Override
    public void recalculatePermissionDefaults(Permission permission) {
        if (permission != null && permissions.containsKey(permission.getName().toLowerCase(Locale.ROOT))) {
            calculateDefault(permission);
        }
    }

    @Override
    public Set<Permission> getPermissions() {
        return Set.copyOf(permissions.values());
    }

    @Override
    public void subscribeToPermission(String permission, Permissible permissible) {
        subscriptions.computeIfAbsent(permission.toLowerCase(Locale.ROOT), k -> new HashSet<>()).add(permissible);
    }

    @Override
    public void unsubscribeFromPermission(String permission, Permissible permissible) {
        Set<Permissible> set = subscriptions.get(permission.toLowerCase(Locale.ROOT));
        if (set != null) {
            set.remove(permissible);
        }
    }

    @Override
    public Set<Permissible> getPermissionSubscriptions(String permission) {
        return Set.copyOf(subscriptions.getOrDefault(permission.toLowerCase(Locale.ROOT), Set.of()));
    }

    @Override
    public void subscribeToDefaultPerms(boolean op, Permissible permissible) {
        defaultSubscriptions.computeIfAbsent(op, k -> new HashSet<>()).add(permissible);
    }

    @Override
    public void unsubscribeFromDefaultPerms(boolean op, Permissible permissible) {
        Set<Permissible> set = defaultSubscriptions.get(op);
        if (set != null) {
            set.remove(permissible);
        }
    }

    @Override
    public Set<Permissible> getDefaultPermSubscriptions(boolean op) {
        return Set.copyOf(defaultSubscriptions.getOrDefault(op, Set.of()));
    }

    @Override
    public boolean useTimings() {
        return false;
    }

    @Override
    public boolean isTransitiveDependency(io.papermc.paper.plugin.configuration.PluginMeta plugin,
            io.papermc.paper.plugin.configuration.PluginMeta depend) {
        return plugin.getPluginDependencies().contains(depend.getName())
            || plugin.getPluginSoftDependencies().contains(depend.getName());
    }

    boolean grantedByDefault(String permission, boolean op) {
        Permission known = getPermission(permission);
        if (known == null) {
            return false;
        }
        return known.getDefault().getValue(op);
    }
}
