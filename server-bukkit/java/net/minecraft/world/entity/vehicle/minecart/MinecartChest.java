// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.ContainerUser;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;
public class MinecartChest extends AbstractMinecartContainer {
    public MinecartChest(EntityType<? extends MinecartChest> type, Level level) {
        super(type, level);
    }
    protected Item getDropItem() {
        return net.minecraft.world.item.Items.CHEST_MINECART;
    }
    public ItemStack getPickResult() {
        return new ItemStack(net.minecraft.world.item.Items.CHEST_MINECART);
    }
    public int getContainerSize() {
        return 27;
    }
    public BlockState getDefaultDisplayBlockState() {
        return net.minecraft.world.level.block.Blocks.CHEST.defaultBlockState();
    }
    public int getDefaultDisplayOffset() {
        return 8;
    }
    public AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return net.minecraft.world.inventory.ChestMenu.threeRows(containerId, inventory, this);
    }
    public void stopOpen(ContainerUser containerUser) {
        level().gameEvent(net.minecraft.world.level.gameevent.GameEvent.CONTAINER_CLOSE, position(), net.minecraft.world.level.gameevent.GameEvent.Context.of(this));
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
}
