// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem.loader;
import java.nio.file.Path;
import net.minecraft.IdentifierException;
import net.minecraft.resources.FileToIdConverter;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.PackType;
public class TemplatePathFactory {
    private final Path sourceDir;
    public TemplatePathFactory(Path sourceDir, PackType packType) {
        this(sourceDir.resolve(packType == PackType.CLIENT_RESOURCES ? "assets" : "data"));
    }
    public TemplatePathFactory(Path sourceDir) {
        this.sourceDir = sourceDir;
    }
    public Path createAndValidatePathToStructure(Identifier id, FileToIdConverter converter) {
        return createAndValidatePathToStructure(converter.idToFile(id));
    }
    public Path createAndValidatePathToStructure(Identifier resourceLocation) {
        Path path = sourceDir.resolve(resourceLocation.getNamespace());
        for (String part : resourceLocation.getPath().split("/", -1)) {
            if (part.isEmpty() || part.equals(".") || part.equals("..") || !part.matches("[a-z0-9_.-]+")) {
                throw new IdentifierException("Resource path '" + resourceLocation + "' is not portable");
            }
            path = path.resolve(part);
        }
        return path;
    }
}
