// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import java.util.function.Consumer;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.dedicated.DedicatedServer;
import net.minecraft.util.StringUtil;
import org.bukkit.BanList;
import org.bukkit.Bukkit;
public class OldUsersConverter {
    public static final File OLD_IPBANLIST = new File("banned-ips.txt");
    public static final File OLD_USERBANLIST = new File("banned-players.txt");
    public static final File OLD_OPLIST = new File("ops.txt");
    public static final File OLD_WHITELIST = new File("white-list.txt");
    private static final List<File> kOldFiles = List.of(OLD_USERBANLIST, OLD_IPBANLIST, OLD_OPLIST, OLD_WHITELIST);
    private static boolean present(File file) {
        return file.exists() && file.isFile();
    }
    private static boolean convert(File file, Consumer<String[]> entry) {
        if (!present(file)) {
            return true;
        }
        try {
            for (String line : Files.readAllLines(file.toPath(), StandardCharsets.UTF_8)) {
                String trimmed = line.trim();
                if (!trimmed.isEmpty() && !trimmed.startsWith("#")) {
                    entry.accept(trimmed.split("\\|"));
                }
            }
            return file.renameTo(new File(file.getName() + ".converted"));
        } catch (IOException | RuntimeException failure) {
            return false;
        }
    }
    private static Date date(String[] fields, int index) {
        try {
            return fields.length > index ? new SimpleDateFormat("yyyy-MM-dd HH:mm:ss Z", Locale.ROOT).parse(fields[index]) : null;
        } catch (ParseException ignored) {
            return null;
        }
    }
    private static void ban(BanList.Type type, String[] fields) {
        Bukkit.getBanList(type).addBan(fields[0], fields.length > 4 ? fields[4] : null, date(fields, 3), fields.length > 2 ? fields[2] : null);
    }
    public static boolean convertUserBanlist(MinecraftServer server) {
        return convert(OLD_USERBANLIST, fields -> ban(BanList.Type.PROFILE, fields));
    }
    public static boolean convertIpBanlist(MinecraftServer server) {
        return convert(OLD_IPBANLIST, fields -> ban(BanList.Type.IP, fields));
    }
    public static boolean convertOpsList(MinecraftServer server) {
        return convert(OLD_OPLIST, fields -> Bukkit.getOfflinePlayer(fields[0]).setOp(true));
    }
    public static boolean convertWhiteList(MinecraftServer server) {
        return convert(OLD_WHITELIST, fields -> Bukkit.getOfflinePlayer(fields[0]).setWhitelisted(true));
    }
    public static UUID convertMobOwnerIfNecessary(MinecraftServer server, String owner) {
        if (!StringUtil.isNullOrEmpty(owner) && owner.length() <= 16) {
            return Bukkit.getOfflinePlayer(owner).getUniqueId();
        }
        try {
            return UUID.fromString(owner);
        } catch (IllegalArgumentException | NullPointerException ignored) {
            return null;
        }
    }
    public static boolean convertPlayers(DedicatedServer server) {
        return true;
    }
    public static boolean areOldUserlistsRemoved() {
        return kOldFiles.stream().noneMatch(OldUsersConverter::present);
    }
    static class ConversionError extends RuntimeException {
        private ConversionError(String message, Throwable cause) {
            super(message, cause);
        }
        private ConversionError(String message) {
            super(message);
        }
    }
}
