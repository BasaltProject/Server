// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.io.IOException;
import java.nio.file.Path;
import java.util.List;
import net.minecraft.world.level.validation.DirectoryValidator;
import net.minecraft.world.level.validation.ForbiddenSymlinkInfo;
public abstract class PackDetector<T> {
    private final DirectoryValidator validator;

    protected PackDetector(DirectoryValidator validator) {
        this.validator = validator;
        
    }
    public T detectPackResources(Path content, List<ForbiddenSymlinkInfo> issues) throws IOException {
        BasicFileAttributes attributes;
        try {
            attributes = Files.readAttributes(content, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
        } catch (NoSuchFileException missing) {
            return null;
        }
        Path target = content;
        if (attributes.isSymbolicLink()) {
            validator.validateSymlink(content, issues);
            if (!issues.isEmpty()) {
                return null;
            }
            target = Files.readSymbolicLink(content);
            attributes = Files.readAttributes(target, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
        }
        if (attributes.isDirectory()) {
            validator.validateKnownDirectory(target, issues);
            if (!issues.isEmpty() || !Files.isRegularFile(target.resolve("pack.mcmeta"))) {
                return null;
            }
            return createDirectoryPack(target);
        }
        return attributes.isRegularFile() && target.getFileName().toString().endsWith(".zip") ? createZipPack(target) : null;
    }
    protected abstract T createZipPack(Path content) throws IOException;
    protected abstract T createDirectoryPack(Path content) throws IOException;
}
