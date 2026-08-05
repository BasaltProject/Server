package org.basaltmc.bukkit.plugin;

import java.io.IOException;
import java.io.InputStream;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import org.bukkit.plugin.InvalidDescriptionException;
import org.bukkit.plugin.PluginDescriptionFile;
import org.yaml.snakeyaml.Yaml;

public final class PluginDescriptors {
    private PluginDescriptors() {}

    private static Map<String, Object> load(JarFile file, JarEntry entry) throws IOException, InvalidDescriptionException {
        try (InputStream stream = file.getInputStream(entry)) {
            Object parsed = new Yaml().load(stream);
            if (!(parsed instanceof Map<?, ?> map)) {
                throw new InvalidDescriptionException("The plugin description is not a map");
            }
            Map<String, Object> out = new LinkedHashMap<>();
            for (Map.Entry<?, ?> each : map.entrySet()) {
                out.put(String.valueOf(each.getKey()), each.getValue());
            }
            return out;
        }
    }

    public static PluginDescriptionFile describe(java.io.File jar) throws InvalidDescriptionException, IOException {
        try (JarFile file = new JarFile(jar)) {
            JarEntry spigot = file.getJarEntry("plugin.yml");
            JarEntry paper = file.getJarEntry("paper-plugin.yml");
            if (spigot == null && paper == null) {
                throw new InvalidDescriptionException("Jar does not contain plugin.yml");
            }
            Map<String, Object> map;
            if (spigot != null) {
                map = load(file, spigot);
                if (paper != null && !map.containsKey("depend") && !map.containsKey("softdepend")
                        && !map.containsKey("loadbefore")) {
                    translateDependencies(load(file, paper), map);
                }
            } else {
                map = translate(load(file, paper));
            }
            return new PluginDescriptionFile(new StringReader(new Yaml().dump(map)));
        }
    }

    /** paper-plugin.yml says what a dependency needs in a section of its own; plugin.yml says it in three lists. */
    static Map<String, Object> translate(Map<String, Object> paper) {
        Map<String, Object> out = new LinkedHashMap<>();
        for (String key : List.of("name", "version", "main", "description", "author", "authors", "website",
                "api-version", "prefix", "provides", "permissions", "contributors", "defaultPerm", "load")) {
            if (paper.containsKey(key)) {
                out.put(key, paper.get(key));
            }
        }
        translateDependencies(paper, out);
        return out;
    }

    private static void translateDependencies(Map<String, Object> paper, Map<String, Object> into) {
        Object dependencies = paper.get("dependencies");
        if (!(dependencies instanceof Map<?, ?> sections) || !(sections.get("server") instanceof Map<?, ?> server)) {
            return;
        }
        List<String> depend = new ArrayList<>();
        List<String> soft = new ArrayList<>();
        List<String> before = new ArrayList<>();
        for (Map.Entry<?, ?> each : server.entrySet()) {
            String name = String.valueOf(each.getKey());
            Map<?, ?> options = each.getValue() instanceof Map<?, ?> map ? map : Map.of();
            boolean required = !Boolean.FALSE.equals(options.get("required"));
            String load = String.valueOf(options.containsKey("load") ? options.get("load") : "OMIT");
            if (load.equalsIgnoreCase("BEFORE")) {
                before.add(name);
            } else if (required) {
                depend.add(name);
            } else {
                soft.add(name);
            }
        }
        if (!depend.isEmpty()) {
            into.put("depend", depend);
        }
        if (!soft.isEmpty()) {
            into.put("softdepend", soft);
        }
        if (!before.isEmpty()) {
            into.put("loadbefore", before);
        }
    }
}
