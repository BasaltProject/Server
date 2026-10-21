package org.basaltmc.bukkit;

import java.io.File;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;
import org.bukkit.event.Event;
import org.bukkit.event.Listener;
import org.bukkit.plugin.InvalidDescriptionException;
import org.bukkit.plugin.InvalidPluginException;
import org.bukkit.plugin.Plugin;
import org.bukkit.plugin.PluginDescriptionFile;
import org.bukkit.plugin.PluginLoader;
import org.bukkit.plugin.RegisteredListener;
import org.bukkit.plugin.UnknownDependencyException;

/** What JavaPlugin#getPluginLoader answers: loading goes through the plugin manager, never through this. */
public final class DummyPluginLoader implements PluginLoader {
    @Override
    public Plugin loadPlugin(File file) throws InvalidPluginException, UnknownDependencyException {
        throw new UnsupportedOperationException();
    }

    @Override
    public PluginDescriptionFile getPluginDescription(File file) throws InvalidDescriptionException {
        throw new UnsupportedOperationException();
    }

    @Override
    public Pattern[] getPluginFileFilters() {
        return new Pattern[] {Pattern.compile("\\.jar$")};
    }

    @Override
    public Map<Class<? extends Event>, Set<RegisteredListener>> createRegisteredListeners(Listener listener,
            Plugin plugin) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void enablePlugin(Plugin plugin) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void disablePlugin(Plugin plugin) {
        throw new UnsupportedOperationException();
    }
}
