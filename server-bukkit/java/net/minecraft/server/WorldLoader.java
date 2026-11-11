// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import com.mojang.datafixers.util.Pair;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import net.minecraft.commands.Commands;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.core.RegistryAccess;
import net.minecraft.server.packs.PackType;
import net.minecraft.server.packs.repository.PackRepository;
import net.minecraft.server.packs.resources.CloseableResourceManager;
import net.minecraft.server.packs.resources.MultiPackResourceManager;
import net.minecraft.server.packs.resources.ResourceManager;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.world.level.WorldDataConfiguration;

public class WorldLoader {
    public static <D, R> CompletableFuture<R> load(InitConfig config, WorldDataSupplier<D> worldDataSupplier, ResultFactory<D, R> resultFactory, Executor backgroundExecutor, Executor mainThreadExecutor) {
        Pair<WorldDataConfiguration, CloseableResourceManager> opened = config.packConfig().createResourceManager();
        CloseableResourceManager resources = opened.getSecond();
        LayeredRegistryAccess<RegistryLayer> layers = RegistryLayer.createRegistryAccess();
        RegistryAccess.Frozen visible = layers.compositeAccess();
        DataLoadOutput<D> output = worldDataSupplier.get(new DataLoadContext(resources, opened.getFirst(), visible, visible));
        LayeredRegistryAccess<RegistryLayer> withDimensions = layers.replaceFrom(RegistryLayer.DIMENSIONS, output.finalDimensions());
        return ReloadableServerResources.loadResources(resources, withDimensions, java.util.List.of(), opened.getFirst().enabledFeatures(), config.commandSelection(), config.functionCompilationPermissions(), backgroundExecutor, mainThreadExecutor)
            .whenComplete((managers, failure) -> {
                if (failure != null) {
                    resources.close();
                }
            })
            .thenApplyAsync(managers -> {
                managers.updateComponentsAndStaticRegistryTags();
                return resultFactory.create(resources, managers, withDimensions, output.cookie());
            }, mainThreadExecutor);
    }

    public static record DataLoadContext(ResourceManager resources, WorldDataConfiguration dataConfiguration, HolderLookup.Provider datapackWorldRegistries, RegistryAccess.Frozen datapackDimensions) {}

    public static record DataLoadOutput<D>(D cookie, RegistryAccess.Frozen finalDimensions) {}

    public static record PackConfig(PackRepository packRepository, WorldDataConfiguration initialDataConfig, boolean safeMode, boolean initMode) {
        public Pair<WorldDataConfiguration, CloseableResourceManager> createResourceManager() {
            WorldDataConfiguration selected = MinecraftServer.configurePackRepository(packRepository, initialDataConfig, initMode, safeMode);
            return Pair.of(selected, new MultiPackResourceManager(PackType.SERVER_DATA, packRepository.openAllSelected()));
        }
    }

    public static record InitConfig(PackConfig packConfig, Commands.CommandSelection commandSelection, PermissionSet functionCompilationPermissions) {}

    public interface ResultFactory<D, R> {
        R create(CloseableResourceManager resources, ReloadableServerResources managers, LayeredRegistryAccess<RegistryLayer> registries, D cookie);
    }

    public interface WorldDataSupplier<D> {
        DataLoadOutput<D> get(DataLoadContext context);
    }
}
