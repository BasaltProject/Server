// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.advancements.packs;
import java.util.List;
import java.util.stream.Stream;
import net.minecraft.advancements.Advancement;
import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.data.advancements.AdvancementSubProvider;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.Item;
public class VanillaHusbandryAdvancements extends AdvancementSubProvider {
    public static final List<EntityType<?>> BREEDABLE_ANIMALS = List.of(EntityTypes.HORSE, EntityTypes.DONKEY, EntityTypes.MULE,
        EntityTypes.SHEEP, EntityTypes.COW, EntityTypes.MOOSHROOM, EntityTypes.PIG, EntityTypes.CHICKEN, EntityTypes.WOLF,
        EntityTypes.OCELOT, EntityTypes.RABBIT, EntityTypes.LLAMA, EntityTypes.CAT, EntityTypes.PANDA, EntityTypes.FOX,
        EntityTypes.BEE, EntityTypes.HOGLIN, EntityTypes.STRIDER, EntityTypes.GOAT, EntityTypes.AXOLOTL, EntityTypes.CAMEL,
        EntityTypes.ARMADILLO, EntityTypes.NAUTILUS);
    public static final List<EntityType<?>> INDIRECTLY_BREEDABLE_ANIMALS = List.of(EntityTypes.TURTLE, EntityTypes.FROG, EntityTypes.SNIFFER);
    public static final Item[] WAX_SCRAPING_TOOLS = {Items.WOODEN_AXE, Items.GOLDEN_AXE, Items.STONE_AXE,
        Items.COPPER_AXE, Items.IRON_AXE, Items.DIAMOND_AXE, Items.NETHERITE_AXE};
    public VanillaHusbandryAdvancements(BootstrapContext<Advancement> output) {
        super((net.minecraft.data.worldgen.BootstrapContext) null);
    }
    public void generate() {
    }
    public AdvancementHolder createBreedAllAnimalsAdvancement(AdvancementHolder parent, Stream<EntityType<?>> breedable, Stream<EntityType<?>> indirectlyBreedable) {
        return null;
    }
}
