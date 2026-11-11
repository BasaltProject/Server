// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem.loader;
import com.mojang.datafixers.DataFixer;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.FileToIdConverter;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.resources.ResourceManager;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
public class ResourceManagerTemplateSource extends TemplateSource {
    private ResourceManager resourceManager;
    private final FileToIdConverter fileToIdConverter;
    public ResourceManagerTemplateSource(DataFixer fixerUpper, HolderGetter<Block> blockLookup, ResourceManager resourceManager, FileToIdConverter fileToIdConverter) {
        super(fixerUpper, blockLookup);
        this.resourceManager = resourceManager;
        this.fileToIdConverter = fileToIdConverter;
    }
    public void setResourceManager(ResourceManager resourceManager) {
        this.resourceManager = resourceManager;
    }
    public Optional<StructureTemplate> load(Identifier id) {
        Identifier file = fileToIdConverter.idToFile(id);
        return load(() -> resourceManager.open(file), false, failure -> { });
    }
    public Stream<Identifier> list() {
        return fileToIdConverter.listMatchingResources(resourceManager).keySet().stream().map(fileToIdConverter::fileToId);


    }
}
