// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot;
import java.util.Set;
import java.util.function.Function;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.entries.LootPoolEntryContainer;
import net.minecraft.world.level.storage.loot.functions.FunctionUserBuilder;
import net.minecraft.world.level.storage.loot.predicates.ConditionUserBuilder;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public abstract class BlockLootSubProvider implements LootTableSubProvider {
    protected final LootTableSubProvider.Context output = null;
    protected final HolderGetter<Enchantment> enchantments = null;
    protected final HolderGetter<Item> items = null;
    protected final HolderGetter<Block> blocks = null;
    protected final HolderGetter<LootItemCondition> predicates = null;
    protected static final float[] NORMAL_LEAVES_SAPLING_CHANCES = {0.05F, 0.0625F, 0.083333336F, 0.1F};
    protected BlockLootSubProvider(Set<Item> explosionResistant, FeatureFlagSet enabledFeatures, LootTableSubProvider.Context output) {
        
    }
    protected Holder<LootItemCondition> hasSilkTouch() {
        return null;
    }
    protected LootItemCondition.Builder doesNotHaveSilkTouch() {
        return null;
    }
    protected Holder<LootItemCondition> hasShears() {
        return null;
    }
    protected <T extends FunctionUserBuilder<T>> T applyExplosionDecay(ItemLike type, FunctionUserBuilder<T> builder) {
        return (T) builder;
    }
    protected <T extends ConditionUserBuilder<T>> T applyExplosionCondition(ItemLike type, ConditionUserBuilder<T> builder) {
        return (T) builder;
    }
    public LootTable.Builder createSingleItemTable(ItemLike drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createSilkTouchDispatchTable(Block original, LootPoolEntryContainer.Builder<?> entry) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createShearsDispatchTable(Block original, LootPoolEntryContainer.Builder<?> entry) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createSilkTouchOrShearsDispatchTable(Block original, LootPoolEntryContainer.Builder<?> entry) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createSingleItemTableWithSilkTouch(Block original, ItemLike drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createSingleItemTable(ItemLike drop, Holder<ContextIntProvider> count) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createSingleItemTableWithSilkTouch(Block original, ItemLike drop, Holder<ContextIntProvider> count) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createSlabItemTable(Block slab) {
        return LootTable.lootTable();
    }
    protected <T extends Comparable<T> & StringRepresentable> LootTable.Builder createSinglePropConditionTable(Block drop, Property<T> property, T value) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createNameableBlockEntityTable(Block drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createShulkerBoxDrop(Block shulkerBox) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createCopperOreDrops(Block block) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createLapisOreDrops(Block block) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createRedstoneOreDrops(Block block) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createBannerDrop(Block original) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createBeeNestDrop(Block original) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createBeeHiveDrop(Block original) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createCaveVinesDrop(Block original) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createCopperGolemStatueBlock(Block block) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createOreDrop(Block original, Item drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createMushroomBlockDrop(Block original, ItemLike drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createGrassDrops(Block original) {
        return LootTable.lootTable();
    }
    public LootTable.Builder createStemDrops(Block block, Item drop) {
        return LootTable.lootTable();
    }
    public LootTable.Builder createAttachedStemDrops(Block block, Item drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createShearsOnlyDrop(ItemLike drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createShearsOrSilkTouchOnlyDrop(ItemLike drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createMultifaceBlockDrops(Block block, Holder<LootItemCondition> condition) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createMultifaceBlockDrops(Block block) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createMossyCarpetBlockDrops(Block block) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createLeavesDrops(Block original, Block sapling, float... saplingChances) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createOakLeavesDrops(Block original, Block sapling, float... saplingChances) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createMangroveLeavesDrops(Block block) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createCropDrops(Block original, Item cropDrop, Item seedDrop, LootItemCondition.Builder isMaxAge) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createDoublePlantShearsDrop(Block block) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createDoublePlantWithSeedDrops(Block block, Block drop) {
        return LootTable.lootTable();
    }
    protected LootTable.Builder createCandleDrops(Block block) {
        return LootTable.lootTable();
    }
    public LootTable.Builder createSegmentedBlockDrops(Block block) {
        return LootTable.lootTable();
    }
    protected static LootTable.Builder createCandleCakeDrops(Block candle) {
        return LootTable.lootTable();
    }
    public static LootTable.Builder noDrop() {
        return LootTable.lootTable();
    }
    protected abstract void generate();
    public void run() {
    }
    protected void addNetherVinesDropTable(Block vineBlock, Block plantBlock) {
    }
    protected LootTable.Builder createDoorTable(Block block) {
        return LootTable.lootTable();
    }
    protected void dropPottedContents(Block potted) {
    }
    protected void otherWhenSilkTouch(Block block, Block other) {
    }
    protected void dropOther(Block block, ItemLike drop) {
    }
    protected void dropWhenSilkTouch(Block block) {
    }
    protected void dropSelf(Block block) {
    }
    protected void add(Block block, Function<Block, LootTable.Builder> builder) {
    }
    protected void add(Block block, LootTable.Builder builder) {
    }
}
