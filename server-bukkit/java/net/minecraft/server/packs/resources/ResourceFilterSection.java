// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import net.minecraft.server.packs.metadata.MetadataSectionType;
import net.minecraft.util.IdentifierPattern;
public class ResourceFilterSection {
    public static final MetadataSectionType<ResourceFilterSection> TYPE = new MetadataSectionType<>("filter", RecordCodecBuilder.<ResourceFilterSection>create(i -> i.group(
        Codec.list(IdentifierPattern.CODEC).fieldOf("block").forGetter(section -> section.blockList)).apply(i, ResourceFilterSection::new)));
    private final List<IdentifierPattern> blockList;
    public ResourceFilterSection(List<IdentifierPattern> blockList) {
        this.blockList = List.copyOf(blockList);
        
    }
    public boolean isNamespaceFiltered(String namespace) {
        return blockList.stream().anyMatch(pattern -> pattern.namespacePredicate().test(namespace));
    }
    public boolean isPathFiltered(String path) {
        return blockList.stream().anyMatch(pattern -> pattern.pathPredicate().test(path));
    }
}
