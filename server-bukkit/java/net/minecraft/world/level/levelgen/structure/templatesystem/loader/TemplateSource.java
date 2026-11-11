// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem.loader;
import net.minecraft.nbt.CompoundTag;
import com.mojang.datafixers.DataFixer;
import java.io.InputStream;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.stream.Stream;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.resources.IoSupplier;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
public abstract class TemplateSource {
    private final DataFixer fixerUpper;
    private final HolderGetter<Block> blockLookup;
    private StructureTemplate readTemplate(CompoundTag tag) {
        StructureTemplate template = new StructureTemplate();
        int version = net.minecraft.nbt.NbtUtils.getDataVersion(tag, 500);
        template.load(blockLookup, net.minecraft.util.datafix.DataFixTypes.STRUCTURE.updateToCurrentVersion(fixerUpper, tag, version));
        return template;
    }
    protected TemplateSource(DataFixer fixerUpper, HolderGetter<Block> blockLookup) {
        this.fixerUpper = fixerUpper;
        this.blockLookup = blockLookup;
    }
    public abstract Optional<StructureTemplate> load(Identifier id);
    public abstract Stream<Identifier> list();
    protected Optional<StructureTemplate> load(IoSupplier<InputStream> opener, boolean asText, Consumer<Throwable> onError) {
        try (InputStream raw = opener.get(); InputStream input = new java.io.BufferedInputStream(raw)) {
            CompoundTag tag = asText
                ? net.minecraft.nbt.NbtUtils.snbtToStructure(new String(input.readAllBytes(), java.nio.charset.StandardCharsets.UTF_8))
                : net.minecraft.nbt.NbtIo.readCompressed(input, net.minecraft.nbt.NbtAccounter.unlimitedHeap());
            return Optional.of(readTemplate(tag));
        } catch (java.io.FileNotFoundException missing) {
            return Optional.empty();
        } catch (Throwable failure) {
            onError.accept(failure);
            return Optional.empty();
        }
    }
}
