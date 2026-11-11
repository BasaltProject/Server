// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;
import java.util.Optional;
import net.minecraft.SharedConstants;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.*;
import net.minecraft.server.packs.resources.ResourceMetadata;
import net.minecraft.server.packs.metadata.pack.PackMetadataSection;
import net.minecraft.world.flag.FeatureFlags;
import net.minecraft.world.level.validation.DirectoryValidator;
import net.minecraft.world.level.storage.LevelResource;
import java.nio.file.Path;
import net.minecraft.network.chat.Component;
import net.minecraft.server.packs.VanillaPackResources;
import net.minecraft.world.level.storage.LevelStorageSource;
import net.minecraft.world.level.validation.DirectoryValidator;
public class ServerPacksSource extends BuiltInPackSource {
    private static final ResourceMetadata kBuiltInMetadata = ResourceMetadata.of(PackMetadataSection.SERVER_TYPE,
        new PackMetadataSection(Component.translatable("dataPack.vanilla.description"), SharedConstants.getCurrentVersion().packVersion(PackType.SERVER_DATA).minorRange()),
        FeatureFlagsMetadataSection.TYPE, new FeatureFlagsMetadataSection(FeatureFlags.DEFAULT_FLAGS));
    private static final PackLocationInfo kVanillaInfo = new PackLocationInfo("vanilla", Component.translatable("dataPack.vanilla.name"), PackSource.BUILT_IN, Optional.of(CORE_PACK_INFO));

    public ServerPacksSource(DirectoryValidator validator) {
        super(PackType.SERVER_DATA, createVanillaPackSource(), Identifier.withDefaultNamespace("datapacks"), validator);
    }
    public static VanillaPackResources createVanillaPackSource() {
        return new VanillaPackResourcesBuilder().setMetadata(kBuiltInMetadata).exposeNamespace("minecraft").applyDevelopmentConfig().pushJarResources().build(kVanillaInfo);
    }
    protected Component getPackTitle(String id) {
        return Component.literal(id);
    }
    protected Pack createVanillaPack(Pack.ResourcesSupplier resources) {
        return Pack.readMetaAndCreate(kVanillaInfo, resources, PackType.SERVER_DATA, new PackSelectionConfig(false, Pack.Position.BOTTOM, false));
    }
    protected Pack createBuiltinPack(String id, Pack.ResourcesSupplier resources, Component name) {
        PackLocationInfo info = new PackLocationInfo(id, name, PackSource.FEATURE, Optional.of(KnownPack.vanilla(id)));
        return Pack.readMetaAndCreate(info, resources, PackType.SERVER_DATA, new PackSelectionConfig(false, Pack.Position.TOP, false));
    }
    public static PackRepository createPackRepository(Path datapackDir, DirectoryValidator validator) {
        return new PackRepository(new RepositorySource[] {new ServerPacksSource(validator),
            new FolderRepositorySource(datapackDir, PackType.SERVER_DATA, PackSource.WORLD, validator)});
    }
    public static PackRepository createVanillaTrustedRepository() {
        return new PackRepository(new RepositorySource[] {new ServerPacksSource(new DirectoryValidator(path -> true))});
    }
    public static PackRepository createPackRepository(LevelStorageSource.LevelStorageAccess levelSourceAccess) {
        return createPackRepository(levelSourceAccess.getLevelPath(LevelResource.DATAPACK_DIR), levelSourceAccess.parent().getWorldDirValidator());
    }
}
