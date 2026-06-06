package org.basaltmc.bukkit;

import java.io.File;
import java.io.IOException;

/** Jars shipped with Basalt that teach one plugin about this server, found in addons/ next to the runtime jar. */
final class Addons {
    private Addons() {}

    static void attach(BasaltPluginClassLoader loader, String plugin) throws IOException {
        String first = System.getProperty("java.class.path", "").split(File.pathSeparator)[0];
        File parent = new File(first).getAbsoluteFile().getParentFile();
        File addon = new File(new File(parent, "addons"), plugin + ".jar");
        if (!addon.isFile()) {
            return;
        }
        loader.addAddon(addon);
        if ("FastAsyncWorldEdit".equals(plugin)) {
            System.setProperty("worldedit.bukkit.adapter", "org.basaltmc.bukkit.fawe.BasaltFaweAdapter");
        }
        Natives.log(1, "Bukkit", "Attached the Basalt addon " + addon.getName() + " to " + plugin);
    }
}
