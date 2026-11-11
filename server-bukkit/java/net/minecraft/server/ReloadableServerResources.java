// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.Commands;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.core.Registry;
import net.minecraft.core.component.DataComponentInitializers;
import net.minecraft.server.packs.resources.PreparableReloadListener;
import net.minecraft.server.packs.resources.ResourceManager;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.crafting.RecipeManager;

public class ReloadableServerResources {
    private final ReloadableServerRegistries.Holder fullRegistries;
    private final Commands commands;
    private final RecipeManager recipes;
    private final ServerAdvancementManager advancements;
    private final ServerFunctionLibrary functionLibrary;

    private ReloadableServerResources(ReloadableServerRegistries.LoadResult loadingContext, FeatureFlagSet enabledFeatures, Commands.CommandSelection commandSelection, List<Registry.PendingTags<?>> postponedTags, PermissionSet functionCompilationPermissions, List<DataComponentInitializers.PendingComponents<?>> newComponents) {
        this.fullRegistries = new ReloadableServerRegistries.Holder(loadingContext.lookupWithUpdatedTags());
        this.commands = new Commands(commandSelection, CommandBuildContext.simple(loadingContext.lookupWithUpdatedTags(), enabledFeatures));
        this.recipes = new RecipeManager(loadingContext.lookupWithUpdatedTags());
        this.advancements = new ServerAdvancementManager(loadingContext.lookupWithUpdatedTags());
        this.functionLibrary = new ServerFunctionLibrary(functionCompilationPermissions, commands.getDispatcher());
    }

    public ServerFunctionLibrary getFunctionLibrary() {
        return functionLibrary;
    }

    public ReloadableServerRegistries.Holder fullRegistries() {
        return fullRegistries;
    }

    public RecipeManager getRecipeManager() {
        return recipes;
    }

    public Commands getCommands() {
        return commands;
    }

    public ServerAdvancementManager getAdvancements() {
        return advancements;
    }

    public List<PreparableReloadListener> listeners() {
        return List.of(functionLibrary);
    }

    public static CompletableFuture<ReloadableServerResources> loadResources(ResourceManager resourceManager, LayeredRegistryAccess<RegistryLayer> contextLayers, List<Registry.PendingTags<?>> updatedContextTags, FeatureFlagSet enabledFeatures, Commands.CommandSelection commandSelection, PermissionSet functionCompilationPermissions, Executor backgroundExecutor, Executor mainThreadExecutor) {
        return ReloadableServerRegistries.reload(contextLayers, updatedContextTags, resourceManager, backgroundExecutor)
            .thenApplyAsync(loaded -> new ReloadableServerResources(loaded, enabledFeatures, commandSelection, updatedContextTags, functionCompilationPermissions, List.of()), mainThreadExecutor);
    }

    public void updateComponentsAndStaticRegistryTags() {}
}
