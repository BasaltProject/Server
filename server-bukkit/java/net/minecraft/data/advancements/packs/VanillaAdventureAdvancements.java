// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.advancements.packs;
import java.util.List;
import net.minecraft.advancements.Advancement;
import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.core.HolderGetter;
import net.minecraft.data.advancements.AdvancementSubProvider;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
public class VanillaAdventureAdvancements extends AdvancementSubProvider {
    public VanillaAdventureAdvancements(BootstrapContext<Advancement> output) {
        super((net.minecraft.data.worldgen.BootstrapContext) null);
    }
    public void generate() {
    }
    public AdvancementHolder createMonsterHunterAdvancement(AdvancementHolder parent, List<EntityType<?>> mobsToKill) {
        return null;
    }
    protected void createAdventuringTime(AdvancementHolder sleepInBed, MultiNoiseBiomeSourceParameterList.Preset preset) {
    }
    public static Advancement.Builder addBiomes(Advancement.Builder advancement, HolderGetter<Biome> biomes, List<ResourceKey<Biome>> explorableBiomes) {
        return advancement;
    }
}
