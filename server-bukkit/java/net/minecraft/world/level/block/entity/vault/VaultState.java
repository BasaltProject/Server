// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.vault;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.phys.Vec3;
import net.minecraft.core.Direction;
import net.minecraft.core.dispenser.DefaultDispenseItemBehavior;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.StringRepresentable;
public enum VaultState implements StringRepresentable {
    INACTIVE, ACTIVE, UNLOCKING, EJECTING;
    private static final int kRecheckTicks = 20;
    private static void eject(ServerLevel level, BlockPos pos, ItemStack stack, float progress) {
        DefaultDispenseItemBehavior.spawnItem(level, stack, 2, Direction.UP, Vec3.atBottomCenterOf(pos).relative(Direction.UP, 1.2));
        level.levelEvent(3017, pos, 0);
        level.playSound((Entity) null, pos, SoundEvents.VAULT_EJECT_ITEM, SoundSource.BLOCKS, 1.0F, 0.8F + 0.4F * progress);
    }
    private static VaultState settle(ServerLevel level, BlockPos pos, VaultConfig config, VaultServerData serverData, VaultSharedData sharedData, double range) {
        sharedData.updateConnectedPlayersWithinRange(level, pos, serverData, config, range);
        serverData.pauseStateUpdatingUntil(level.getGameTime() + kRecheckTicks);
        return sharedData.hasConnectedPlayers() ? ACTIVE : INACTIVE;
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public int lightLevel() {
        return this == INACTIVE ? 6 : 12;
    }
    public VaultState tickAndGetNext(ServerLevel serverLevel, BlockPos pos, VaultConfig config, VaultServerData serverData, VaultSharedData sharedData) {
        switch (this) {
            case INACTIVE:
                return settle(serverLevel, pos, config, serverData, sharedData, config.activationRange());
            case ACTIVE:
                return settle(serverLevel, pos, config, serverData, sharedData, config.deactivationRange());
            case UNLOCKING:
                serverData.pauseStateUpdatingUntil(serverLevel.getGameTime() + kRecheckTicks);
                return EJECTING;
            default:
                break;
        }
        if (serverData.getItemsToEject().isEmpty()) {
            serverData.markEjectionFinished();
            return settle(serverLevel, pos, config, serverData, sharedData, config.deactivationRange());
        }
        eject(serverLevel, pos, serverData.popNextItemToEject(), serverData.ejectionProgress());
        sharedData.setDisplayItem(serverData.getNextItemToEject());
        serverData.pauseStateUpdatingUntil(serverLevel.getGameTime() + kRecheckTicks);
        return EJECTING;
    }
    public void onTransition(ServerLevel serverLevel, BlockPos pos, VaultState to, VaultConfig config, VaultSharedData sharedData, boolean isOminous) {
        onExit(serverLevel, pos, config, sharedData);
        to.onEnter(serverLevel, pos, config, sharedData, isOminous);
    }
    protected void onEnter(ServerLevel serverLevel, BlockPos pos, VaultConfig config, VaultSharedData sharedData, boolean isOminous) {
        int flag = isOminous ? 1 : 0;
        switch (this) {
            case INACTIVE -> {
                sharedData.setDisplayItem(ItemStack.EMPTY);
                serverLevel.levelEvent(3016, pos, flag);
            }
            case ACTIVE -> serverLevel.levelEvent(3015, pos, flag);
            case UNLOCKING -> serverLevel.playSound((Entity) null, pos, SoundEvents.VAULT_INSERT_ITEM, SoundSource.BLOCKS, 1.0F, 1.0F);
            case EJECTING -> serverLevel.playSound((Entity) null, pos, SoundEvents.VAULT_OPEN_SHUTTER, SoundSource.BLOCKS, 1.0F, 1.0F);
        }
    }
    protected void onExit(ServerLevel serverLevel, BlockPos pos, VaultConfig config, VaultSharedData sharedData) {
        if (this == EJECTING) {
            serverLevel.playSound((Entity) null, pos, SoundEvents.VAULT_CLOSE_SHUTTER, SoundSource.BLOCKS, 1.0F, 1.0F);
        }
    }
    static enum LightLevel {
        HALF_LIT, LIT;
    }
}
