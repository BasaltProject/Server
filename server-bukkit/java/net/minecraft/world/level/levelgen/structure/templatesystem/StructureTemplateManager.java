// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;

import com.mojang.datafixers.DataFixer;
import java.io.IOException;
import java.nio.file.Path;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.FileToIdConverter;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.resources.ResourceManager;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.levelgen.structure.templatesystem.loader.TemplatePathFactory;
import net.minecraft.world.level.storage.LevelStorageSource;
public class StructureTemplateManager {
    private TemplatePathFactory worldTemplates;
    protected StructureTemplateManager() {
    }
    public static final String STRUCTURE_DIRECTORY_NAME = "structure";
    public static final FileToIdConverter WORLD_STRUCTURE_LISTER = null;
    public static final FileToIdConverter RESOURCE_TEXT_STRUCTURE_LISTER = null;
    public StructureTemplateManager(ResourceManager resourceManager, LevelStorageSource.LevelStorageAccess storage, DataFixer fixerUpper, HolderGetter<Block> blockLookup) {
        worldTemplates = new TemplatePathFactory(storage == null
            ? net.minecraft.server.MinecraftServer.getServer().getWorldPath(net.minecraft.world.level.storage.LevelResource.GENERATED_DIR)
            : storage.getLevelPath(net.minecraft.world.level.storage.LevelResource.GENERATED_DIR).normalize());
    }
    public StructureTemplate getOrCreate(Identifier id) {
        return get(id).orElseGet(StructureTemplate::new);
    }
    public Optional<StructureTemplate> get(Identifier id) {
        byte[] bytes = org.basaltmc.bukkit.LeftoverNatives.structureRead(id.toString());
        if (bytes == null) {
            return Optional.empty();
        }
        StructureTemplate template = new StructureTemplate();
        template.load(null, org.basaltmc.bukkit.facade.NbtCodec.fromBytes(bytes));
        return Optional.of(template);
    }
    public Stream<Identifier> listTemplates() {
        String[] ids = org.basaltmc.bukkit.LeftoverNatives.structureIds();
        return ids == null ? Stream.empty() : java.util.Arrays.stream(ids).map(Identifier::parse);
    }
    public void onResourceManagerReload(ResourceManager resourceManager) {

    }
    public boolean save(Identifier id) {
        return get(id).map(template -> org.basaltmc.bukkit.LeftoverNatives.structureStore(id.toString(),
            org.basaltmc.bukkit.facade.NbtCodec.toBytes(template.save(new net.minecraft.nbt.CompoundTag())), true)).orElse(false);
    }
    public static boolean save(Path file, StructureTemplate structureTemplate, boolean asText) throws IOException {
        java.nio.file.Files.createDirectories(file.getParent());
        java.nio.file.Files.write(file, org.basaltmc.bukkit.facade.NbtCodec.toBytes(structureTemplate.save(new net.minecraft.nbt.CompoundTag())));
        return true;
    }
    public TemplatePathFactory worldTemplates() {
        return worldTemplates;
    }
    public TemplatePathFactory testTemplates() {
        return null;
    }
    public void remove(Identifier id) {
        org.basaltmc.bukkit.LeftoverNatives.structureRemove(id.toString(), false);
    }
}
