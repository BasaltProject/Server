package org.basaltmc.bukkit.fs;

import java.io.File;
import java.nio.file.Path;
import java.util.function.BiFunction;
import org.basaltmc.bukkit.fs.PathTranslator.Access;
import org.basaltmc.bukkit.fs.PathTranslator.Resolved;

/** Installs the translator under every file access the VM makes, when the VM offers a hook for it. */
public final class PathMapping {
    private static final int WRITE = 1;
    private static final String[] READABLE = {"/dev/random", "/dev/urandom", "/dev/zero", "/etc/localtime",
        "/usr/share/zoneinfo", "/var/db/timezone", "/private/var/db/timezone"};

    private static volatile PathTranslator translator;

    private PathMapping() { }

    public static PathTranslator translator() {
        return translator;
    }

    /** Returns false when the VM has no path hook; the translator is then only used by the API. */
    public static boolean install(File serverRoot) {
        PathTranslator made = new PathTranslator(serverRoot.toPath());
        for (String location : READABLE) {
            made.allowOutside(Path.of(location), Access.READ_ONLY);
        }
        made.allowOutside(Path.of("/dev/null"), Access.READ_WRITE);
        for (String entry : System.getProperty("java.class.path", "").split(File.pathSeparator)) {
            if (!entry.isEmpty()) {
                Path parent = new File(entry).getAbsoluteFile().toPath().getParent();
                made.allowOutside(parent == null ? Path.of(entry) : parent, Access.READ_ONLY);
            }
        }
        String home = System.getProperty("java.home");
        if (home != null && !home.isEmpty()) {
            made.allowOutside(Path.of(home), Access.READ_ONLY);
        }
        translator = made;
        System.setProperty("java.io.tmpdir", made.shadow().resolve("tmp").toString());
        BiFunction<String, Integer, String> mapper = (path, operation) -> {
            Resolved resolved = made.resolve(Path.of(path));
            if (resolved.access() == Access.DENIED || (operation == WRITE && resolved.access() == Access.READ_ONLY)) {
                return null;
            }
            return resolved.real().toString();
        };
        try {
            Class.forName("shim.FileNative").getMethod("setPathMapper", BiFunction.class).invoke(null, mapper);
            return true;
        } catch (ReflectiveOperationException | LinkageError missing) {
            return false;
        }
    }

    public static void registerWorld(String name, File folder, String environment) {
        PathTranslator active = translator;
        if (active != null) {
            active.world(name, folder.toPath(), environment);
        }
    }

    /** The folder as the plugin should see it: where a Paper server would keep the world. */
    public static File worldFolder(String name, File real) {
        PathTranslator active = translator;
        return active == null ? real : active.root().resolve(name).toFile();
    }
}
