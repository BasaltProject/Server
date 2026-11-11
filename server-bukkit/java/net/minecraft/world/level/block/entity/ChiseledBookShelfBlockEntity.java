// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import net.minecraft.tags.ItemTags;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.Level;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.ServerLevel;
import org.basaltmc.bukkit.facade.ValueIo;
import net.minecraft.core.BlockPos;
import net.minecraft.core.NonNullList;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.world.Container;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class ChiseledBookShelfBlockEntity extends BlockEntity implements ListBackedContainer {
    public ChiseledBookShelfBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }
    private static final String kItemsKey = "Items";
    private static final BooleanProperty[] kOccupied = {
        BlockStateProperties.SLOT_0_OCCUPIED, BlockStateProperties.SLOT_1_OCCUPIED, BlockStateProperties.SLOT_2_OCCUPIED,
        BlockStateProperties.SLOT_3_OCCUPIED, BlockStateProperties.SLOT_4_OCCUPIED, BlockStateProperties.SLOT_5_OCCUPIED};
    private void refreshState(int slot) {
        if (slot < 0 || slot >= MAX_BOOKS_IN_STORAGE) {
            return;
        }
        CompoundTag tag = nbt();
        tag.putInt("last_interacted_slot", slot);
        load(tag);
        BlockState updated = getBlockState();
        NonNullList<ItemStack> now = getItems();
        for (int i = 0; i < kOccupied.length; i++) {
            updated = updated.setValue(kOccupied[i], !now.get(i).isEmpty());
        }
        Level world = getLevel();
        if (world != null) {
            world.setBlockAndUpdate(getBlockPos(), updated);
            world.gameEvent(GameEvent.BLOCK_CHANGE, getBlockPos(), GameEvent.Context.of(updated));
        }
    }
    public static final int MAX_BOOKS_IN_STORAGE = 6;
    public ChiseledBookShelfBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("chiseled_bookshelf"), worldPosition, blockState);
    }
    protected void loadAdditional(ValueInput input) {
        load(ValueIo.read(input));
    }
    protected void saveAdditional(ValueOutput output) {
        ValueIo.write(output, saveWithoutMetadata((HolderLookup.Provider) null));
    }
    public int getMaxStackSize() {
        return 1;
    }
    public boolean acceptsItemType(ItemStack itemStack) {
        return itemStack.is(ItemTags.BOOKSHELF_BOOKS);
    }
    public ItemStack removeItem(int slot, int count) {
        ItemStack[] taken = {ItemStack.EMPTY};
        ItemListNbt.edit(this, kItemsKey, MAX_BOOKS_IN_STORAGE, items -> {
            taken[0] = items.get(slot);
            items.set(slot, ItemStack.EMPTY);
        });
        if (!taken[0].isEmpty()) {
            refreshState(slot);
        }
        return taken[0];
    }
    public void setItem(int slot, ItemStack itemStack) {
        if (acceptsItemType(itemStack)) {
            ItemListNbt.edit(this, kItemsKey, MAX_BOOKS_IN_STORAGE, items -> items.set(slot, itemStack));
            refreshState(slot);
        } else if (itemStack.isEmpty()) {
            removeItem(slot, getMaxStackSize());
        }
    }
    public boolean canTakeItem(Container into, int slot, ItemStack itemStack) {
        return into.hasAnyMatching(target -> target.isEmpty()
            || ItemStack.isSameItemSameComponents(itemStack, target) && target.getCount() + itemStack.getCount() <= into.getMaxStackSize(target));
    }
    public NonNullList<ItemStack> getItems() {
        return ItemListNbt.live(this, kItemsKey, MAX_BOOKS_IN_STORAGE);
    }
    public boolean stillValid(Player player) {
        return Container.stillValidBlockEntity(this, player);
    }
    public int getLastInteractedSlot() {
        return nbt().getIntOr("last_interacted_slot", -1);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        return;
    }
    protected void collectImplicitComponents(DataComponentMap.Builder components) {
        return;
    }
    public void removeComponentsFromTag(ValueOutput output) {
        output.discard("Items");


    }
}
