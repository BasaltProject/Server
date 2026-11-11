// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.validation;
import java.nio.file.Path;
import java.util.List;
public class ContentValidationException extends Exception {
    private final Path directory;
    private final List<ForbiddenSymlinkInfo> entries;
    public ContentValidationException(Path directory, List<ForbiddenSymlinkInfo> entries) {
        this.directory = directory;
        this.entries = entries;
    }
    @Override
    public String getMessage() {
        return getMessage(directory, entries);
    }
    public static String getMessage(Path directory, List<ForbiddenSymlinkInfo> entries) {
        String links = entries.stream().map(entry -> entry.link() + "->" + entry.target()).collect(java.util.stream.Collectors.joining(", "));
        return "Failed to validate '" + directory + "'. Found forbidden symlinks: " + links;
    }
}
