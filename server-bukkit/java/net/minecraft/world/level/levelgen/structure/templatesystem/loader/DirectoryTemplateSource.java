// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem.loader;
import com.mojang.datafixers.DataFixer;
import java.nio.file.Path;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.FileToIdConverter;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.PackType;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
public class DirectoryTemplateSource extends TemplateSource {
    private final Path sourceDir;
    private final FileToIdConverter fileToIdConverter;
    private final boolean loadAsText;
    public DirectoryTemplateSource(DataFixer fixerUpper, HolderGetter<Block> blockLookup, Path sourceDir, PackType packType, FileToIdConverter fileToIdConverter, boolean loadAsText) {
        this(fixerUpper, blockLookup, sourceDir.resolve(packType.getDirectory()), fileToIdConverter, loadAsText);
    }
    public DirectoryTemplateSource(DataFixer fixerUpper, HolderGetter<Block> blockLookup, Path sourceDir, FileToIdConverter fileToIdConverter, boolean loadAsText) {
        super(fixerUpper, blockLookup);
        this.sourceDir = sourceDir;
        this.fileToIdConverter = fileToIdConverter;
        this.loadAsText = loadAsText;
    }
    public Optional<StructureTemplate> load(Identifier id) {
        if (!java.nio.file.Files.isDirectory(sourceDir)) {
            return Optional.empty();
        }
        net.minecraft.server.packs.resources.IoSupplier<java.io.InputStream> resource =
            net.minecraft.server.packs.PathPackResources.getResource(sourceDir, fileToIdConverter.idToFile(id));
        return resource == null ? Optional.empty() : load(resource, loadAsText, failure -> { });
    }
    public Stream<Identifier> list() {
        if (!java.nio.file.Files.isDirectory(sourceDir)) {
            return Stream.empty();
        }
        Stream.Builder<Identifier> found = Stream.builder();
        for (String namespace : net.minecraft.server.packs.PathPackResources.getNamespaces(sourceDir)) {
            net.minecraft.server.packs.PathPackResources.listResources(sourceDir, namespace, fileToIdConverter.prefix(), (id, opener) -> {
                if (fileToIdConverter.extensionMatches(id)) {
                    found.accept(fileToIdConverter.fileToId(id));
                }
            });
        }
        return found.build();
    }
}
