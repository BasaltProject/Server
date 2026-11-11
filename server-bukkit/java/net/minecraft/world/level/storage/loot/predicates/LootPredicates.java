// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.advancements.predicates.DataComponentMatchers;
import net.minecraft.advancements.predicates.EnchantmentPredicate;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.BlockPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.component.predicates.DataComponentPredicates;
import net.minecraft.core.component.predicates.EnchantmentsPredicate;
import net.minecraft.core.HolderGetter;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.enchantment.Enchantments;
import net.minecraft.world.level.block.Blocks;
import java.util.List;
import net.minecraft.world.level.block.Block;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class LootPredicates {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> HolderGetter<T> getter(BootstrapContext<?> context, ResourceKey key) {
        return (HolderGetter<T>) context.lookup(key);
    }

    private static ResourceKey<LootItemCondition> createKey(String name) {
        return ResourceKey.create(Registries.PREDICATE, net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    public static final ResourceKey<LootItemCondition> TOOL_CAN_SILK_TOUCH = createKey("tool/can_silk_touch");
    public static final ResourceKey<LootItemCondition> TOOL_CAN_SHEAR = createKey("tool/can_shear");
    public static final ResourceKey<LootItemCondition> FAST_FURNACE = createKey("block/fast_cooking");
    public static void bootstrap(BootstrapContext<LootItemCondition> context) {
        HolderGetter<Enchantment> enchantments = getter(context, Registries.ENCHANTMENT);
        HolderGetter<Item> items = getter(context, Registries.ITEM);
        HolderGetter<Block> blocks = getter(context, Registries.BLOCK);
        EnchantmentPredicate silk = new EnchantmentPredicate(enchantments.getOrThrow(Enchantments.SILK_TOUCH), MinMaxBounds.Ints.atLeast(1));
        DataComponentMatchers withSilk = DataComponentMatchers.Builder.components()
            .partial(DataComponentPredicates.ENCHANTMENTS, EnchantmentsPredicate.enchantments(List.of(silk))).build();
        context.register(TOOL_CAN_SILK_TOUCH, MatchTool.toolMatches(ItemPredicate.Builder.item().withComponents(withSilk)).build());
        context.register(TOOL_CAN_SHEAR, MatchTool.toolMatches(ItemPredicate.Builder.item().of(items, Items.SHEARS)).build());
        context.register(FAST_FURNACE, MatchBlock.blockMatches(BlockPredicate.Builder.block().of(blocks, Blocks.SMOKER, Blocks.BLAST_FURNACE)).build());
    }
}
