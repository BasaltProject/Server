// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.PackOutput;
import net.minecraft.world.level.levelgen.structure.Structure;

public class StructureTagsProvider extends TagsProvider<Structure> {
    public StructureTagsProvider(PackOutput output, CompletableFuture<HolderLookup.Provider> lookupProvider) {
        super(output, (net.minecraft.resources.ResourceKey) Registries.STRUCTURE, lookupProvider);
    }

    @Override
    protected void addTags(HolderLookup.Provider registries) {
        addNativeTags();
    }
}
