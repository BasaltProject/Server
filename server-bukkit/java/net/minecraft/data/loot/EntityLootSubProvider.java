// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot;
import net.minecraft.advancements.predicates.DamageSourcePredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.animal.frog.FrogVariant;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.level.block.ColorCollection;
import net.minecraft.world.level.storage.loot.LootPool;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.predicates.AnyOfCondition;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public abstract class EntityLootSubProvider implements LootTableSubProvider {
    protected final LootTableSubProvider.Context output = null;
    protected final HolderGetter<Item> items = null;
    protected final HolderGetter<Enchantment> enchantments = null;
    protected final HolderGetter<EntityType<?>> entityTypes = null;
    protected final HolderGetter<FrogVariant> frogVariants = null;
    protected final HolderGetter<DamageType> damageTypes = null;
    protected final HolderGetter<LootTable> lootTables = null;
    protected EntityLootSubProvider(FeatureFlagSet enabledFeatures, LootTableSubProvider.Context output) {
        
    }
    protected EntityLootSubProvider(FeatureFlagSet allowed, FeatureFlagSet required, LootTableSubProvider.Context output) {
        
    }
    protected DamageSourcePredicate.Builder projectileDamage() {
        return null;
    }
    protected final AnyOfCondition.Builder shouldSmeltLoot() {
        return null;
    }
    public static LootPool.Builder createSheepDispatchPool(ColorCollection<Holder<LootTable>> tableNames) {
        return LootPool.lootPool();
    }
    public abstract void generate();
    public void run() {
    }
    protected LootItemCondition.Builder killedByFrog() {
        return null;
    }
    protected LootItemCondition.Builder killedByFrogVariant(ResourceKey<FrogVariant> variant) {
        return null;
    }
    protected void add(EntityType<?> type, LootTable.Builder builder) {
    }
    protected void add(EntityType<?> type, ResourceKey<LootTable> lootTable, LootTable.Builder builder) {
    }
}
