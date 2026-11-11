// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;
import net.minecraft.core.BlockPos;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.level.storage.loot.LootParams;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.parameters.LootContextParamSets;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
public interface RandomizableContainer extends Container {
    String LOOT_TABLE_TAG = "LootTable";
    String LOOT_TABLE_SEED_TAG = "LootTableSeed";
    ResourceKey<LootTable> getLootTable();
    void setLootTable(ResourceKey<LootTable> lootTable);
    default void setLootTable(ResourceKey<LootTable> lootTable, long seed) {
        setLootTable(lootTable);
        setLootTableSeed(seed);
    }
    long getLootTableSeed();
    void setLootTableSeed(long lootTableSeed);
    BlockPos getBlockPos();
    Level getLevel();
    static void setBlockEntityLootTable(BlockGetter level, RandomSource random, BlockPos blockEntityPos, ResourceKey<LootTable> lootTable) {
        if (level.getBlockEntity(blockEntityPos) instanceof RandomizableContainer container) {
            container.setLootTable(lootTable, random.nextLong());
        }
    }
    default boolean tryLoadLootTable(ValueInput base) {
        ResourceKey<LootTable> table = base.read(LOOT_TABLE_TAG, LootTable.KEY_CODEC).orElse(null);
        setLootTable(table);
        setLootTableSeed(base.getLongOr(LOOT_TABLE_SEED_TAG, 0L));
        return table != null;
    }
    default boolean trySaveLootTable(ValueOutput base) {
        ResourceKey<LootTable> table = getLootTable();
        if (table == null) {
            return false;
        }
        base.store(LOOT_TABLE_TAG, LootTable.KEY_CODEC, table);
        long seed = getLootTableSeed();
        if (seed != 0L) {
            base.putLong(LOOT_TABLE_SEED_TAG, seed);
        }
        return true;
    }
    default void unpackLootTable(Player player) {
        Level level = getLevel();
        ResourceKey<LootTable> key = getLootTable();
        if (key == null || level == null || level.getServer() == null) {
            return;
        }
        LootTable table = level.getServer().reloadableRegistries().getLootTable(key);
        setLootTable(null);
        LootParams.Builder params = new LootParams.Builder((ServerLevel) level).withParameter(LootContextParams.ORIGIN, Vec3.atCenterOf(getBlockPos()));
        if (player != null) {
            params.withLuck(player.getLuck()).withParameter(LootContextParams.THIS_ENTITY, player);
        }
        table.fill(this, params.create(LootContextParamSets.CHEST), getLootTableSeed());
    }
}
