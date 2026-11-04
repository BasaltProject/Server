package org.basaltmc.bukkit.fs;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Maps the paths a Bukkit plugin believes in (a Paper server directory) onto the Basalt layout.
 * Anything that leaves the server directory is denied; anything Basalt has no file for goes to a
 * shadow tree so the plugin keeps working without touching Basalt's own state.
 */
public final class PathTranslator {
    public enum Access { READ_WRITE, READ_ONLY, DENIED }

    public record Resolved(Path real, Access access, String rule) {
        public boolean allowed() {
            return access != Access.DENIED;
        }
    }

    public static final String SHADOW = ".bukkit-shadow";

    private static final Set<String> WORLD_STATE = Set.of("level.dat", "level.dat_old", "uid.dat", "session.lock",
        "playerdata", "stats", "advancements");
    private static final Set<String> PROTECTED = Set.of("players.db", "players.db-wal", "players.db-shm",
        "players.db-journal", "eula.toml", ".git", ".gitmodules");
    private static final Map<String, String> DIMENSION_FOLDERS = Map.of("DIM-1", "NETHER", "DIM1", "THE_END");
    private static final Map<String, String> DIMENSION_KEYS = Map.of("overworld", "NORMAL", "the_nether", "NETHER",
        "the_end", "THE_END");

    private record World(Path folder, String environment) { }

    private final Path root;
    private final Path shadow;
    private final Map<String, World> worlds = new LinkedHashMap<>();
    private final Map<Path, Access> outside = new LinkedHashMap<>();

    public PathTranslator(Path root) {
        this.root = root.toAbsolutePath().normalize();
        this.shadow = this.root.resolve(SHADOW);
        try {
            Files.createDirectories(shadow.resolve("root"));
            Files.createDirectories(shadow.resolve("tmp"));
        } catch (IOException ignored) {
            // Writes into the shadow then fail with the real cause.
        }
    }

    public Path shadow() {
        return shadow;
    }

    public Path root() {
        return root;
    }

    /** Lets the host expose a location outside the server directory (its own jars, /dev/null...). */
    public synchronized void allowOutside(Path location, Access access) {
        outside.put(location.toAbsolutePath().normalize(), access);
    }

    public synchronized void world(String name, Path folder, String environment) {
        worlds.put(name, new World(folder.toAbsolutePath().normalize(), environment));
        try {
            Files.createDirectories(shadow.resolve("worlds").resolve(name));
        } catch (IOException ignored) {
            // The first write through the shadow reports the real problem.
        }
    }

    public synchronized void forgetWorld(String name) {
        worlds.remove(name);
    }

    public synchronized Resolved resolve(Path requested) {
        Path absolute = (requested.isAbsolute() ? requested : root.resolve(requested)).normalize();
        if (!absolute.startsWith(root)) {
            for (Map.Entry<Path, Access> entry : outside.entrySet()) {
                if (absolute.startsWith(entry.getKey())) {
                    return new Resolved(absolute, entry.getValue(), "host-allowed");
                }
            }
            return denied("outside-server");
        }
        Resolved mapped = map(root.relativize(absolute));
        if (!mapped.allowed()) {
            return mapped;
        }
        if (!mapped.real().startsWith(root) || escapesThroughLink(mapped.real())) {
            return denied("outside-server");
        }
        return mapped;
    }

    private Resolved map(Path rel) {
        if (rel.toString().isEmpty()) {
            return new Resolved(root, Access.READ_ONLY, "server-root");
        }
        String head = rel.getName(0).toString();
        Path tail = tail(rel, 1);
        if (PROTECTED.contains(head.toLowerCase(Locale.ROOT))) {
            return denied("protected");
        }
        if (head.equals(".basalt")) {
            boolean runtime = rel.getNameCount() >= 2 && rel.getName(1).toString().equals("bukkit");
            return runtime ? new Resolved(root.resolve(rel), Access.READ_ONLY, "bukkit-runtime")
                : denied("protected");
        }
        switch (head) {
            case "plugins":
                return new Resolved(root.resolve(rel), Access.READ_WRITE, "plugins");
            case "generated":
                return new Resolved(root.resolve(rel), Access.READ_WRITE, "generated");
            case SHADOW:
                return new Resolved(root.resolve(rel), Access.READ_WRITE, "shadow");
            case "logs":
                return new Resolved(root.resolve(rel), Access.READ_ONLY, "logs");
            case "server-icon.png":
                return new Resolved(root.resolve(rel), Access.READ_ONLY, "server-icon");
            case "worlds":
                return nativeWorlds(rel);
            default:
                break;
        }
        World world = worlds.get(head);
        if (world != null) {
            return inWorld(head, world, tail);
        }
        return shadowed(rel, "shadow-root");
    }

    private Resolved nativeWorlds(Path rel) {
        if (rel.getNameCount() == 1) {
            return new Resolved(root.resolve(rel), Access.READ_ONLY, "native-worlds");
        }
        boolean settings = rel.getNameCount() == 3 && rel.getName(2).toString().equals("settings.toml");
        return new Resolved(root.resolve(rel), settings ? Access.READ_ONLY : Access.READ_WRITE, "native-world");
    }

    private Resolved inWorld(String name, World world, Path tail) {
        if (tail.toString().isEmpty()) {
            return new Resolved(world.folder(), Access.READ_WRITE, "world");
        }
        String first = tail.getName(0).toString();
        Path rest = tail(tail, 1);
        if (first.equals("generated")) {
            return new Resolved(root.resolve(tail), Access.READ_WRITE, "generated");
        }
        if (WORLD_STATE.contains(first)) {
            return new Resolved(shadow.resolve("worlds").resolve(name).resolve(tail), Access.READ_WRITE,
                "shadow-world-state");
        }
        String legacyEnvironment = DIMENSION_FOLDERS.get(first);
        if (legacyEnvironment != null) {
            World other = byEnvironment(legacyEnvironment);
            if (other == null) {
                return new Resolved(shadow.resolve("worlds").resolve(name).resolve(tail), Access.READ_WRITE,
                    "shadow-missing-dimension");
            }
            return new Resolved(join(other.folder(), rest), Access.READ_WRITE, "legacy-dimension");
        }
        if (first.equals("dimensions") && tail.getNameCount() >= 3 && tail.getName(1).toString().equals("minecraft")) {
            String environment = DIMENSION_KEYS.get(tail.getName(2).toString());
            if (environment != null) {
                World other = environment.equals(world.environment()) ? world : byEnvironment(environment);
                if (other != null) {
                    return new Resolved(join(other.folder(), tail(tail, 3)), Access.READ_WRITE, "dimension-folder");
                }
            }
        }
        return new Resolved(world.folder().resolve(tail), Access.READ_WRITE, "world");
    }

    private World byEnvironment(String environment) {
        for (World candidate : worlds.values()) {
            if (candidate.environment().equals(environment)) {
                return candidate;
            }
        }
        return null;
    }

    private Resolved shadowed(Path rel, String rule) {
        return new Resolved(shadow.resolve("root").resolve(rel), Access.READ_WRITE, rule);
    }

    private static Path join(Path base, Path rest) {
        return rest.toString().isEmpty() ? base : base.resolve(rest);
    }

    private static Path tail(Path path, int from) {
        return path.getNameCount() <= from ? Path.of("") : path.subpath(from, path.getNameCount());
    }

    private static Resolved denied(String rule) {
        return new Resolved(null, Access.DENIED, rule);
    }

    private boolean escapesThroughLink(Path real) {
        Path existing = real;
        while (existing != null && !Files.exists(existing, java.nio.file.LinkOption.NOFOLLOW_LINKS)) {
            existing = existing.getParent();
        }
        if (existing == null) {
            return true;
        }
        try {
            return !existing.toRealPath().startsWith(root.toRealPath());
        } catch (IOException failure) {
            return true;
        }
    }
}
