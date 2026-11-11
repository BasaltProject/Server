// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;
import java.nio.file.*;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.server.packs.*;
import net.minecraft.server.packs.linkfs.LinkFileSystem;
import net.minecraft.world.level.validation.ForbiddenSymlinkInfo;
import java.io.IOException;
import java.nio.file.Path;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import net.minecraft.server.packs.PackType;
import net.minecraft.world.level.validation.DirectoryValidator;
public class FolderRepositorySource implements RepositorySource {
    private static final PackSelectionConfig kDiscovered = new PackSelectionConfig(false, Pack.Position.TOP, false);
    private final Path folder;
    private final PackType packType;
    private final PackSource packSource;
    private final DirectoryValidator validator;

    public FolderRepositorySource(Path folder, PackType packType, PackSource packSource, DirectoryValidator validator) {
        this.folder = folder;
        this.packType = packType;
        this.packSource = packSource;
        this.validator = validator;
        
    }
    public void loadPacks(Consumer<Pack> result) {
        try {
            net.minecraft.util.FileUtil.createDirectoriesSafe(folder);
            discoverPacks(folder, validator, (content, resources) -> {
                String name = content.getFileName().toString();
                PackLocationInfo info = new PackLocationInfo("file/" + name, Component.literal(name), packSource, Optional.empty());
                Pack pack = Pack.readMetaAndCreate(info, resources, packType, kDiscovered);
                if (pack != null) {
                    result.accept(pack);
                }
            });
        } catch (IOException failure) {
            return;
        }
    }
    public static void discoverPacks(Path folder, DirectoryValidator validator, BiConsumer<Path, Pack.ResourcesSupplier> result) throws IOException {
        FolderPackDetector detector = new FolderPackDetector(validator);
        try (DirectoryStream<Path> entries = Files.newDirectoryStream(folder)) {
            for (Path entry : entries) {
                try {
                    List<ForbiddenSymlinkInfo> issues = new ArrayList<>();
                    Pack.ResourcesSupplier found = detector.detectPackResources(entry, issues);
                    if (issues.isEmpty() && found != null) {
                        result.accept(entry, found);
                    }
                } catch (IOException unreadable) {
                    continue;
                }
            }
        }
    }
    static class FolderPackDetector extends PackDetector<Pack.ResourcesSupplier> {
        protected FolderPackDetector(DirectoryValidator validator) {
            super(validator);
        }
        protected Pack. ResourcesSupplier createZipPack(Path content) {
            FileSystem system = content.getFileSystem();
            return system == FileSystems.getDefault() || system instanceof LinkFileSystem ? new FilePackResources.FileResourcesSupplier(content) : null;
        }
        protected Pack.ResourcesSupplier createDirectoryPack(Path content) {
            return new PathPackResources.PathResourcesSupplier(content);
        }
    }
}
