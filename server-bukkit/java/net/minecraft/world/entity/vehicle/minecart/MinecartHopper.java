// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.entity.Hopper;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class MinecartHopper extends AbstractMinecartContainer implements Hopper {
    public MinecartHopper(EntityType<? extends MinecartHopper> type, Level level) {
        super(type, level);
    }
    public BlockState getDefaultDisplayBlockState() {
        return net.minecraft.world.level.block.Blocks.HOPPER.defaultBlockState();
    }
    public int getDefaultDisplayOffset() {
        return 1;
    }
    public int getContainerSize() {
        return 5;
    }
    public void activateMinecart(ServerLevel level, int xt, int yt, int zt, boolean state) {
        setEnabled(!state);
    }
    public boolean isEnabled() {
        return live() instanceof org.bukkit.entity.minecart.HopperMinecart api && api.isEnabled();
    }
    public void setEnabled(boolean enabled) {
        if (live() instanceof org.bukkit.entity.minecart.HopperMinecart api) { api.setEnabled(enabled); }
    }
    public double getLevelX() {
        return getX();
    }
    public double getLevelY() {
        return getY() + 0.5;
    }
    public double getLevelZ() {
        return getZ();
    }
    public boolean isGridAligned() {
        return false;
    }
    public void tick() {
    }
    protected double makeStepAlongTrack(BlockPos pos, RailShape shape, double movementLeft) {
        return 0;
    }
    public boolean suckInItems() {
        return false;
    }
    protected Item getDropItem() {
        return net.minecraft.world.item.Items.HOPPER_MINECART;
    }
    public ItemStack getPickResult() {
        return new ItemStack(net.minecraft.world.item.Items.HOPPER_MINECART);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return new net.minecraft.world.inventory.HopperMenu(containerId, inventory, this);
    }
}
