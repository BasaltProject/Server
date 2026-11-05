// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.validation;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.PathMatcher;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.Stream;

public class DirectoryValidator {
    private final PathMatcher symlinkTargetAllowList;

    public DirectoryValidator(PathMatcher symlinkTargetAllowList) {
        this.symlinkTargetAllowList = symlinkTargetAllowList;
    }

    public void validateSymlink(Path path, List<ForbiddenSymlinkInfo> issues) throws IOException {
        Path target = Files.readSymbolicLink(path);
        if (!symlinkTargetAllowList.matches(target)) {
            issues.add(new ForbiddenSymlinkInfo(path, target));
        }
    }

    public List<ForbiddenSymlinkInfo> validateSymlink(Path path) throws IOException {
        List<ForbiddenSymlinkInfo> issues = new ArrayList<>();
        validateSymlink(path, issues);
        return issues;
    }

    public List<ForbiddenSymlinkInfo> validateDirectory(Path directory, boolean allowTopSymlink) throws IOException {
        List<ForbiddenSymlinkInfo> issues = new ArrayList<>();
        if (Files.isSymbolicLink(directory)) {
            if (!allowTopSymlink) {
                validateSymlink(directory, issues);
                return issues;
            }
            directory = directory.toRealPath();
        }
        validateKnownDirectory(directory, issues);
        return issues;
    }

    public void validateKnownDirectory(Path directory, List<ForbiddenSymlinkInfo> issues) throws IOException {
        try (Stream<Path> walk = Files.walk(directory)) {
            for (Path entry : (Iterable<Path>) walk::iterator) {
                if (Files.isSymbolicLink(entry)) {
                    validateSymlink(entry, issues);
                }
            }
        }
    }
}
