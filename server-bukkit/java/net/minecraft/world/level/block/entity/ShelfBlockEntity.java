// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.Container;
import net.minecraft.world.level.block.ShelfBlock;
import org.basaltmc.bukkit.facade.ValueIo;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.NonNullList;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.protocol.game.ClientboundBlockEntityDataPacket;
import net.minecraft.world.entity.ItemOwner;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class ShelfBlockEntity extends BlockEntity implements ListBackedContainer, ItemOwner {
    private static final String kItemsKey = "Items";
    public ShelfBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }
    public static final int MAX_ITEMS = 3;
    public ShelfBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("shelf"), worldPosition, blockState);
    }
    protected void loadAdditional(ValueInput input) {
        load(ValueIo.read(input));
    }
    protected void saveAdditional(ValueOutput output) {
        ValueIo.write(output, saveWithoutMetadata((HolderLookup.Provider) null));
    }
    public ClientboundBlockEntityDataPacket getUpdatePacket() {
        return ClientboundBlockEntityDataPacket.create(this);
    }
    public CompoundTag getUpdateTag(HolderLookup.Provider registries) {
        return saveWithoutMetadata(registries);
    }
    public NonNullList<ItemStack> getItems() {
        return ItemListNbt.live(this, kItemsKey, MAX_ITEMS);
    }
    public boolean stillValid(Player player) {
        return Container.stillValidBlockEntity(this, player);
    }
    public ItemStack swapItemNoUpdate(int slot, ItemStack heldItemStack) {
        ItemStack[] taken = {ItemStack.EMPTY};
        ItemListNbt.edit(this, kItemsKey, MAX_ITEMS, items -> {
            taken[0] = items.get(slot);
            items.set(slot, heldItemStack);
        });
        return taken[0];
    }
    public void setChanged(Holder. Reference<GameEvent> event) {
        super.setChanged();
        if (getLevel() != null && event != null) {
            getLevel().gameEvent(event, getBlockPos(), GameEvent.Context.of(getBlockState()));
        }
    }
    public void setChanged() {
        setChanged(GameEvent.BLOCK_ACTIVATE);
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
    public Level level() {
        return getLevel();
    }
    public Vec3 position() {
        return Vec3.atCenterOf(getBlockPos());
    }
    public float getVisualRotationYInDegrees() {
        return getBlockState().getValue(ShelfBlock.FACING).getOpposite().toYRot();
    }
    public boolean getAlignItemsToBottom() {
        return nbt().getBooleanOr("align_items_to_bottom", false);


    }
}
