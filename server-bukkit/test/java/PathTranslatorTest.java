import java.nio.file.Files;
import java.nio.file.Path;
import org.basaltmc.bukkit.fs.PathTranslator;
import org.basaltmc.bukkit.fs.PathTranslator.Access;
import org.basaltmc.bukkit.fs.PathTranslator.Resolved;

public final class PathTranslatorTest {
    private static Path root;
    private static PathTranslator t;

    private static void is(String what, String path, Access access, String real) {
        Resolved r = t.resolve(Path.of(path));
        String got = r.real() == null ? null : root.relativize(r.real()).toString();
        if (r.access() != access || (real != null && !real.equals(got))) {
            throw new AssertionError(what + ": " + path + " -> " + r.access() + " " + got + " (" + r.rule() + ")");
        }
    }

    public static void main(String[] args) throws Exception {
        root = Files.createTempDirectory("basalt-paths").toRealPath();
        Files.createDirectories(root.resolve("worlds/main"));
        Files.createDirectories(root.resolve("plugins"));
        t = new PathTranslator(root);
        t.world("main", root.resolve("worlds/main"), "NORMAL");
        t.world("nether", root.resolve("worlds/nether"), "NETHER");
        String out = root.getParent().toString();

        is("world dir", "main", Access.READ_WRITE, "worlds/main");
        is("region", "main/region/r.0.0.mca", Access.READ_WRITE, "worlds/main/region/r.0.0.mca");
        is("datapacks", "main/datapacks/a", Access.READ_WRITE, "worlds/main/datapacks/a");
        is("level.dat", "main/level.dat", Access.READ_WRITE, ".basalt/bukkit/shadow/worlds/main/level.dat");
        is("playerdata", "main/playerdata/x.dat", Access.READ_WRITE, ".basalt/bukkit/shadow/worlds/main/playerdata/x.dat");
        is("DIM-1", "main/DIM-1/region", Access.READ_WRITE, "worlds/nether/region");
        is("DIM1 missing", "main/DIM1/region", Access.READ_WRITE, ".basalt/bukkit/shadow/worlds/main/DIM1/region");
        is("dimension folder", "main/dimensions/minecraft/the_nether/region", Access.READ_WRITE,
            "worlds/nether/region");
        is("dimension self", "main/dimensions/minecraft/overworld/region", Access.READ_WRITE, "worlds/main/region");
        is("generated", "generated/minecraft/structure/a.nbt", Access.READ_WRITE, "generated/minecraft/structure/a.nbt");
        is("world generated", "main/generated/minecraft/structure/a.nbt", Access.READ_WRITE,
            "generated/minecraft/structure/a.nbt");
        is("shadow idempotent", ".basalt/bukkit/shadow/root/x", Access.READ_WRITE, ".basalt/bukkit/shadow/root/x");
        t.allowOutside(Path.of("/dev/null"), Access.READ_WRITE);
        is("allowed outside", "/dev/null", Access.READ_WRITE, null);
        is("plugin data", "plugins/Foo/config.yml", Access.READ_WRITE, "plugins/Foo/config.yml");
        is("root file", "myfile.txt", Access.READ_WRITE, ".basalt/bukkit/shadow/root/myfile.txt");
        is("server.properties", "server.properties", Access.READ_WRITE, ".basalt/bukkit/shadow/root/server.properties");
        is("bukkit.yml", "bukkit.yml", Access.READ_WRITE, ".basalt/bukkit/shadow/root/bukkit.yml");
        is("ops.json", "ops.json", Access.READ_WRITE, ".basalt/bukkit/shadow/root/ops.json");
        is("paper config", "config/paper-global.yml", Access.READ_WRITE, ".basalt/bukkit/shadow/root/config/paper-global.yml");
        is("logs", "logs/latest.log", Access.READ_ONLY, "logs/latest.log");
        is("native settings", "worlds/main/settings.toml", Access.READ_ONLY, null);
        is("native world", "worlds/main/region/x", Access.READ_WRITE, "worlds/main/region/x");
        is("absolute", root.resolve("plugins/Foo/a").toString(), Access.READ_WRITE, "plugins/Foo/a");
        is("players.db", "players.db", Access.DENIED, null);
        is("PLAYERS.DB", "Players.DB", Access.DENIED, null);
        is("eula", "eula.toml", Access.DENIED, null);
        is("dotdot", "../x", Access.DENIED, null);
        is("dotdot deep", "main/../../x", Access.DENIED, null);
        is("absolute outside", out + "/other", Access.DENIED, null);
        is("etc", "/etc/passwd", Access.DENIED, null);

        Files.createSymbolicLink(root.resolve("plugins/escape"), root.getParent());
        is("symlink escape", "plugins/escape/x", Access.DENIED, null);
        System.out.println("PathTranslatorTest ok");
    }
}
