// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.commands.CommandSource;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BaseCommandBlock;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class MinecartCommandBlock extends AbstractMinecart {
    public MinecartCommandBlock(EntityType<? extends MinecartCommandBlock> type, Level level) {
        super(type, level);
    }
    protected Item getDropItem() {
        return net.minecraft.world.item.Items.COMMAND_BLOCK_MINECART;
    }
    public ItemStack getPickResult() {
        return new ItemStack(net.minecraft.world.item.Items.COMMAND_BLOCK_MINECART);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
        commandBlock.load(input);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
        commandBlock.save(output);
    }
    private final BaseCommandBlock commandBlock = new MinecartCommandBase();
    public BaseCommandBlock getCommandBlock() {
        return commandBlock;
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    class MinecartCommandBase extends BaseCommandBlock {
        private MinecartCommandBase() {
            
        }
        public void onUpdated(ServerLevel level) {
            
        }
        public CommandSourceStack createCommandSourceStack(ServerLevel level, CommandSource source) {
            return new CommandSourceStack(source, position(), getRotationVector(), level, net.minecraft.server.permissions.PermissionSet.ALL_PERMISSIONS, level.getServer(), MinecartCommandBlock.this);
        }
        public boolean isValid() {
            return !isRemoved();
        }
    }
}
