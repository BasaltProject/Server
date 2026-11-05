// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class MinecartFurnace extends AbstractMinecart {
    public Vec3 push;
    public MinecartFurnace(EntityType<? extends MinecartFurnace> type, Level level) {
        super(type, level);
    }
    public boolean isFurnace() {
        return false;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void tick() {
    }
    protected double getMaxSpeed(ServerLevel level) {
        return 0;
    }
    protected Item getDropItem() {
        return net.minecraft.world.item.Items.FURNACE_MINECART;
    }
    public ItemStack getPickResult() {
        return new ItemStack(net.minecraft.world.item.Items.FURNACE_MINECART);
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean addFuel(Vec3 interactingPos, ItemStack itemStack) {
        return itemStack.is(net.minecraft.tags.ItemTags.FURNACE_MINECART_FUEL)
            && org.basaltmc.bukkit.EntityNatives.call(level().getWorld().getName(), getId(), "cart_add_fuel", new Object[] {interactingPos.x, interactingPos.z})
                instanceof Boolean added && added;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected boolean hasFuel() {
        return false;
    }
    protected void setHasFuel(boolean fuel) {
        }
}
