// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.PackOutput;
import net.minecraft.world.level.levelgen.presets.WorldPreset;

public class WorldPresetTagsProvider extends TagsProvider<WorldPreset> {
    public WorldPresetTagsProvider(PackOutput output, CompletableFuture<HolderLookup.Provider> lookupProvider) {
        super(output, Registries.WORLD_PRESET, lookupProvider);
    }

    @Override
    protected void addTags(HolderLookup.Provider registries) {
        addNativeTags();
    }
}
