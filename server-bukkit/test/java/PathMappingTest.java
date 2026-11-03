import java.io.File;
import java.nio.file.Files;
import org.basaltmc.bukkit.fs.PathMapping;

/** Run on a VM with the path hook, from inside a scratch server directory. */
public final class PathMappingTest {
    private static void expect(boolean ok, String what) {
        if (!ok) {
            throw new AssertionError(what);
        }
        System.out.println("ok " + what);
    }

    public static void main(String[] args) throws Exception {
        File root = new File(".").getAbsoluteFile();
        new File(root, "worlds/main").mkdirs();
        new File(root, "plugins").mkdirs();
        expect(PathMapping.install(root), "hook installed");
        PathMapping.registerWorld("main", new File(root, "worlds/main"), "NORMAL");

        File level = new File(PathMapping.worldFolder("main", null), "level.dat");
        Files.writeString(level.toPath(), "x");
        expect(new File(root, ".bukkit-shadow/worlds/main/level.dat").isFile(), "level.dat lands in the shadow");
        expect(!new File(root, "worlds/main/level.dat").exists(), "level.dat never reaches the native world");

        Files.createDirectories(new File(root, "main/region").toPath());
        Files.writeString(new File(root, "main/region/r.0.0.mca").toPath(), "r");
        expect(new File(root, "worlds/main/region/r.0.0.mca").isFile(), "region reaches the native world");

        Files.writeString(new File("server.properties").toPath(), "a=b");
        expect(new File(root, ".bukkit-shadow/root/server.properties").isFile(), "root file goes to the shadow");
        expect(new File("server.properties").isFile(), "and the plugin reads it back");

        new File("plugins/Foo").mkdirs();
        Files.writeString(new File("plugins/Foo/config.yml").toPath(), "k: v");
        expect(new File(root, "plugins/Foo/config.yml").isFile(), "plugin data stays in plugins");

        boolean refused = false;
        try {
            Files.writeString(new File(root, "players.db").toPath(), "x");
        } catch (java.io.IOException expected) {
            refused = true;
        }
        expect(refused, "players.db is refused");
        expect(!new File("/etc/passwd").exists(), "outside the server nothing exists");
        expect(!new File(root, "../escape").mkdir(), "dotdot is refused");
        File temp = File.createTempFile("plug", ".tmp");
        expect(temp.getPath().contains(".bukkit-shadow"), "temp files live in the shadow");
        System.out.println("PathMappingTest done");
    }
}
