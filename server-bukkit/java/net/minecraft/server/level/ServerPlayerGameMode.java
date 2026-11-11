// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.network.protocol.game.ServerboundPlayerActionPacket;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;

public class ServerPlayerGameMode {
    protected ServerLevel level;
    protected final ServerPlayer player;
    private GameType previous;

    public ServerPlayerGameMode(ServerPlayer player) {
        this.player = player;
        try {
            this.level = player.level();
        } catch (UnsupportedOperationException notBound) {
            this.level = null;
        }
    }

    public boolean changeGameModeForPlayer(GameType gameModeForPlayer) {
        GameType before = getGameModeForPlayer();
        if (before == gameModeForPlayer) {
            return false;
        }
        previous = before;
        return player.setGameMode(gameModeForPlayer);
    }

    protected void setGameModeForPlayer(GameType gameModeForPlayer, GameType previousGameModeForPlayer) {
        previous = previousGameModeForPlayer;
        player.setGameMode(gameModeForPlayer);
    }

    public GameType getGameModeForPlayer() {
        return player.gameMode();
    }

    public GameType getPreviousGameModeForPlayer() {
        return previous;
    }

    public boolean isSurvival() {
        return getGameModeForPlayer().isSurvival();
    }

    public boolean isCreative() {
        return getGameModeForPlayer().isCreative();
    }

    public void tick() {}

    public void handleBlockBreakAction(BlockPos pos, ServerboundPlayerActionPacket.Action action, Direction direction, int maxY, int sequence) {
        if (action == ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK && isCreative()) {
            destroyAndAck(pos, sequence, "creative destroy");
        }
    }

    public void destroyAndAck(BlockPos pos, int sequence, String exitId) {
        destroyBlock(pos);
    }

    public boolean destroyBlock(BlockPos pos) {
        return player.bukkitPlayer().breakBlock(player.bukkitPlayer().getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()));
    }

    public InteractionResult useItem(ServerPlayer player, Level level, ItemStack itemStack, InteractionHand hand) {
        return InteractionResult.PASS;
    }

    public InteractionResult useItemOn(ServerPlayer player, Level level, ItemStack itemStack, InteractionHand hand, BlockHitResult hitResult) {
        return InteractionResult.PASS;
    }

    public void setLevel(ServerLevel newLevel) {
        this.level = newLevel;
    }
}
