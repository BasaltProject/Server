package org.basaltmc.bukkit.facade;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Constructor;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.level.Level;

/** Builds the species class of an entity type, falling back to the generic classes when it cannot be built. */
public final class Species {
    private static final Map<String, Constructor<?>> kConstructors = new HashMap<>();
    private static Map<String, String> classNames;
    private static final ThreadLocal<Boolean> kViewing = ThreadLocal.withInitial(() -> false);

    private Species() {}

    public static boolean viewing() {
        return kViewing.get();
    }

    private static synchronized Constructor<?> constructor(String key) {
        if (classNames == null) {
            classNames = new HashMap<>();
            try (InputStream in = Species.class.getResourceAsStream("/entity_classes.txt")) {
                if (in != null) {
                    BufferedReader reader = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8));
                    for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                        int eq = line.indexOf('=');
                        if (eq > 0) {
                            classNames.put(line.substring(0, eq), line.substring(eq + 1));
                        }
                    }
                }
            } catch (java.io.IOException failure) {
                classNames.clear();
            }
        }
        if (kConstructors.containsKey(key)) {
            return kConstructors.get(key);
        }
        Constructor<?> found = null;
        String name = classNames.get(key);
        if (name != null) {
            try {
                found = Class.forName(name).getDeclaredConstructor(EntityType.class, Level.class);
                found.setAccessible(true);
            } catch (ReflectiveOperationException | LinkageError failure) {
                found = null;
            }
        }
        kConstructors.put(key, found);
        return found;
    }

    /** Species object for an entity type, or null when no species class can be built for it. */
    public static Entity create(EntityType<?> type, Level level, boolean view) {
        Constructor<?> constructor = constructor(EntityType.getKey(type).toString());
        if (constructor == null) {
            return null;
        }
        boolean before = kViewing.get();
        kViewing.set(view);
        try {
            return (Entity) constructor.newInstance(type, level);
        } catch (ReflectiveOperationException | RuntimeException | LinkageError failure) {
            return null;
        } finally {
            kViewing.set(before);
        }
    }

    /** Species object bound to an existing native entity. */
    public static Entity view(ServerLevel level, int id, java.util.UUID uuid, EntityType<?> type) {
        Entity made = create(type, level, true);
        if (made != null) {
            made.rebindAsView(id, uuid);
        }
        return made;
    }
}
