// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.core.NonNullList;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.RandomizableContainer;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.loot.LootTable;

public abstract class RandomizableContainerBlockEntity extends BaseContainerBlockEntity implements RandomizableContainer {
    protected ResourceKey<LootTable> lootTable;
    protected long lootTableSeed;

    protected RandomizableContainerBlockEntity(BlockEntityType<?> type, BlockPos worldPosition, BlockState blockState) {
        super(type, worldPosition, blockState);
    }

    protected RandomizableContainerBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    @Override
    @SuppressWarnings({"unchecked", "rawtypes"})
    public ResourceKey<LootTable> getLootTable() {
        String id = nbt().getStringOr("LootTable", "");
        return id.isEmpty() ? null : (ResourceKey) ResourceKey.create(
            (ResourceKey) net.minecraft.core.registries.Registries.LOOT_TABLE, Identifier.parse(id));
    }

    @Override
    public void setLootTable(ResourceKey<LootTable> lootTable) {
        CompoundTag tag = nbt();
        if (lootTable == null) {
            tag.remove("LootTable");
        } else {
            tag.putString("LootTable", lootTable.identifier().toString());
        }
        load(tag);
    }

    @Override
    public long getLootTableSeed() {
        return nbt().getLongOr("LootTableSeed", 0L);
    }

    @Override
    public void setLootTableSeed(long lootTableSeed) {
        CompoundTag tag = nbt();
        tag.putLong("LootTableSeed", lootTableSeed);
        load(tag);
    }

    @Override
    protected NonNullList<ItemStack> getItems() {
        return readItems(getContainerSize());
    }

    @Override
    protected void setItems(NonNullList<ItemStack> items) {
        writeItems(items);
    }
}
