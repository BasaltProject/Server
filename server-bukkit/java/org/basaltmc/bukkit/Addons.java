package org.basaltmc.bukkit;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.jar.JarFile;
import java.util.zip.ZipEntry;

/** Jars in addons/ next to the runtime jar that implement a service provider interface some plugin expects the server */
final class Addons {
    private record Manifest(String id, File jar, String spi, String requires, String excludes, int priority,
            Map<String, String> properties) {}

    private static List<Manifest> manifests;

    private Addons() {}

    static void attach(BasaltPluginClassLoader loader) throws IOException {
        Manifest best = null;
        for (Manifest manifest : manifests()) {
            if (matches(loader, manifest) && (best == null || manifest.priority() > best.priority())) {
                best = manifest;
            }
        }
        if (best == null) {
            return;
        }
        loader.addAddon(best.jar());
        best.properties().forEach(System::setProperty);
        Natives.log(1, "Bukkit", "Attached the Basalt add-on " + best.id() + " to " + loader.description().getName());
    }

    private static boolean matches(BasaltPluginClassLoader loader, Manifest manifest) {
        if (!has(loader, manifest.spi())) {
            return false;
        }
        if (manifest.requires() != null && !has(loader, manifest.requires())) {
            return false;
        }
        return manifest.excludes() == null || !has(loader, manifest.excludes());
    }

    private static boolean has(BasaltPluginClassLoader loader, String className) {
        return loader.findResource(className.replace('.', '/') + ".class") != null;
    }

    private static synchronized List<Manifest> manifests() {
        if (manifests != null) {
            return manifests;
        }
        List<Manifest> found = new ArrayList<>();
        String first = System.getProperty("java.class.path", "").split(File.pathSeparator)[0];
        File[] jars = new File(new File(first).getAbsoluteFile().getParentFile(), "addons")
            .listFiles((dir, name) -> name.endsWith(".jar"));
        if (jars != null) {
            for (File jar : jars) {
                try (JarFile open = new JarFile(jar)) {
                    ZipEntry entry = open.getEntry("basalt-addon.properties");
                    if (entry == null) {
                        continue;
                    }
                    Properties read = new Properties();
                    try (InputStream stream = open.getInputStream(entry)) {
                        read.load(stream);
                    }
                    Map<String, String> system = new HashMap<>();
                    read.forEach((key, value) -> {
                        if (key.toString().startsWith("property.")) {
                            system.put(key.toString().substring("property.".length()), value.toString());
                        }
                    });
                    if (read.getProperty("spi") != null) {
                        found.add(new Manifest(read.getProperty("id", jar.getName()), jar, read.getProperty("spi"),
                            read.getProperty("requires"), read.getProperty("excludes"),
                            Integer.parseInt(read.getProperty("priority", "0")), system));
                    }
                } catch (IOException | NumberFormatException e) {
                    Natives.log(2, "Bukkit", "Ignoring the add-on " + jar.getName() + ": " + e);
                }
            }
        }
        found.sort(Comparator.comparing(Manifest::id));
        manifests = found;
        return found;
    }
}
